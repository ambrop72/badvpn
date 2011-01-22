/**
 * @file BSocksClient.c
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

#include <misc/byteorder.h>
#include <system/BLog.h>

#include <socksclient/BSocksClient.h>

#include <generated/blog_channel_BSocksClient.h>

#define STATE_CONNECTING 1
#define STATE_SENDING_HELLO 2
#define STATE_SENT_HELLO 3
#define STATE_SENDING_REQUEST 4
#define STATE_SENT_REQUEST 5
#define STATE_RECEIVED_REPLY_HEADER 6
#define STATE_UP 7

#define COMPONENT_SOURCE 1
#define COMPONENT_SINK 2

static void report_error (BSocksClient *o, int error);
static void init_control_io (BSocksClient *o);
static void free_control_io (BSocksClient *o);
static void init_up_io (BSocksClient *o);
static void free_up_io (BSocksClient *o);
static void start_receive (BSocksClient *o, uint8_t *dest, int total);
static void do_receive (BSocksClient *o);
static void error_handler (BSocksClient *o, int component, int code);
static void socket_error_handler (BSocksClient *o, int event);
static void connect_handler (BSocksClient *o, int event);
static void recv_handler_done (BSocksClient *o, int data_len);
static void send_handler_done (BSocksClient *o);

void report_error (BSocksClient *o, int error)
{
    DEBUGERROR(&o->d_err, o->handler(o->user, error))
}

void init_control_io (BSocksClient *o)
{
    // init receiving
    StreamSocketSource_Init(&o->control.recv_source, FlowErrorReporter_Create(&o->domain, COMPONENT_SOURCE), &o->sock, BReactor_PendingGroup(o->reactor));
    o->control.recv_if = StreamSocketSource_GetOutput(&o->control.recv_source);
    StreamRecvInterface_Receiver_Init(o->control.recv_if, (StreamRecvInterface_handler_done)recv_handler_done, o);
    
    // init sending
    StreamSocketSink_Init(&o->control.send_sink, FlowErrorReporter_Create(&o->domain, COMPONENT_SINK), &o->sock, BReactor_PendingGroup(o->reactor));
    PacketStreamSender_Init(&o->control.send_sender, StreamSocketSink_GetInput(&o->control.send_sink), sizeof(o->control.msg), BReactor_PendingGroup(o->reactor));
    o->control.send_if = PacketStreamSender_GetInput(&o->control.send_sender);
    PacketPassInterface_Sender_Init(o->control.send_if, (PacketPassInterface_handler_done)send_handler_done, o);
}

void free_control_io (BSocksClient *o)
{
    // free sending
    PacketStreamSender_Free(&o->control.send_sender);
    StreamSocketSink_Free(&o->control.send_sink);
    
    // free receiving
    StreamSocketSource_Free(&o->control.recv_source);
}

void init_up_io (BSocksClient *o)
{
    // init receiving
    StreamSocketSource_Init(&o->up.recv_source, FlowErrorReporter_Create(&o->domain, COMPONENT_SOURCE), &o->sock, BReactor_PendingGroup(o->reactor));
    
    // init sending
    StreamSocketSink_Init(&o->up.send_sink, FlowErrorReporter_Create(&o->domain, COMPONENT_SINK), &o->sock, BReactor_PendingGroup(o->reactor));
}

void free_up_io (BSocksClient *o)
{
    // free sending
    StreamSocketSink_Free(&o->up.send_sink);
    
    // free receiving
    StreamSocketSource_Free(&o->up.recv_source);
}

void start_receive (BSocksClient *o, uint8_t *dest, int total)
{
    ASSERT(total > 0)
    
    o->control.recv_dest = dest;
    o->control.recv_len = 0;
    o->control.recv_total = total;
    
    do_receive(o);
}

void do_receive (BSocksClient *o)
{
    ASSERT(o->control.recv_len < o->control.recv_total)
    
    StreamRecvInterface_Receiver_Recv(o->control.recv_if, o->control.recv_dest + o->control.recv_len, o->control.recv_total - o->control.recv_len);
}

void error_handler (BSocksClient* o, int component, int code)
{
    ASSERT(component == COMPONENT_SOURCE || component == COMPONENT_SINK)
    DebugObject_Access(&o->d_obj);
    
    if (o->state == STATE_UP && component == COMPONENT_SOURCE && code == STREAMSOCKETSOURCE_ERROR_CLOSED) {
        BLog(BLOG_DEBUG, "connection closed");
        
        report_error(o, BSOCKSCLIENT_EVENT_ERROR_CLOSED);
        return;
    }
    
    BLog(BLOG_NOTICE, "socket error (%d)", BSocket_GetError(&o->sock));
    
    report_error(o, BSOCKSCLIENT_EVENT_ERROR);
    return;
}

void socket_error_handler (BSocksClient *o, int event)
{
    ASSERT(event == BSOCKET_ERROR)
    DebugObject_Access(&o->d_obj);
    
    BLog(BLOG_NOTICE, "socket error event");
    
    report_error(o, BSOCKSCLIENT_EVENT_ERROR);
    return;
}

void connect_handler (BSocksClient *o, int event)
{
    ASSERT(event == BSOCKET_CONNECT)
    ASSERT(o->state == STATE_CONNECTING)
    DebugObject_Access(&o->d_obj);
    
    // remove event handler
    BSocket_RemoveEventHandler(&o->sock, BSOCKET_CONNECT);
    
    // check connect result
    int res = BSocket_GetConnectResult(&o->sock);
    if (res != 0) {
        BLog(BLOG_NOTICE, "connection failed (%d)", res);
        report_error(o, BSOCKSCLIENT_EVENT_ERROR);
        return;
    }
    
    BLog(BLOG_DEBUG, "connected");
    
    // init control I/O
    init_control_io(o);
    
    // send hello
    o->control.msg.client_hello.header.ver = hton8(SOCKS_VERSION);
    o->control.msg.client_hello.header.nmethods = hton8(1);
    o->control.msg.client_hello.method.method = hton8(SOCKS_METHOD_NO_AUTHENTICATION_REQUIRED);
    PacketPassInterface_Sender_Send(o->control.send_if, (uint8_t *)&o->control.msg.client_hello, sizeof(o->control.msg.client_hello));
    
    // set state
    o->state = STATE_SENDING_HELLO;
}

void recv_handler_done (BSocksClient *o, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= o->control.recv_total - o->control.recv_len)
    DebugObject_Access(&o->d_obj);
    
    o->control.recv_len += data_len;
    
    if (o->control.recv_len < o->control.recv_total) {
        do_receive(o);
        return;
    }
    
    switch (o->state) {
        case STATE_SENT_HELLO: {
            BLog(BLOG_DEBUG, "received hello");
            
            if (ntoh8(o->control.msg.server_hello.ver != SOCKS_VERSION)) {
                BLog(BLOG_NOTICE, "wrong version");
                report_error(o, BSOCKSCLIENT_EVENT_ERROR);
                return;
            }
            
            if (ntoh8(o->control.msg.server_hello.method != SOCKS_METHOD_NO_AUTHENTICATION_REQUIRED)) {
                BLog(BLOG_NOTICE, "wrong method");
                report_error(o, BSOCKSCLIENT_EVENT_ERROR);
                return;
            }
            
            // send request
            o->control.msg.request.header.ver = hton8(SOCKS_VERSION);
            o->control.msg.request.header.cmd = hton8(SOCKS_CMD_CONNECT);
            o->control.msg.request.header.rsv = hton8(0);
            int len = sizeof(o->control.msg.request.header);
            switch (o->dest_addr.type) {
                case BADDR_TYPE_IPV4:
                    o->control.msg.request.header.atyp = hton8(SOCKS_ATYP_IPV4);
                    o->control.msg.request.addr.ipv4.addr = o->dest_addr.ipv4.ip;
                    o->control.msg.request.addr.ipv4.port = o->dest_addr.ipv4.port;
                    len += sizeof(o->control.msg.request.addr.ipv4);
                    break;
                case BADDR_TYPE_IPV6:
                    o->control.msg.request.header.atyp = hton8(SOCKS_ATYP_IPV6);
                    memcpy(o->control.msg.request.addr.ipv6.addr, o->dest_addr.ipv6.ip, sizeof(o->dest_addr.ipv6.ip));
                    o->control.msg.request.addr.ipv6.port = o->dest_addr.ipv6.port;
                    len += sizeof(o->control.msg.request.addr.ipv6);
                    break;
                default:
                    ASSERT(0);
            }
            PacketPassInterface_Sender_Send(o->control.send_if, (uint8_t *)&o->control.msg.request, len);
            
            // set state
            o->state = STATE_SENDING_REQUEST;
        } break;
        
        case STATE_SENT_REQUEST: {
            BLog(BLOG_DEBUG, "received reply header");
            
            if (ntoh8(o->control.msg.reply.header.ver) != SOCKS_VERSION) {
                BLog(BLOG_NOTICE, "wrong version");
                report_error(o, BSOCKSCLIENT_EVENT_ERROR);
                return;
            }
            
            if (ntoh8(o->control.msg.reply.header.rep) != SOCKS_REP_SUCCEEDED) {
                BLog(BLOG_NOTICE, "reply not successful");
                report_error(o, BSOCKSCLIENT_EVENT_ERROR);
                return;
            }
            
            int addr_len;
            switch (ntoh8(o->control.msg.reply.header.atyp)) {
                case SOCKS_ATYP_IPV4:
                    addr_len = sizeof(o->control.msg.reply.addr.ipv4);
                    break;
                case SOCKS_ATYP_IPV6:
                    addr_len = sizeof(o->control.msg.reply.addr.ipv6);
                    break;
                default:
                    BLog(BLOG_NOTICE, "reply has unknown address type");
                    report_error(o, BSOCKSCLIENT_EVENT_ERROR);
                    return;
            }
            
            // receive the rest of the reply
            start_receive(o, (uint8_t *)&o->control.msg.reply.addr, addr_len);
            
            // set state
            o->state = STATE_RECEIVED_REPLY_HEADER;
        } break;
        
        case STATE_RECEIVED_REPLY_HEADER: {
            BLog(BLOG_DEBUG, "received reply rest");
            
            // free control I/O
            free_control_io(o);
            
            // init up I/O
            init_up_io(o);
            
            // set state
            o->state = STATE_UP;
            
            // call handler
            o->handler(o->user, BSOCKSCLIENT_EVENT_UP);
            return;
        } break;
        
        default:
            ASSERT(0);
    }
}

void send_handler_done (BSocksClient *o)
{
    DebugObject_Access(&o->d_obj);
    
    switch (o->state) {
        case STATE_SENDING_HELLO: {
            BLog(BLOG_DEBUG, "sent hello");
            
            // receive hello
            start_receive(o, (uint8_t *)&o->control.msg.server_hello, sizeof(o->control.msg.server_hello));
            
            // set state
            o->state = STATE_SENT_HELLO;
        } break;
        
        case STATE_SENDING_REQUEST: {
            BLog(BLOG_DEBUG, "sent request");
            
            // receive reply header
            start_receive(o, (uint8_t *)&o->control.msg.reply.header, sizeof(o->control.msg.reply.header));
            
            // set state
            o->state = STATE_SENT_REQUEST;
        } break;
        
        default:
            ASSERT(0);
    }
}

int BSocksClient_Init (BSocksClient *o, BAddr server_addr, BAddr dest_addr, BSocksClient_handler handler, void *user, BReactor *reactor)
{
    ASSERT(dest_addr.type == BADDR_TYPE_IPV4 || dest_addr.type == BADDR_TYPE_IPV6)
    
    // init arguments
    o->dest_addr = dest_addr;
    o->handler = handler;
    o->user = user;
    o->reactor = reactor;
    
    // init error domain
    FlowErrorDomain_Init(&o->domain, (FlowErrorDomain_handler)error_handler, o);
    
    // init socket
    if (BSocket_Init(&o->sock, o->reactor, server_addr.type, BSOCKET_TYPE_STREAM) < 0) {
        BLog(BLOG_NOTICE, "BSocket_Init failed");
        goto fail0;
    }
    
    // connect socket
    if (BSocket_Connect(&o->sock, &server_addr) >= 0 || BSocket_GetError(&o->sock) != BSOCKET_ERROR_IN_PROGRESS) {
        BLog(BLOG_NOTICE, "BSocket_Connect failed");
        goto fail1;
    }
    
    // setup error event
    BSocket_AddEventHandler(&o->sock, BSOCKET_ERROR, (BSocket_handler)socket_error_handler, o);
    BSocket_EnableEvent(&o->sock, BSOCKET_ERROR);
    
    // setup connect event
    BSocket_AddEventHandler(&o->sock, BSOCKET_CONNECT, (BSocket_handler)connect_handler, o);
    BSocket_EnableEvent(&o->sock, BSOCKET_CONNECT);
    
    // set state
    o->state = STATE_CONNECTING;
    
    DebugObject_Init(&o->d_obj);
    DebugError_Init(&o->d_err, BReactor_PendingGroup(o->reactor));
    
    return 1;
    
fail1:
    BSocket_Free(&o->sock);
fail0:
    return 0;
}

void BSocksClient_Free (BSocksClient *o)
{
    DebugError_Free(&o->d_err);
    DebugObject_Free(&o->d_obj);
    
    if (o->state == STATE_UP) {
        // free up I/O
        free_up_io(o);
    }
    else if (o->state != STATE_CONNECTING) {
        ASSERT(o->state == STATE_SENDING_HELLO || o->state == STATE_SENT_HELLO ||
               o->state == STATE_SENDING_REQUEST || o->state == STATE_SENT_REQUEST ||
               o->state == STATE_RECEIVED_REPLY_HEADER
        )
        // free control I/O
        free_control_io(o);
    }
    
    // free socket
    BSocket_Free(&o->sock);
}

StreamPassInterface * BSocksClient_GetSendInterface (BSocksClient *o)
{
    ASSERT(o->state == STATE_UP)
    DebugObject_Access(&o->d_obj);
    
    return StreamSocketSink_GetInput(&o->up.send_sink);
}

StreamRecvInterface * BSocksClient_GetRecvInterface (BSocksClient *o)
{
    ASSERT(o->state == STATE_UP)
    DebugObject_Access(&o->d_obj);
    
    return StreamSocketSource_GetOutput(&o->up.recv_source);
}
