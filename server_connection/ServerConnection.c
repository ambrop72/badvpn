/**
 * @file ServerConnection.c
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

#include <stdio.h>

#include <misc/debug.h>
#include <system/BLog.h>

#include <server_connection/ServerConnection.h>

#include <generated/blog_channel_ServerConnection.h>

#define STATE_CONNECTING 1
#define STATE_WAITINIT 2
#define STATE_COMPLETE 3

#define COMPONENT_SOURCE 1
#define COMPONENT_SINK 2
#define COMPONENT_DECODER 3

static void report_error (ServerConnection *o);
static void connect_handler (ServerConnection *o, int event);
static void pending_handler (ServerConnection *o);
static SECStatus client_auth_data_callback (ServerConnection *o, PRFileDesc *fd, CERTDistNames *caNames, CERTCertificate **pRetCert, SECKEYPrivateKey **pRetKey);
static void error_handler (ServerConnection *o, int component, const void *data);
static void input_handler_send (ServerConnection *o, uint8_t *data, int data_len);
static void packet_hello (ServerConnection *o, uint8_t *data, int data_len);
static void packet_newclient (ServerConnection *o, uint8_t *data, int data_len);
static void packet_endclient (ServerConnection *o, uint8_t *data, int data_len);
static void packet_inmsg (ServerConnection *o, uint8_t *data, int data_len);
static int start_packet (ServerConnection *o, void **data, int len);
static void end_packet (ServerConnection *o, uint8_t type);

void report_error (ServerConnection *o)
{
    DEBUGERROR(&o->d_err, o->handler_error(o->user))
}

void connect_handler (ServerConnection *o, int event)
{
    ASSERT(o->state == STATE_CONNECTING)
    ASSERT(event == BSOCKET_CONNECT)
    DebugObject_Access(&o->d_obj);
    
    // remove connect event handler
    BSocket_RemoveEventHandler(&o->sock, BSOCKET_CONNECT);
    
    // check connection attempt result
    int res = BSocket_GetConnectResult(&o->sock);
    if (res != 0) {
        BLog(BLOG_ERROR, "connection failed (BSocket error %d)", res);
        goto fail0;
    }
    
    BLog(BLOG_NOTICE, "connected");
    
    if (o->have_ssl) {
        // create BSocket NSPR file descriptor
        BSocketPRFileDesc_Create(&o->bottom_prfd, &o->sock);
        
        // create SSL file descriptor from the socket's BSocketPRFileDesc
        if (!(o->ssl_prfd = SSL_ImportFD(NULL, &o->bottom_prfd))) {
            BLog(BLOG_ERROR, "SSL_ImportFD failed");
            ASSERT_FORCE(PR_Close(&o->bottom_prfd) == PR_SUCCESS)
            goto fail0;
        }
        
        // set client mode
        if (SSL_ResetHandshake(o->ssl_prfd, PR_FALSE) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_ResetHandshake failed");
            goto fail1;
        }
        
        // set server name
        if (SSL_SetURL(o->ssl_prfd, o->server_name) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_SetURL failed");
            goto fail1;
        }
        
        // set client certificate callback
        if (SSL_GetClientAuthDataHook(o->ssl_prfd, (SSLGetClientAuthData)client_auth_data_callback, o) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_GetClientAuthDataHook failed");
            goto fail1;
        }
        
        // init BPRFileDesc
        BPRFileDesc_Init(&o->ssl_bprfd, o->ssl_prfd);
    }
    
    // init error domain
    FlowErrorDomain_Init(&o->ioerrdomain, (FlowErrorDomain_handler)error_handler, o);
    
    // init input chain
    StreamRecvInterface *source_interface;
    if (o->have_ssl) {
        PRStreamSource_Init(&o->input_source.ssl, FlowErrorReporter_Create(&o->ioerrdomain, COMPONENT_SOURCE), &o->ssl_bprfd, BReactor_PendingGroup(o->reactor));
        source_interface = PRStreamSource_GetOutput(&o->input_source.ssl);
    } else {
        StreamSocketSource_Init(&o->input_source.plain, FlowErrorReporter_Create(&o->ioerrdomain, COMPONENT_SOURCE), &o->sock, BReactor_PendingGroup(o->reactor));
        source_interface = StreamSocketSource_GetOutput(&o->input_source.plain);
    }
    PacketPassInterface_Init(&o->input_interface, SC_MAX_ENC, (PacketPassInterface_handler_send)input_handler_send, o, BReactor_PendingGroup(o->reactor));
    if (!PacketProtoDecoder_Init(
        &o->input_decoder,
        FlowErrorReporter_Create(&o->ioerrdomain, COMPONENT_DECODER),
        source_interface,
        &o->input_interface,
        BReactor_PendingGroup(o->reactor)
    )) {
        BLog(BLOG_ERROR, "PacketProtoDecoder_Init failed");
        goto fail2;
    }
    
    // set job to send hello
    // this needs to be in here because hello sending must be done after sending started (so we can write into the send buffer),
    // but before receiving started (so we don't get into conflict with the user sending packets)
    BPending_Init(&o->start_job, BReactor_PendingGroup(o->reactor), (BPending_handler)pending_handler, o);
    BPending_Set(&o->start_job);
    
    // init keepalive output branch
    SCKeepaliveSource_Init(&o->output_ka_zero, BReactor_PendingGroup(o->reactor));
    PacketProtoEncoder_Init(&o->output_ka_encoder, SCKeepaliveSource_GetOutput(&o->output_ka_zero), BReactor_PendingGroup(o->reactor));
    
    // init output common
    
    // init sink
    StreamPassInterface *sink_interface;
    if (o->have_ssl) {
        PRStreamSink_Init(&o->output_sink.ssl, FlowErrorReporter_Create(&o->ioerrdomain, COMPONENT_SINK), &o->ssl_bprfd, BReactor_PendingGroup(o->reactor));
        sink_interface = PRStreamSink_GetInput(&o->output_sink.ssl);
    } else {
        StreamSocketSink_Init(&o->output_sink.plain, FlowErrorReporter_Create(&o->ioerrdomain, COMPONENT_SINK), &o->sock, BReactor_PendingGroup(o->reactor));
        sink_interface = StreamSocketSink_GetInput(&o->output_sink.plain);
    }
    
    // init sender
    PacketStreamSender_Init(&o->output_sender, sink_interface, PACKETPROTO_ENCLEN(SC_MAX_ENC), BReactor_PendingGroup(o->reactor));
    
    // init keepalives
    if (!KeepaliveIO_Init(&o->output_keepaliveio, o->reactor, PacketStreamSender_GetInput(&o->output_sender), PacketProtoEncoder_GetOutput(&o->output_ka_encoder), o->keepalive_interval)) {
        BLog(BLOG_ERROR, "KeepaliveIO_Init failed");
        goto fail3;
    }
    
    // init queue
    PacketPassPriorityQueue_Init(&o->output_queue, KeepaliveIO_GetInput(&o->output_keepaliveio), BReactor_PendingGroup(o->reactor), 0);
    
    // init output local flow
    
    // init queue flow
    PacketPassPriorityQueueFlow_Init(&o->output_local_qflow, &o->output_queue, 0);
    
    // init PacketProtoFlow
    if (!PacketProtoFlow_Init(&o->output_local_oflow, SC_MAX_ENC, o->buffer_size, PacketPassPriorityQueueFlow_GetInput(&o->output_local_qflow), BReactor_PendingGroup(o->reactor))) {
        BLog(BLOG_ERROR, "PacketProtoFlow_Init failed");
        goto fail4;
    }
    o->output_local_if = PacketProtoFlow_GetInput(&o->output_local_oflow);
    
    // have no output packet
    o->output_local_packet_len = -1;
    
    // init output user flow
    PacketPassPriorityQueueFlow_Init(&o->output_user_qflow, &o->output_queue, 1);
    
    // update state
    o->state = STATE_WAITINIT;
    
    return;
    
fail4:
    PacketPassPriorityQueueFlow_Free(&o->output_local_qflow);
    // free output common
    PacketPassPriorityQueue_Free(&o->output_queue);
    KeepaliveIO_Free(&o->output_keepaliveio);
fail3:
    PacketStreamSender_Free(&o->output_sender);
    if (o->have_ssl) {
        PRStreamSink_Free(&o->output_sink.ssl);
    } else {
        StreamSocketSink_Free(&o->output_sink.plain);
    }
    // free output keep-alive branch
    PacketProtoEncoder_Free(&o->output_ka_encoder);
    SCKeepaliveSource_Free(&o->output_ka_zero);
    // free job
    BPending_Free(&o->start_job);
    // free input
    PacketProtoDecoder_Free(&o->input_decoder);
fail2:
    PacketPassInterface_Free(&o->input_interface);
    if (o->have_ssl) {
        PRStreamSource_Free(&o->input_source.ssl);
    } else {
        StreamSocketSource_Free(&o->input_source.plain);
    }
    // free SSL
    if (o->have_ssl) {
        BPRFileDesc_Free(&o->ssl_bprfd);
fail1:
        ASSERT_FORCE(PR_Close(o->ssl_prfd) == PR_SUCCESS)
    }
fail0:
    // report error
    report_error(o);
}

void pending_handler (ServerConnection *o)
{
    ASSERT(o->state == STATE_WAITINIT)
    DebugObject_Access(&o->d_obj);
    
    // send hello
    struct sc_client_hello *packet;
    if (!start_packet(o, (void **)&packet, sizeof(struct sc_client_hello))) {
        BLog(BLOG_ERROR, "no buffer for hello");
        report_error(o);
        return;
    }
    packet->version = htol16(SC_VERSION);
    end_packet(o, SCID_CLIENTHELLO);
}

SECStatus client_auth_data_callback (ServerConnection *o, PRFileDesc *fd, CERTDistNames *caNames, CERTCertificate **pRetCert, SECKEYPrivateKey **pRetKey)
{
    ASSERT(o->have_ssl)
    DebugObject_Access(&o->d_obj);
    
    CERTCertificate *newcert;
    if (!(newcert = CERT_DupCertificate(o->client_cert))) {
        return SECFailure;
    }
    
    SECKEYPrivateKey *newkey;
    if (!(newkey = SECKEY_CopyPrivateKey(o->client_key))) {
        CERT_DestroyCertificate(newcert);
        return SECFailure;
    }
    
    *pRetCert = newcert;
    *pRetKey = newkey;
    return SECSuccess;
}

void error_handler (ServerConnection *o, int component, const void *data)
{
    ASSERT(o->state >= STATE_WAITINIT)
    DebugObject_Access(&o->d_obj);
    
    switch (component) {
        case COMPONENT_SOURCE:
        case COMPONENT_SINK:
            BLog(BLOG_ERROR, "BSocket error %d", BSocket_GetError(&o->sock));
            if (o->have_ssl) {
                BLog(BLOG_ERROR, "NSPR error %d", (int)PR_GetError());
            }
            break;
        case COMPONENT_DECODER:
            BLog(BLOG_ERROR, "decoder error %d", *((int *)data));
            break;
        default:
            ASSERT(0);
    }
    
    BLog(BLOG_ERROR, "lost connection");
    
    report_error(o);
    return;
}

void input_handler_send (ServerConnection *o, uint8_t *data, int data_len)
{
    ASSERT(o->state >= STATE_WAITINIT)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= SC_MAX_ENC)
    DebugObject_Access(&o->d_obj);
    
    if (data_len < sizeof(struct sc_header)) {
        BLog(BLOG_ERROR, "packet too short (no sc header)");
        report_error(o);
        return;
    }
    
    struct sc_header *header = (struct sc_header *)data;
    
    uint8_t *sc_data = data + sizeof(struct sc_header);
    int sc_data_len = data_len - sizeof(struct sc_header);
    
    // finish packet
    PacketPassInterface_Done(&o->input_interface);
    
    // call appropriate handler based on packet type
    switch (header->type) {
        case SCID_SERVERHELLO:
            packet_hello(o, sc_data, sc_data_len);
            return;
        case SCID_NEWCLIENT:
            packet_newclient(o, sc_data, sc_data_len);
            return;
        case SCID_ENDCLIENT:
            packet_endclient(o, sc_data, sc_data_len);
            return;
        case SCID_INMSG:
            packet_inmsg(o, sc_data, sc_data_len);
            return;
        default:
            BLog(BLOG_ERROR, "unknown packet type %d", (int)header->type);
            report_error(o);
            return;
    }
}

void packet_hello (ServerConnection *o, uint8_t *data, int data_len)
{
    if (o->state != STATE_WAITINIT) {
        BLog(BLOG_ERROR, "hello: not expected");
        report_error(o);
        return;
    }
    
    if (data_len != sizeof(struct sc_server_hello)) {
        BLog(BLOG_ERROR, "hello: invalid length");
        report_error(o);
        return;
    }
    
    struct sc_server_hello *msg = (struct sc_server_hello *)data;
    
    // change state
    o->state = STATE_COMPLETE;
    
    // report
    o->handler_ready(o->user, ltoh16(msg->id), msg->clientAddr);
    return;
}

void packet_newclient (ServerConnection *o, uint8_t *data, int data_len)
{
    if (o->state != STATE_COMPLETE) {
        BLog(BLOG_ERROR, "newclient: not expected");
        report_error(o);
        return;
    }
    
    if (data_len < sizeof(struct sc_server_newclient) || data_len > sizeof(struct sc_server_newclient) + SCID_NEWCLIENT_MAX_CERT_LEN) {
        BLog(BLOG_ERROR, "newclient: invalid length");
        report_error(o);
        return;
    }
    
    struct sc_server_newclient *msg = (struct sc_server_newclient *)data;
    peerid_t id = ltoh16(msg->id);
    int flags = ltoh16(msg->flags);
    
    uint8_t *cert_data = (uint8_t *)msg + sizeof(struct sc_server_newclient);
    int cert_len = data_len - sizeof(struct sc_server_newclient);
    
    // report
    o->handler_newclient(o->user, id, flags, cert_data, cert_len);
    return;
}

void packet_endclient (ServerConnection *o, uint8_t *data, int data_len)
{
    if (o->state != STATE_COMPLETE) {
        BLog(BLOG_ERROR, "endclient: not expected");
        report_error(o);
        return;
    }
    
    if (data_len != sizeof(struct sc_server_endclient)) {
        BLog(BLOG_ERROR, "endclient: invalid length");
        report_error(o);
        return;
    }
    
    struct sc_server_endclient *msg = (struct sc_server_endclient *)data;
    peerid_t id = ltoh16(msg->id);
    
    // report
    o->handler_endclient(o->user, id);
    return;
}

void packet_inmsg (ServerConnection *o, uint8_t *data, int data_len)
{
    if (o->state != STATE_COMPLETE) {
        BLog(BLOG_ERROR, "inmsg: not expected");
        report_error(o);
        return;
    }
    
    if (data_len < sizeof(struct sc_server_inmsg)) {
        BLog(BLOG_ERROR, "inmsg: missing header");
        report_error(o);
        return;
    }
    
    if (data_len > sizeof(struct sc_server_inmsg) + SC_MAX_MSGLEN) {
        BLog(BLOG_ERROR, "inmsg: too long");
        report_error(o);
        return;
    }
    
    struct sc_server_inmsg *msg = (struct sc_server_inmsg *)data;
    peerid_t peer_id = ltoh16(msg->clientid);
    uint8_t *payload = data + sizeof(struct sc_server_inmsg);
    int payload_len = data_len - sizeof(struct sc_server_inmsg);
    
    // report
    o->handler_message(o->user, peer_id, payload, payload_len);
    return;
}

int start_packet (ServerConnection *o, void **data, int len)
{
    ASSERT(o->state >= STATE_WAITINIT)
    ASSERT(o->output_local_packet_len == -1)
    ASSERT(len >= 0)
    ASSERT(len <= SC_MAX_PAYLOAD)
    ASSERT(data || len == 0)
    
    // obtain memory location
    if (!BufferWriter_StartPacket(o->output_local_if, &o->output_local_packet)) {
        BLog(BLOG_ERROR, "out of buffer");
        return 0;
    }
    
    o->output_local_packet_len = len;
    
    if (data) {
        *data = o->output_local_packet + sizeof(struct sc_header);
    }
    
    return 1;
}

void end_packet (ServerConnection *o, uint8_t type)
{
    ASSERT(o->state >= STATE_WAITINIT)
    ASSERT(o->output_local_packet_len >= 0)
    ASSERT(o->output_local_packet_len <= SC_MAX_PAYLOAD)
    
    // write header
    struct sc_header *header = (struct sc_header *)o->output_local_packet;
    header->type = type;
    
    // finish writing packet
    BufferWriter_EndPacket(o->output_local_if, sizeof(struct sc_header) + o->output_local_packet_len);
    
    o->output_local_packet_len = -1;
}

int ServerConnection_Init (
    ServerConnection *o,
    BReactor *reactor,
    BAddr addr,
    int keepalive_interval,
    int buffer_size,
    int have_ssl,
    CERTCertificate *client_cert,
    SECKEYPrivateKey *client_key,
    const char *server_name,
    void *user,
    ServerConnection_handler_error handler_error,
    ServerConnection_handler_ready handler_ready,
    ServerConnection_handler_newclient handler_newclient,
    ServerConnection_handler_endclient handler_endclient,
    ServerConnection_handler_message handler_message
)
{
    ASSERT(keepalive_interval > 0)
    ASSERT(buffer_size > 0)
    ASSERT(have_ssl == 0 || have_ssl == 1)
    
    // init arguments
    o->reactor = reactor;
    o->keepalive_interval = keepalive_interval;
    o->buffer_size = buffer_size;
    o->have_ssl = have_ssl;
    if (have_ssl) {
        o->client_cert = client_cert;
        o->client_key = client_key;
        snprintf(o->server_name, sizeof(o->server_name), "%s", server_name);
    }
    o->user = user;
    o->handler_error = handler_error;
    o->handler_ready = handler_ready;
    o->handler_newclient = handler_newclient;
    o->handler_endclient = handler_endclient;
    o->handler_message = handler_message;
    
    // init socket
    if (BSocket_Init(&o->sock, o->reactor, addr.type, BSOCKET_TYPE_STREAM) < 0) {
        BLog(BLOG_ERROR, "BSocket_Init failed (%d)", BSocket_GetError(&o->sock));
        goto fail0;
    }
    
    // start connecting
    int res = BSocket_Connect(&o->sock, &addr);
    if (res != -1 || BSocket_GetError(&o->sock) != BSOCKET_ERROR_IN_PROGRESS) {
        BLog(BLOG_ERROR, "BSocket_Connect failed (%d)", BSocket_GetError(&o->sock));
        goto fail1;
    }
    
    // be informed of connection result
    BSocket_AddEventHandler(&o->sock, BSOCKET_CONNECT, (BSocket_handler)connect_handler, o);
    BSocket_EnableEvent(&o->sock, BSOCKET_CONNECT);
    
    // set state
    o->state = STATE_CONNECTING;
    
    DebugObject_Init(&o->d_obj);
    DebugError_Init(&o->d_err);
    
    return 1;
    
fail1:
    BSocket_Free(&o->sock);
fail0:
    return 0;
}

void ServerConnection_Free (ServerConnection *o)
{
    DebugError_Free(&o->d_err);
    DebugObject_Free(&o->d_obj);
    
    if (o->state > STATE_CONNECTING) {
        // allow freeing queue flows
        PacketPassPriorityQueue_PrepareFree(&o->output_queue);
        
        // free output user flow
        PacketPassPriorityQueueFlow_Free(&o->output_user_qflow);
        
        // free output local flow
        PacketProtoFlow_Free(&o->output_local_oflow);
        PacketPassPriorityQueueFlow_Free(&o->output_local_qflow);
        
        // free output common
        PacketPassPriorityQueue_Free(&o->output_queue);
        KeepaliveIO_Free(&o->output_keepaliveio);
        PacketStreamSender_Free(&o->output_sender);
        if (o->have_ssl) {
            PRStreamSink_Free(&o->output_sink.ssl);
        } else {
            StreamSocketSink_Free(&o->output_sink.plain);
        }
        
        // free output keep-alive branch
        PacketProtoEncoder_Free(&o->output_ka_encoder);
        SCKeepaliveSource_Free(&o->output_ka_zero);
        
        // free job
        BPending_Free(&o->start_job);
        
        // free input chain
        PacketProtoDecoder_Free(&o->input_decoder);
        PacketPassInterface_Free(&o->input_interface);
        if (o->have_ssl) {
            PRStreamSource_Free(&o->input_source.ssl);
        } else {
            StreamSocketSource_Free(&o->input_source.plain);
        }
        
        // free SSL
        if (o->have_ssl) {
            BPRFileDesc_Free(&o->ssl_bprfd);
            ASSERT_FORCE(PR_Close(o->ssl_prfd) == PR_SUCCESS)
        }
    }
    
    // free socket
    BSocket_Free(&o->sock);
}

int ServerConnection_IsReady (ServerConnection *o)
{
    DebugObject_Access(&o->d_obj);
    
    return (o->state == STATE_COMPLETE);
}

int ServerConnection_StartMessage (ServerConnection *o, void **data, peerid_t peer_id, int len)
{
    ASSERT(o->state == STATE_COMPLETE)
    ASSERT(o->output_local_packet_len == -1)
    ASSERT(len >= 0)
    ASSERT(len <= SC_MAX_MSGLEN)
    ASSERT(data || len == 0)
    DebugError_AssertNoError(&o->d_err);
    DebugObject_Access(&o->d_obj);
    
    uint8_t *packet;
    if (!start_packet(o, (void **)&packet, sizeof(struct sc_client_outmsg) + len)) {
        return 0;
    }
    
    struct sc_client_outmsg *msg = (struct sc_client_outmsg *)packet;
    msg->clientid = htol16(peer_id);
    
    if (data) {
        *data = packet + sizeof(struct sc_client_outmsg);
    }
    
    return 1;
}

void ServerConnection_EndMessage (ServerConnection *o)
{
    ASSERT(o->state == STATE_COMPLETE)
    ASSERT(o->output_local_packet_len >= 0)
    DebugError_AssertNoError(&o->d_err);
    DebugObject_Access(&o->d_obj);
    
    end_packet(o, SCID_OUTMSG);
}

PacketPassInterface * ServerConnection_GetSendInterface (ServerConnection *o)
{
    ASSERT(o->state == STATE_COMPLETE)
    DebugError_AssertNoError(&o->d_err);
    DebugObject_Access(&o->d_obj);
    
    return PacketPassPriorityQueueFlow_GetInput(&o->output_user_qflow);
}
