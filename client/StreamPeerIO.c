/**
 * @file StreamPeerIO.c
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

#include <ssl.h>
#include <sslerr.h>

#include <misc/offset.h>
#include <misc/debug.h>
#include <misc/byteorder.h>
#include <system/BLog.h>

#include <client/StreamPeerIO.h>

#include <generated/blog_channel_StreamPeerIO.h>

#define STREAMPEERIO_COMPONENT_SEND_SINK 0
#define STREAMPEERIO_COMPONENT_RECEIVE_SOURCE 1
#define STREAMPEERIO_COMPONENT_RECEIVE_DECODER 2

#define MODE_NONE 0
#define MODE_CONNECT 1
#define MODE_LISTEN 2

#define CONNECT_STATE_CONNECTING 0
#define CONNECT_STATE_HANDSHAKE 1
#define CONNECT_STATE_SENDING 2
#define CONNECT_STATE_SENT 3
#define CONNECT_STATE_FINISHED 4

#define LISTEN_STATE_LISTENER 0
#define LISTEN_STATE_GOTCLIENT 1
#define LISTEN_STATE_FINISHED 2

#define COMPONENT_SOURCE 1
#define COMPONENT_SINK 2
#define COMPONENT_DECODER 3

static int init_persistent_io (StreamPeerIO *pio, PacketPassInterface *user_recv_if);
static void free_persistent_io (StreamPeerIO *pio);
static void connecting_connect_handler (StreamPeerIO *pio, int event);
static SECStatus client_auth_certificate_callback (StreamPeerIO *pio, PRFileDesc *fd, PRBool checkSig, PRBool isServer);
static SECStatus client_client_auth_data_callback (StreamPeerIO *pio, PRFileDesc *fd, CERTDistNames *caNames, CERTCertificate **pRetCert, SECKEYPrivateKey **pRetKey);
static void connecting_try_handshake (StreamPeerIO *pio);
static void connecting_handshake_read_handler (StreamPeerIO *pio, PRInt16 event);
static void connecting_pwsender_handler (StreamPeerIO *pio, int is_error);
static void error_handler (StreamPeerIO *pio, int component, int code);
static void listener_handler_client (StreamPeerIO *pio, sslsocket *sock);
static int init_io (StreamPeerIO *pio, sslsocket *sock);
static void free_io (StreamPeerIO *pio);
static int compare_certificate (StreamPeerIO *pio, CERTCertificate *cert);
static void reset_state (StreamPeerIO *pio);
static void cleanup_socket (sslsocket *sock, int ssl);
static void reset_and_report_error (StreamPeerIO *pio);

void connecting_connect_handler (StreamPeerIO *pio, int event)
{
    ASSERT(event == BSOCKET_CONNECT)
    ASSERT(pio->mode == MODE_CONNECT)
    ASSERT(pio->connect.state == CONNECT_STATE_CONNECTING)
    DebugObject_Access(&pio->d_obj);
    
    // remove connect event handler
    BSocket_RemoveEventHandler(&pio->connect.sock.sock, BSOCKET_CONNECT);
    
    // check connection result
    int res = BSocket_GetConnectResult(&pio->connect.sock.sock);
    if (res != 0) {
        BLog(BLOG_NOTICE, "Connection failed (%d)", res);
        goto fail0;
    }
    
    if (pio->ssl) {
        // create BSocket NSPR file descriptor
        BSocketPRFileDesc_Create(&pio->connect.sock.bottom_prfd, &pio->connect.sock.sock);
        
        // create SSL file descriptor from the socket's BSocketPRFileDesc
        if (!(pio->connect.sock.ssl_prfd = SSL_ImportFD(NULL, &pio->connect.sock.bottom_prfd))) {
            ASSERT_FORCE(PR_Close(&pio->connect.sock.bottom_prfd) == PR_SUCCESS)
            goto fail0;
        }
        
        // set client mode
        if (SSL_ResetHandshake(pio->connect.sock.ssl_prfd, PR_FALSE) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_ResetHandshake failed");
            goto fail_ssl1;
        }
        
        // set verify peer certificate hook
        if (SSL_AuthCertificateHook(pio->connect.sock.ssl_prfd, (SSLAuthCertificate)client_auth_certificate_callback, pio) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_AuthCertificateHook failed");
            goto fail_ssl1;
        }
        
        // set client certificate callback
        if (SSL_GetClientAuthDataHook(pio->connect.sock.ssl_prfd, (SSLGetClientAuthData)client_client_auth_data_callback, pio) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_GetClientAuthDataHook failed");
            goto fail_ssl1;
        }
        
        // initialize BPRFileDesc on SSL file descriptor
        BPRFileDesc_Init(&pio->connect.sock.ssl_bprfd, pio->connect.sock.ssl_prfd);
        
        // add event handler for driving handshake
        BPRFileDesc_AddEventHandler(&pio->connect.sock.ssl_bprfd, PR_POLL_READ, (BPRFileDesc_handler)connecting_handshake_read_handler, pio);
        
        // change state
        pio->connect.state = CONNECT_STATE_HANDSHAKE;
        
        // start handshake
        connecting_try_handshake(pio);
        return;
    } else {
        // init password sender
        PasswordSender_Init(&pio->connect.pwsender, pio->connect.password, 0, &pio->connect.sock.sock, NULL, (PasswordSender_handler)connecting_pwsender_handler, pio, pio->reactor);
        
        // change state
        pio->connect.state = CONNECT_STATE_SENDING;
        
        return;
    }
    
    // cleanup
fail_ssl1:
    ASSERT_FORCE(PR_Close(pio->connect.sock.ssl_prfd) == PR_SUCCESS)
fail0:
    reset_and_report_error(pio);
    return;
}

SECStatus client_auth_certificate_callback (StreamPeerIO *pio, PRFileDesc *fd, PRBool checkSig, PRBool isServer)
{
    ASSERT(pio->ssl)
    ASSERT(pio->mode == MODE_CONNECT)
    ASSERT(pio->connect.state == CONNECT_STATE_HANDSHAKE)
    DebugObject_Access(&pio->d_obj);
    
    // This callback is used to bypass checking the server's domain name, as peers
    // don't have domain names. We byte-compare the certificate to the one reported
    // by the server anyway.
    
    SECStatus ret = SECFailure;
    
    CERTCertificate *server_cert = SSL_PeerCertificate(pio->connect.sock.ssl_prfd);
    if (!server_cert) {
        BLog(BLOG_ERROR, "SSL_PeerCertificate failed");
        PORT_SetError(SSL_ERROR_BAD_CERTIFICATE);
        goto fail1;
    }
    
    if (CERT_VerifyCertNow(CERT_GetDefaultCertDB(), server_cert, PR_TRUE, certUsageSSLServer, SSL_RevealPinArg(pio->connect.sock.ssl_prfd)) != SECSuccess) {
        goto fail2;
    }
    
    // compare to certificate provided by the server
    if (!compare_certificate(pio, server_cert)) {
        PORT_SetError(SSL_ERROR_BAD_CERTIFICATE);
        goto fail2;
    }
    
    ret = SECSuccess;
    
fail2:
    CERT_DestroyCertificate(server_cert);
fail1:
    return ret;
}

SECStatus client_client_auth_data_callback (StreamPeerIO *pio, PRFileDesc *fd, CERTDistNames *caNames, CERTCertificate **pRetCert, SECKEYPrivateKey **pRetKey)
{
    ASSERT(pio->ssl)
    ASSERT(pio->mode == MODE_CONNECT)
    ASSERT(pio->connect.state == CONNECT_STATE_HANDSHAKE)
    DebugObject_Access(&pio->d_obj);
    
    CERTCertificate *cert = CERT_DupCertificate(pio->connect.ssl_cert);
    if (!cert) {
        BLog(BLOG_ERROR, "CERT_DupCertificate failed");
        goto fail0;
    }
    
    SECKEYPrivateKey *key = SECKEY_CopyPrivateKey(pio->connect.ssl_key);
    if (!key) {
        BLog(BLOG_ERROR, "SECKEY_CopyPrivateKey failed");
        goto fail1;
    }
    
    *pRetCert = cert;
    *pRetKey = key;
    return SECSuccess;
    
fail1:
    CERT_DestroyCertificate(cert);
fail0:
    return SECFailure;
}

void connecting_try_handshake (StreamPeerIO *pio)
{
    ASSERT(pio->ssl)
    ASSERT(pio->mode == MODE_CONNECT)
    ASSERT(pio->connect.state == CONNECT_STATE_HANDSHAKE)
    
    if (SSL_ForceHandshake(pio->connect.sock.ssl_prfd) != SECSuccess) {
        PRErrorCode error = PR_GetError();
        if (error == PR_WOULD_BLOCK_ERROR) {
            BPRFileDesc_EnableEvent(&pio->connect.sock.ssl_bprfd, PR_POLL_READ);
            return;
        }
        BLog(BLOG_NOTICE, "SSL_ForceHandshake failed (%d)", (int)error);
        goto fail0;
    }
    
    // remove client certificate callback
    if (SSL_GetClientAuthDataHook(pio->connect.sock.ssl_prfd, NULL, NULL) != SECSuccess) {
        BLog(BLOG_ERROR, "SSL_GetClientAuthDataHook failed");
        goto fail0;
    }
    
    // remove verify peer certificate callback
    if (SSL_AuthCertificateHook(pio->connect.sock.ssl_prfd, NULL, NULL) != SECSuccess) {
        BLog(BLOG_ERROR, "SSL_AuthCertificateHook failed");
        goto fail0;
    }
    
    // remove read handler
    BPRFileDesc_RemoveEventHandler(&pio->connect.sock.ssl_bprfd, PR_POLL_READ);
    
    // init password sender
    PasswordSender_Init(&pio->connect.pwsender, pio->connect.password, 1, NULL, &pio->connect.sock.ssl_bprfd, (PasswordSender_handler)connecting_pwsender_handler, pio, pio->reactor);
    
    // change state
    pio->connect.state = CONNECT_STATE_SENDING;
    
    return;
    
    // cleanup
fail0:
    reset_and_report_error(pio);
    return;
}

void connecting_handshake_read_handler (StreamPeerIO *pio, PRInt16 event)
{
    ASSERT(pio->ssl)
    ASSERT(pio->mode == MODE_CONNECT)
    ASSERT(pio->connect.state == CONNECT_STATE_HANDSHAKE)
    DebugObject_Access(&pio->d_obj);
    
    connecting_try_handshake(pio);
    return;
}

static void connecting_pwsender_handler (StreamPeerIO *pio, int is_error)
{
    ASSERT(pio->mode == MODE_CONNECT)
    ASSERT(pio->connect.state == CONNECT_STATE_SENDING)
    DebugObject_Access(&pio->d_obj);
    
    if (is_error) {
        BLog(BLOG_NOTICE, "error sending password");
        goto fail0;
    }
    
    // free password sender
    PasswordSender_Free(&pio->connect.pwsender);
    
    // change state
    pio->connect.state = CONNECT_STATE_SENT;
    
    // setup i/o
    if (!init_io(pio, &pio->connect.sock)) {
        goto fail0;
    }
    
    // change state
    pio->connect.state = CONNECT_STATE_FINISHED;
    
    return;
    
fail0:
    reset_and_report_error(pio);
    return;
}

void error_handler (StreamPeerIO *pio, int component, int code)
{
    ASSERT(pio->sock)
    DebugObject_Access(&pio->d_obj);
    
    // cleanup
    reset_and_report_error(pio);
    return;
}

void listener_handler_client (StreamPeerIO *pio, sslsocket *sock)
{
    ASSERT(pio->mode == MODE_LISTEN)
    ASSERT(pio->listen.state == LISTEN_STATE_LISTENER)
    DebugObject_Access(&pio->d_obj);
    
    // remember socket
    pio->listen.sock = sock;
    
    // change state
    pio->listen.state = LISTEN_STATE_GOTCLIENT;
    
    // check ceritficate
    if (pio->ssl) {
        CERTCertificate *peer_cert = SSL_PeerCertificate(pio->listen.sock->ssl_prfd);
        if (!peer_cert) {
            BLog(BLOG_ERROR, "SSL_PeerCertificate failed");
            goto fail0;
        }
        
        // compare certificate to the one provided by the server
        if (!compare_certificate(pio, peer_cert)) {
            CERT_DestroyCertificate(peer_cert);
            goto fail0;
        }
        
        CERT_DestroyCertificate(peer_cert);
    }
    
    // setup i/o
    if (!init_io(pio, pio->listen.sock)) {
        goto fail0;
    }
    
    // change state
    pio->listen.state = LISTEN_STATE_FINISHED;
    
    return;
    
    // cleanup
fail0:
    reset_and_report_error(pio);
    return;
}

int init_persistent_io (StreamPeerIO *pio, PacketPassInterface *user_recv_if)
{
    // init error domain
    FlowErrorDomain_Init(&pio->ioerrdomain, (FlowErrorDomain_handler)error_handler, pio);
    
    // init receiveing objects
    StreamRecvConnector_Init(&pio->input_connector, BReactor_PendingGroup(pio->reactor));
    if (!PacketProtoDecoder_Init(
        &pio->input_decoder, FlowErrorReporter_Create(&pio->ioerrdomain, COMPONENT_DECODER),
        StreamRecvConnector_GetOutput(&pio->input_connector), user_recv_if, BReactor_PendingGroup(pio->reactor)
    )) {
        goto fail1;
    }
    
    // init sending objects
    PacketCopier_Init(&pio->output_user_copier, pio->payload_mtu, BReactor_PendingGroup(pio->reactor));
    PacketProtoEncoder_Init(&pio->output_user_ppe, PacketCopier_GetOutput(&pio->output_user_copier), BReactor_PendingGroup(pio->reactor));
    PacketPassConnector_Init(&pio->output_connector, PACKETPROTO_ENCLEN(pio->payload_mtu), BReactor_PendingGroup(pio->reactor));
    if (!SinglePacketBuffer_Init(&pio->output_user_spb, PacketProtoEncoder_GetOutput(&pio->output_user_ppe), PacketPassConnector_GetInput(&pio->output_connector), BReactor_PendingGroup(pio->reactor))) {
        goto fail2;
    }
    
    return 1;
    
fail2:
    PacketPassConnector_Free(&pio->output_connector);
    PacketProtoEncoder_Free(&pio->output_user_ppe);
    PacketCopier_Free(&pio->output_user_copier);
    PacketProtoDecoder_Free(&pio->input_decoder);
fail1:
    StreamRecvConnector_Free(&pio->input_connector);
    return 0;
}

void free_persistent_io (StreamPeerIO *pio)
{
    // free sending objects
    SinglePacketBuffer_Free(&pio->output_user_spb);
    PacketPassConnector_Free(&pio->output_connector);
    PacketProtoEncoder_Free(&pio->output_user_ppe);
    PacketCopier_Free(&pio->output_user_copier);
    
    // free receiveing objects
    PacketProtoDecoder_Free(&pio->input_decoder);
    StreamRecvConnector_Free(&pio->input_connector);
}

int init_io (StreamPeerIO *pio, sslsocket *sock)
{
    ASSERT(!pio->sock)
    
    // init receiving
    StreamRecvInterface *source_interface;
    if (pio->ssl) {
        PRStreamSource_Init(
            &pio->input_source.ssl, FlowErrorReporter_Create(&pio->ioerrdomain, COMPONENT_SOURCE),
            &sock->ssl_bprfd, BReactor_PendingGroup(pio->reactor)
        );
        source_interface = PRStreamSource_GetOutput(&pio->input_source.ssl);
    } else {
        StreamSocketSource_Init(
            &pio->input_source.plain, FlowErrorReporter_Create(&pio->ioerrdomain, COMPONENT_SOURCE),
            &sock->sock, BReactor_PendingGroup(pio->reactor)
        );
        source_interface = StreamSocketSource_GetOutput(&pio->input_source.plain);
    }
    StreamRecvConnector_ConnectInput(&pio->input_connector, source_interface);
    
    // init sending
    StreamPassInterface *sink_interface;
    if (pio->ssl) {
        PRStreamSink_Init(
            &pio->output_sink.ssl, FlowErrorReporter_Create(&pio->ioerrdomain, COMPONENT_SINK),
            &sock->ssl_bprfd, BReactor_PendingGroup(pio->reactor)
        );
        sink_interface = PRStreamSink_GetInput(&pio->output_sink.ssl);
    } else {
        StreamSocketSink_Init(
            &pio->output_sink.plain, FlowErrorReporter_Create(&pio->ioerrdomain, COMPONENT_SINK),
            &sock->sock, BReactor_PendingGroup(pio->reactor)
        );
        sink_interface = StreamSocketSink_GetInput(&pio->output_sink.plain);
    }
    PacketStreamSender_Init(&pio->output_pss, sink_interface, PACKETPROTO_ENCLEN(pio->payload_mtu), BReactor_PendingGroup(pio->reactor));
    PacketPassConnector_ConnectOutput(&pio->output_connector, PacketStreamSender_GetInput(&pio->output_pss));
    
    pio->sock = sock;
    
    return 1;
}

void free_io (StreamPeerIO *pio)
{
    ASSERT(pio->sock)
    
    // reset decoder
    PacketProtoDecoder_Reset(&pio->input_decoder);
    
    // free sending
    PacketPassConnector_DisconnectOutput(&pio->output_connector);
    PacketStreamSender_Free(&pio->output_pss);
    if (pio->ssl) {
        PRStreamSink_Free(&pio->output_sink.ssl);
    } else {
        StreamSocketSink_Free(&pio->output_sink.plain);
    }
    
    // free receiving
    StreamRecvConnector_DisconnectInput(&pio->input_connector);
    if (pio->ssl) {
        PRStreamSource_Free(&pio->input_source.ssl);
    } else {
        StreamSocketSource_Free(&pio->input_source.plain);
    }
    
    pio->sock = NULL;
}

int compare_certificate (StreamPeerIO *pio, CERTCertificate *cert)
{
    ASSERT(pio->ssl)
    
    SECItem der = cert->derCert;
    if (der.len != pio->ssl_peer_cert_len || memcmp(der.data, pio->ssl_peer_cert, der.len)) {
        BLog(BLOG_NOTICE, "Client certificate doesn't match");
        return 0;
    }
    
    return 1;
}

void reset_state (StreamPeerIO *pio)
{
    // free resources
    switch (pio->mode) {
        case MODE_NONE:
            break;
        case MODE_LISTEN:
            switch (pio->listen.state) {
                case LISTEN_STATE_FINISHED:
                    free_io(pio);
                case LISTEN_STATE_GOTCLIENT:
                    cleanup_socket(pio->listen.sock, pio->ssl);
                    free(pio->listen.sock);
                    break;
                case LISTEN_STATE_LISTENER:
                    PasswordListener_RemoveEntry(pio->listen.listener, &pio->listen.pwentry);
                    break;
                default:
                    ASSERT(0);
            }
            pio->mode = MODE_NONE;
            break;
        case MODE_CONNECT:
            switch (pio->connect.state) {
                case CONNECT_STATE_FINISHED:
                    free_io(pio);
                case CONNECT_STATE_SENT:
                case CONNECT_STATE_SENDING:
                    if (pio->connect.state == CONNECT_STATE_SENDING) {
                        PasswordSender_Free(&pio->connect.pwsender);
                    }
                case CONNECT_STATE_HANDSHAKE:
                    if (pio->ssl) {
                        BPRFileDesc_Free(&pio->connect.sock.ssl_bprfd);
                        ASSERT_FORCE(PR_Close(pio->connect.sock.ssl_prfd) == PR_SUCCESS)
                    }
                case CONNECT_STATE_CONNECTING:
                    BSocket_Free(&pio->connect.sock.sock);
                    break;
                default:
                    ASSERT(0);
            }
            pio->mode = MODE_NONE;
            break;
        default:
            ASSERT(0);
    }
    
    ASSERT(!pio->sock)
}

void cleanup_socket (sslsocket *sock, int ssl)
{
    if (ssl) {
        // free BPRFileDesc
        BPRFileDesc_Free(&sock->ssl_bprfd);
        // free SSL NSPR file descriptor
        ASSERT_FORCE(PR_Close(sock->ssl_prfd) == PR_SUCCESS)
    }
    
    // free socket
    BSocket_Free(&sock->sock);
}

void reset_and_report_error (StreamPeerIO *pio)
{
    reset_state(pio);
    
    pio->handler_error(pio->user);
    return;
}

int StreamPeerIO_Init (
    StreamPeerIO *pio,
    BReactor *reactor,
    int ssl,
    uint8_t *ssl_peer_cert,
    int ssl_peer_cert_len,
    int payload_mtu,
    PacketPassInterface *user_recv_if,
    StreamPeerIO_handler_error handler_error,
    void *user
)
{
    ASSERT(ssl == 0 || ssl == 1)
    ASSERT(payload_mtu >= 0)
    ASSERT(PacketPassInterface_GetMTU(user_recv_if) >= payload_mtu)
    
    // init arguments
    pio->reactor = reactor;
    pio->ssl = ssl;
    if (pio->ssl) {
        pio->ssl_peer_cert = ssl_peer_cert;
        pio->ssl_peer_cert_len = ssl_peer_cert_len;
    }
    pio->payload_mtu = payload_mtu;
    pio->handler_error = handler_error;
    pio->user = user;
    
    // check payload MTU
    if (pio->payload_mtu > PACKETPROTO_MAXPAYLOAD) {
        BLog(BLOG_ERROR, "payload MTU is too large");
        goto fail0;
    }
    
    // init persistent I/O modules
    if (!init_persistent_io(pio, user_recv_if)) {
        goto fail0;
    }
    
    // set mode none
    pio->mode = MODE_NONE;
    
    // set no socket
    pio->sock = NULL;
    
    DebugObject_Init(&pio->d_obj);
    
    return 1;
    
fail0:
    return 0;
}

void StreamPeerIO_Free (StreamPeerIO *pio)
{
    DebugObject_Free(&pio->d_obj);

    // reset state
    reset_state(pio);
    
    // free persistent I/O modules
    free_persistent_io(pio);
}

PacketPassInterface * StreamPeerIO_GetSendInput (StreamPeerIO *pio)
{
    DebugObject_Access(&pio->d_obj);
    
    return PacketCopier_GetInput(&pio->output_user_copier);
}

int StreamPeerIO_Connect (StreamPeerIO *pio, BAddr addr, uint64_t password, CERTCertificate *ssl_cert, SECKEYPrivateKey *ssl_key)
{
    ASSERT(!BAddr_IsInvalid(&addr))
    DebugObject_Access(&pio->d_obj);
    
    // reset state
    reset_state(pio);
    
    // create socket
    if (BSocket_Init(&pio->connect.sock.sock, pio->reactor, addr.type, BSOCKET_TYPE_STREAM) < 0) {
        BLog(BLOG_ERROR, "BSocket_Init failed");
        goto fail0;
    }
    
    // attempt connection
    if (BSocket_Connect(&pio->connect.sock.sock, &addr, 1) >= 0 || BSocket_GetError(&pio->connect.sock.sock) != BSOCKET_ERROR_IN_PROGRESS) {
        BLog(BLOG_NOTICE, "BSocket_Connect failed");
        goto fail1;
    }
    
    // waiting for connection result
    BSocket_AddEventHandler(&pio->connect.sock.sock, BSOCKET_CONNECT, (BSocket_handler)connecting_connect_handler, pio);
    BSocket_EnableEvent(&pio->connect.sock.sock, BSOCKET_CONNECT);
    
    // remember data
    if (pio->ssl) {
        pio->connect.ssl_cert = ssl_cert;
        pio->connect.ssl_key = ssl_key;
    }
    pio->connect.password = htol64(password);
    
    // set state
    pio->mode = MODE_CONNECT;
    pio->connect.state = CONNECT_STATE_CONNECTING;
    
    return 1;
    
fail1:
    BSocket_Free(&pio->connect.sock.sock);
fail0:
    return 0;
}

void StreamPeerIO_Listen (StreamPeerIO *pio, PasswordListener *listener, uint64_t *password)
{
    ASSERT(listener->ssl == pio->ssl)
    DebugObject_Access(&pio->d_obj);
    
    // reset state
    reset_state(pio);
    
    // add PasswordListener entry
    uint64_t newpass = PasswordListener_AddEntry(listener, &pio->listen.pwentry, (PasswordListener_handler_client)listener_handler_client, pio);
    
    // remember data
    pio->listen.listener = listener;
    
    // set state
    pio->mode = MODE_LISTEN;
    pio->listen.state = LISTEN_STATE_LISTENER;
    
    *password = newpass;
}
