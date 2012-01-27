/**
 * @file BSocksClient.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <misc/byteorder.h>
#include <base/BLog.h>

#include <socksclient/BSocksClient.h>

#include <generated/blog_channel_BSocksClient.h>

#define STATE_CONNECTING 1
#define STATE_SENDING_HELLO 2
#define STATE_SENT_HELLO 3
#define STATE_SENDING_REQUEST 4
#define STATE_SENT_REQUEST 5
#define STATE_RECEIVED_REPLY_HEADER 6
#define STATE_UP 7

static void report_error (BSocksClient *o, int error);
static void init_control_io (BSocksClient *o);
static void free_control_io (BSocksClient *o);
static void init_up_io (BSocksClient *o);
static void free_up_io (BSocksClient *o);
static void start_receive (BSocksClient *o, uint8_t *dest, int total);
static void do_receive (BSocksClient *o);
static void connector_handler (BSocksClient* o, int is_error);
static void connection_handler (BSocksClient* o, int event);
static void recv_handler_done (BSocksClient *o, int data_len);
static void send_handler_done (BSocksClient *o);

void report_error (BSocksClient *o, int error)
{
    DEBUGERROR(&o->d_err, o->handler(o->user, error))
}

void init_control_io (BSocksClient *o)
{
    // init receiving
    BConnection_RecvAsync_Init(&o->con);
    o->control.recv_if = BConnection_RecvAsync_GetIf(&o->con);
    StreamRecvInterface_Receiver_Init(o->control.recv_if, (StreamRecvInterface_handler_done)recv_handler_done, o);
    
    // init sending
    BConnection_SendAsync_Init(&o->con);
    PacketStreamSender_Init(&o->control.send_sender, BConnection_SendAsync_GetIf(&o->con), sizeof(o->control.msg), BReactor_PendingGroup(o->reactor));
    o->control.send_if = PacketStreamSender_GetInput(&o->control.send_sender);
    PacketPassInterface_Sender_Init(o->control.send_if, (PacketPassInterface_handler_done)send_handler_done, o);
}

void free_control_io (BSocksClient *o)
{
    // free sending
    PacketStreamSender_Free(&o->control.send_sender);
    BConnection_SendAsync_Free(&o->con);
    
    // free receiving
    BConnection_RecvAsync_Free(&o->con);
}

void init_up_io (BSocksClient *o)
{
    // init receiving
    BConnection_RecvAsync_Init(&o->con);
    
    // init sending
    BConnection_SendAsync_Init(&o->con);
}

void free_up_io (BSocksClient *o)
{
    // free sending
    BConnection_SendAsync_Free(&o->con);
    
    // free receiving
    BConnection_RecvAsync_Free(&o->con);
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

void connector_handler (BSocksClient* o, int is_error)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == STATE_CONNECTING)
    
    // check connection result
    if (is_error) {
        BLog(BLOG_ERROR, "connection failed");
        goto fail0;
    }
    
    // init connection
    if (!BConnection_Init(&o->con, BCONNECTION_SOURCE_CONNECTOR(&o->connector), o->reactor, o, (BConnection_handler)connection_handler)) {
        BLog(BLOG_ERROR, "BConnection_Init failed");
        goto fail0;
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
    
    return;
    
fail0:
    report_error(o, BSOCKSCLIENT_EVENT_ERROR);
    return;
}

void connection_handler (BSocksClient* o, int event)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state != STATE_CONNECTING)
    
    if (o->state == STATE_UP && event == BCONNECTION_EVENT_RECVCLOSED) {
        report_error(o, BSOCKSCLIENT_EVENT_ERROR_CLOSED);
        return;
    }
    
    report_error(o, BSOCKSCLIENT_EVENT_ERROR);
    return;
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
    ASSERT(!BAddr_IsInvalid(&server_addr))
    ASSERT(dest_addr.type == BADDR_TYPE_IPV4 || dest_addr.type == BADDR_TYPE_IPV6)
    
    // init arguments
    o->dest_addr = dest_addr;
    o->handler = handler;
    o->user = user;
    o->reactor = reactor;
    
    // init connector
    if (!BConnector_Init(&o->connector, server_addr, o->reactor, o, (BConnector_handler)connector_handler)) {
        BLog(BLOG_ERROR, "BConnector_Init failed");
        goto fail0;
    }
    
    // set state
    o->state = STATE_CONNECTING;
    
    DebugError_Init(&o->d_err, BReactor_PendingGroup(o->reactor));
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail0:
    return 0;
}

void BSocksClient_Free (BSocksClient *o)
{
    DebugObject_Free(&o->d_obj);
    DebugError_Free(&o->d_err);
    
    if (o->state != STATE_CONNECTING) {
        if (o->state == STATE_UP) {
            // free up I/O
            free_up_io(o);
        } else {
            ASSERT(o->state == STATE_SENDING_HELLO || o->state == STATE_SENT_HELLO ||
                o->state == STATE_SENDING_REQUEST || o->state == STATE_SENT_REQUEST ||
                o->state == STATE_RECEIVED_REPLY_HEADER
            )
            // free control I/O
            free_control_io(o);
        }
        
        // free connection
        BConnection_Free(&o->con);
    }
    
    // free connector
    BConnector_Free(&o->connector);
}

StreamPassInterface * BSocksClient_GetSendInterface (BSocksClient *o)
{
    ASSERT(o->state == STATE_UP)
    DebugObject_Access(&o->d_obj);
    
    return BConnection_SendAsync_GetIf(&o->con);
}

StreamRecvInterface * BSocksClient_GetRecvInterface (BSocksClient *o)
{
    ASSERT(o->state == STATE_UP)
    DebugObject_Access(&o->d_obj);
    
    return BConnection_RecvAsync_GetIf(&o->con);
}
