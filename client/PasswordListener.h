/**
 * @file PasswordListener.h
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
 * 
 * @section DESCRIPTION
 * 
 * Object used to listen on a socket, accept clients and identify them
 * based on a number they send.
 */

#ifndef BADVPN_CLIENT_PASSWORDLISTENER_H
#define BADVPN_CLIENT_PASSWORDLISTENER_H

#include <stdint.h>

#include <prio.h>

#include <cert.h>
#include <keyhi.h>

#include <misc/dead.h>
#include <misc/debug.h>
#include <system/DebugObject.h>
#include <system/Listener.h>
#include <misc/sslsocket.h>
#include <structure/LinkedList2.h>
#include <structure/BAVL.h>
#include <nspr_support/BPRFileDesc.h>

/**
 * Handler function called when a client identifies itself with a password
 * belonging to one of the password entries.
 * The password entry is unregistered before the handler is called
 * and must not be unregistered again.
 * 
 * @param user as in {@link PasswordListener_AddEntry}
 * @param sock structure that contains the socket ({@link BSocket}) and, if TLS
 *             is enabled, the SSL socket (PRFileDesc and {@link BPRFileDesc}).
 *             The structure was allocated with malloc() and the user
 *             is responsible for freeing it.
 */
typedef void (*PasswordListener_handler_client) (void *user, sslsocket *sock);

struct PasswordListenerClient;

/**
 * Object used to listen on a socket, accept clients and identify them
 * based on a number they send.
 */
typedef struct {
    DebugObject d_obj;
    BReactor *bsys;
    int ssl;
    PRFileDesc model_dprfd;
    PRFileDesc *model_prfd;
    struct PasswordListenerClient *clients_data;
    LinkedList2 clients_free;
    LinkedList2 clients_used;
    BAVL passwords;
    Listener listener;
    dead_t dead;
} PasswordListener;

typedef struct {
    uint64_t password;
    BAVLNode tree_node;
    PasswordListener_handler_client handler_client;
    void *user;
} PasswordListener_pwentry;

struct PasswordListenerClient {
    PasswordListener *l;
    LinkedList2Node list_node;
    sslsocket *sock;
    uint64_t recv_buffer;
    int recv_buffer_pos;
};

/**
 * Initializes the object.
 * 
 * @param l the object
 * @param bsys reactor we live in
 * @param listen_addr address to listen on. Must not be invalid.
 * @param max_clients maximum number of client to hold until they are identified.
 *                    Must be >0.
 * @param ssl whether to use TLS. Must be 1 or 0.
 * @param cert if using TLS, the server certificate
 * @param key if using TLS, the private key
 * @return 1 on success, 0 on failure
 */
int PasswordListener_Init (PasswordListener *l, BReactor *bsys, BAddr listen_addr, int max_clients, int ssl, CERTCertificate *cert, SECKEYPrivateKey *key) WARN_UNUSED;

/**
 * Frees the object.
 * 
 * @param l the object
 */
void PasswordListener_Free (PasswordListener *l);

/**
 * Registers a password entry.
 * 
 * @param l the object
 * @param entry uninitialized entry structure
 * @param handler_client handler function to call when a client identifies
 *                       with the password which this function returns
 * @param user value to pass to handler function
 * @return password which a client should send to be recognized and
 *         dispatched to the handler function. Should be treated as a numeric
 *         value, which a client should as a little-endian 64-bit unsigned integer
 *         when it connects.
 */
uint64_t PasswordListener_AddEntry (PasswordListener *l, PasswordListener_pwentry *entry, PasswordListener_handler_client handler_client, void *user);

/**
 * Unregisters a password entry.
 * Note that when a client is dispatched, its entry is unregistered
 * automatically and must not be unregistered again here.
 * 
 * @param l the object
 * @param entry entry to unregister
 */
void PasswordListener_RemoveEntry (PasswordListener *l, PasswordListener_pwentry *entry);

#endif
