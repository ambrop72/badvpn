/**
 * @file PasswordListener.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * This file is part of BadVPN.
 * 
 * BadVPN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * BadVPN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>

#include <openssl/rand.h>

#include <prerror.h>

#include <ssl.h>

#include <misc/debug.h>
#include <misc/offset.h>
#include <misc/byteorder.h>
#include <nspr_support/DummyPRFileDesc.h>
#include <nspr_support/BSocketPRFileDesc.h>

#include <client/PasswordListener.h>

static int password_comparator (void *user, uint64_t *p1, uint64_t *p2);
static void cleanup_client (PasswordListener *l, struct PasswordListenerClient *client);
static void listener_handler (PasswordListener *l);
static void client_try_read (struct PasswordListenerClient *client);
static void client_read_handler (struct PasswordListenerClient *client, int event);
static void client_read_handler_ssl (struct PasswordListenerClient *client, PRInt16 event);

int password_comparator (void *user, uint64_t *p1, uint64_t *p2)
{
    if (*p1 < *p2) {
        return -1;
    }
    if (*p1 > *p2) {
        return 1;
    }
    return 0;
}

void cleanup_client (PasswordListener *l, struct PasswordListenerClient *client)
{
    if (l->ssl) {
        BPRFileDesc_Free(&client->sock->ssl_bprfd);
        ASSERT_FORCE(PR_Close(client->sock->ssl_prfd) == PR_SUCCESS)
    }
    BSocket_Free(&client->sock->sock);
    free(client->sock);
}

void listener_handler (PasswordListener *l)
{
    // grab client entry
    LinkedList2Node *node;
    struct PasswordListenerClient *client;
    if (node = LinkedList2_GetFirst(&l->clients_free)) {
        client = UPPER_OBJECT(node, struct PasswordListenerClient, list_node);
        LinkedList2_Remove(&l->clients_free, &client->list_node);
    } else {
        node = LinkedList2_GetFirst(&l->clients_used);
        ASSERT(node)
        client = UPPER_OBJECT(node, struct PasswordListenerClient, list_node);
        cleanup_client(l, client);
        LinkedList2_Remove(&l->clients_used, &client->list_node);
    }
    
    if (!(client->sock = malloc(sizeof(sslsocket)))) {
        DEBUG("cannot allocate sslsocket");
        goto fail0;
    }
    
    // accept a client
    if (!Listener_Accept(&l->listener, &client->sock->sock, NULL)) {
        DEBUG("Listener_Accept failed");
        goto fail1;
    }
    
    DEBUG("Connection accepted");
    
    if (l->ssl) {
        // create BSocket NSPR file descriptor
        BSocketPRFileDesc_Create(&client->sock->bottom_prfd, &client->sock->sock);
        
        // create SSL file descriptor from the socket's BSocketPRFileDesc
        if (!(client->sock->ssl_prfd = SSL_ImportFD(l->model_prfd, &client->sock->bottom_prfd))) {
            ASSERT_FORCE(PR_Close(&client->sock->bottom_prfd) == PR_SUCCESS)
            goto fail2;
        }
        
        // set server mode
        if (SSL_ResetHandshake(client->sock->ssl_prfd, PR_TRUE) != SECSuccess) {
            DEBUG("SSL_ResetHandshake failed");
            goto fail3;
        }
        
        // set require client certificate
        if (SSL_OptionSet(client->sock->ssl_prfd, SSL_REQUEST_CERTIFICATE, PR_TRUE) != SECSuccess) {
            DEBUG("SSL_OptionSet(SSL_REQUEST_CERTIFICATE) failed");
            goto fail3;
        }
        if (SSL_OptionSet(client->sock->ssl_prfd, SSL_REQUIRE_CERTIFICATE, PR_TRUE) != SECSuccess) {
            DEBUG("SSL_OptionSet(SSL_REQUIRE_CERTIFICATE) failed");
            goto fail3;
        }
        
        // initialize BPRFileDesc on SSL file descriptor
        BPRFileDesc_Init(&client->sock->ssl_bprfd, client->sock->ssl_prfd);
        
        // set read handler
        BPRFileDesc_AddEventHandler(&client->sock->ssl_bprfd, PR_POLL_READ, (BPRFileDesc_handler)client_read_handler_ssl, client);
    } else {
        // set read handler
        BSocket_AddEventHandler(&client->sock->sock, BSOCKET_READ, (BSocket_handler)client_read_handler, client);
    }
    
    // init buffer
    client->recv_buffer_pos = 0;
    
    // add to used list
    LinkedList2_Append(&l->clients_used, &client->list_node);
    
    // start receiving password
    // NOTE: listener and connection can die
    client_try_read(client);
    return;
    
    // cleanup on error
fail3:
    if (l->ssl) {
        ASSERT_FORCE(PR_Close(client->sock->ssl_prfd) == PR_SUCCESS)
    }
fail2:
    BSocket_Free(&client->sock->sock);
fail1:
    free(client->sock);
fail0:
    LinkedList2_Append(&l->clients_free, &client->list_node);
}

void client_try_read (struct PasswordListenerClient *client)
{
    PasswordListener *l = client->l;
    
    if (l->ssl) {
        while (client->recv_buffer_pos < sizeof(client->recv_buffer)) {
            PRInt32 recvd = PR_Read(
                client->sock->ssl_prfd,
                (uint8_t *)&client->recv_buffer + client->recv_buffer_pos,
                sizeof(client->recv_buffer) - client->recv_buffer_pos
            );
            if (recvd < 0) {
                PRErrorCode error = PR_GetError();
                if (error == PR_WOULD_BLOCK_ERROR) {
                    BPRFileDesc_EnableEvent(&client->sock->ssl_bprfd, PR_POLL_READ);
                    return;
                }
                DEBUG("PR_Read failed (%d)", (int)error);
                goto free_client;
            }
            if (recvd == 0) {
                DEBUG("Connection terminated");
                goto free_client;
            }
            client->recv_buffer_pos += recvd;
        }
    } else {
        while (client->recv_buffer_pos < sizeof(client->recv_buffer)) {
            int recvd = BSocket_Recv(
                &client->sock->sock,
                (uint8_t *)&client->recv_buffer + client->recv_buffer_pos,
                sizeof(client->recv_buffer) - client->recv_buffer_pos
            );
            if (recvd < 0) {
                int error = BSocket_GetError(&client->sock->sock);
                if (error == BSOCKET_ERROR_LATER) {
                    BSocket_EnableEvent(&client->sock->sock, BSOCKET_READ);
                    return;
                }
                DEBUG("BSocket_Recv failed (%d)", error);
                goto free_client;
            }
            if (recvd == 0) {
                DEBUG("Connection terminated");
                goto free_client;
            }
            client->recv_buffer_pos += recvd;
        }
    }
    
    // check password
    uint64_t received_pass = ltoh64(client->recv_buffer);
    BAVLNode *pw_tree_node = BAVL_LookupExact(&l->passwords, &received_pass);
    if (!pw_tree_node) {
        DEBUG("WARNING: unknown password");
        goto free_client;
    }
    PasswordListener_pwentry *pw_entry = UPPER_OBJECT(pw_tree_node, PasswordListener_pwentry, tree_node);
    
    DEBUG("Password recognized");
    
    // remove password entry
    BAVL_Remove(&l->passwords, &pw_entry->tree_node);
    
    // move client entry to free list
    LinkedList2_Remove(&l->clients_used, &client->list_node);
    LinkedList2_Append(&l->clients_free, &client->list_node);
    
    if (l->ssl) {
        // remove event handler
        BPRFileDesc_RemoveEventHandler(&client->sock->ssl_bprfd, PR_POLL_READ);
    } else {
        // remove event handler
        BSocket_RemoveEventHandler(&client->sock->sock, BSOCKET_READ);
    }
    
    // give the socket to the handler
    // NOTE: listener can die
    pw_entry->handler_client(pw_entry->user, client->sock);
    return;
    
free_client:
    cleanup_client(l, client);
    LinkedList2_Remove(&l->clients_used, &client->list_node);
    LinkedList2_Append(&l->clients_free, &client->list_node);
}

void client_read_handler (struct PasswordListenerClient *client, int event)
{
    ASSERT(event == BSOCKET_READ)
    BSocket_DisableEvent(&client->sock->sock, BSOCKET_READ);
    
    // NOTE: listener and connection can die
    client_try_read(client);
}

void client_read_handler_ssl (struct PasswordListenerClient *client, PRInt16 event)
{
    ASSERT(event == PR_POLL_READ)
    
    // NOTE: listener and connection can die
    client_try_read(client);
}

int PasswordListener_Init (PasswordListener *l, BReactor *bsys, BAddr listen_addr, int max_clients, int ssl, CERTCertificate *cert, SECKEYPrivateKey *key)
{
    ASSERT(!BAddr_IsInvalid(&listen_addr))
    ASSERT(max_clients > 0)
    ASSERT(ssl == 0 || ssl == 1)
    
    l->bsys = bsys;
    l->ssl = ssl;
    
    // allocate client entries
    if (!(l->clients_data = malloc(max_clients * sizeof(struct PasswordListenerClient)))) {
        goto fail0;
    }
    
    if (l->ssl) {
        // initialize model SSL fd
        DummyPRFileDesc_Create(&l->model_dprfd);
        if (!(l->model_prfd = SSL_ImportFD(NULL, &l->model_dprfd))) {
            DEBUG("SSL_ImportFD failed");
            ASSERT_FORCE(PR_Close(&l->model_dprfd) == PR_SUCCESS)
            goto fail1;
        }
        
        // set server certificate
        if (SSL_ConfigSecureServer(l->model_prfd, cert, key, NSS_FindCertKEAType(cert)) != SECSuccess) {
            DEBUG("SSL_ConfigSecureServer failed");
            goto fail2;
        }
    }
    
    // initialize client entries
    LinkedList2_Init(&l->clients_free);
    LinkedList2_Init(&l->clients_used);
    int i;
    for (i = 0; i < max_clients; i++) {
        struct PasswordListenerClient *conn = &l->clients_data[i];
        conn->l = l;
        LinkedList2_Append(&l->clients_free, &conn->list_node);
    }
    
    // initialize passwords tree
    BAVL_Init(&l->passwords, OFFSET_DIFF(PasswordListener_pwentry, password, tree_node), (BAVL_comparator)password_comparator, NULL);
    
    // initialize listener
    if (!Listener_Init(&l->listener, l->bsys, listen_addr, (Listener_handler)listener_handler, l)) {
        DEBUG("Listener_Init failed");
        goto fail2;
    }
    
    // initialize dead variable
    DEAD_INIT(l->dead);
    
    // init debug object
    DebugObject_Init(&l->d_obj);
    
    return 1;
    
    // cleanup
fail2:
    if (l->ssl) {
        ASSERT_FORCE(PR_Close(l->model_prfd) == PR_SUCCESS)
    }
fail1:
    free(l->clients_data);
fail0:
    return 0;
}

void PasswordListener_Free (PasswordListener *l)
{
    // free debug object
    DebugObject_Free(&l->d_obj);

    // free clients
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &l->clients_used);
    LinkedList2Node *node;
    while (node = LinkedList2Iterator_Next(&it)) {
        struct PasswordListenerClient *client = UPPER_OBJECT(node, struct PasswordListenerClient, list_node);
        cleanup_client(l, client);
    }
    
    // kill dead variable
    DEAD_KILL(l->dead);
    
    // free listener
    Listener_Free(&l->listener);
    
    // free model SSL file descriptor
    if (l->ssl) {
        ASSERT_FORCE(PR_Close(l->model_prfd) == PR_SUCCESS)
    }
    
    // free client entries
    free(l->clients_data);
}

uint64_t PasswordListener_AddEntry (PasswordListener *l, PasswordListener_pwentry *entry, PasswordListener_handler_client handler_client, void *user)
{
    while (1) {
        // generate password
        DEBUG_ZERO_MEMORY(&entry->password, sizeof(entry->password));
        ASSERT_FORCE(RAND_bytes((uint8_t *)&entry->password, sizeof(entry->password)) == 1)
        // try inserting
        if (BAVL_Insert(&l->passwords, &entry->tree_node, NULL)) {
            break;
        }
    }
    
    entry->handler_client = handler_client;
    entry->user = user;
    
    return entry->password;
}

void PasswordListener_RemoveEntry (PasswordListener *l, PasswordListener_pwentry *entry)
{
    BAVL_Remove(&l->passwords, &entry->tree_node);
}
