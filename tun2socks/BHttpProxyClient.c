/**
 *
 * @file BHttpProxyClient.c
 * @author B. Blechschmidt, based on the SocksClient implementation by Ambroz Bizjak
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

#include <string.h>

#include <misc/byteorder.h>
#include <misc/balloc.h>
#include <base/BLog.h>

#include "BHttpProxyClient.h"

#include <generated/blog_channel_BHttpProxyClient.h>

#define STATE_CONNECTING 1
#define STATE_CONNECTED_HANDLER 2
#define STATE_SENDING_REQUEST 3
#define STATE_SENT_REQUEST 4
#define STATE_RECEIVED_REPLY_HEADER 5
#define STATE_UP 6

#define IPV4_STR_LEN 15
#define IPV6_STR_LEN 39

static void report_error (BHttpProxyClient *o, int error);
static void init_control_io (BHttpProxyClient *o);
static void free_control_io (BHttpProxyClient *o);
static void init_up_io (BHttpProxyClient *o);
static void free_up_io (BHttpProxyClient *o);
static int reserve_buffer (BHttpProxyClient *o, bsize_t size);
static void start_receive (BHttpProxyClient *o, uint8_t *dest, int total);
static void do_receive (BHttpProxyClient *o);
static void connector_handler (BHttpProxyClient* o, int is_error);
static void connection_handler (BHttpProxyClient* o, int event);
static void continue_job_handler (BHttpProxyClient *o);
static void recv_handler_done (BHttpProxyClient *o, int data_len);
static void send_handler_done (BHttpProxyClient *o);
static void send_connect (BHttpProxyClient *o);
static void base64_encode(const unsigned char *raw, char *encoded);
static int init_auth(BHttpProxyClient *o, const char *username, const char *password);

void base64_encode(const unsigned char *raw, char *encoded)
{
    const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t char_count = 0;
    unsigned char chars[3];
    size_t i;

    while(1) {
        if (*raw != 0) {
            chars[char_count++] = *raw++;
        }
        if (char_count == 3 || *raw == 0) {
            if (char_count == 0) {
                break;
            }

            for (i = 2; i >= char_count; i--) {
                chars[i] = 0;
            }

            *encoded++ = alphabet[chars[0] >> 2u];
            *encoded++ = alphabet[(unsigned char)((chars[0] & 0x03u) << 4u) | (unsigned char)(chars[1] >> 4u)];
            *encoded++ = alphabet[(unsigned char)((chars[1] & 0x0Fu) << 2u) | (unsigned char)(chars[2] >> 6u)];
            *encoded++ = alphabet[chars[2] & 0x3Fu];

            for (i = 0; i < 3 - char_count; i++) {
                *(encoded - i - 1) = '=';
            }

            if (*raw == 0) {
                break;
            }
            char_count = 0;
        }
    }
    *encoded = '\0';
}


void report_error (BHttpProxyClient *o, int error)
{
    DEBUGERROR(&o->d_err, o->handler(o->user, error))
}

void init_control_io (BHttpProxyClient *o)
{
    // init receiving
    BConnection_RecvAsync_Init(&o->con);
    o->control.recv_if = BConnection_RecvAsync_GetIf(&o->con);
    StreamRecvInterface_Receiver_Init(o->control.recv_if, (StreamRecvInterface_handler_done)recv_handler_done, o);
    
    // init sending
    BConnection_SendAsync_Init(&o->con);
    PacketStreamSender_Init(&o->control.send_sender, BConnection_SendAsync_GetIf(&o->con), INT_MAX, BReactor_PendingGroup(o->reactor));
    o->control.send_if = PacketStreamSender_GetInput(&o->control.send_sender);
    PacketPassInterface_Sender_Init(o->control.send_if, (PacketPassInterface_handler_done)send_handler_done, o);
}

void free_control_io (BHttpProxyClient *o)
{
    // free sending
    PacketStreamSender_Free(&o->control.send_sender);
    BConnection_SendAsync_Free(&o->con);
    
    // free receiving
    BConnection_RecvAsync_Free(&o->con);
}

void init_up_io (BHttpProxyClient *o)
{
    // init receiving
    BConnection_RecvAsync_Init(&o->con);
    
    // init sending
    BConnection_SendAsync_Init(&o->con);
}

void free_up_io (BHttpProxyClient *o)
{
    // free sending
    BConnection_SendAsync_Free(&o->con);
    
    // free receiving
    BConnection_RecvAsync_Free(&o->con);
}

int reserve_buffer (BHttpProxyClient *o, bsize_t size)
{
    if (size.is_overflow) {
        BLog(BLOG_ERROR, "size overflow");
        return 0;
    }
    
    char *buffer = (char *)BRealloc(o->buffer, size.value);
    if (!buffer) {
        BLog(BLOG_ERROR, "BRealloc failed");
        return 0;
    }
    
    o->buffer = buffer;
    
    return 1;
}

void start_receive (BHttpProxyClient *o, uint8_t *dest, int total)
{
    ASSERT(total > 0)
    
    o->control.recv_dest = dest;
    o->control.recv_len = 0;
    o->control.recv_total = total;
    
    do_receive(o);
}

void do_receive (BHttpProxyClient *o)
{
    ASSERT(o->control.recv_len < o->control.recv_total)
    
    StreamRecvInterface_Receiver_Recv(o->control.recv_if, o->control.recv_dest + o->control.recv_len, o->control.recv_total - o->control.recv_len);
}

void connector_handler (BHttpProxyClient* o, int is_error)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == STATE_CONNECTING)
    
    // check connection result
    if (is_error) {
        BLog(BLOG_ERROR, "connection failed");
        goto fail0;
    }
    
    // init connection
    if (!BConnection_Init(&o->con, BConnection_source_connector(&o->connector), o->reactor, o, (BConnection_handler)connection_handler)) {
        BLog(BLOG_ERROR, "BConnection_Init failed");
        goto fail0;
    }
    
    BLog(BLOG_DEBUG, "connected");
    
    // init control I/O
    init_control_io(o);
    
    // go to STATE_CONNECTED_HANDLER and set the continue job in order to continue
    // in continue_job_handler
    o->state = STATE_CONNECTED_HANDLER;
    BPending_Set(&o->continue_job);

    // call the handler with the connected event
    o->handler(o->user, BHTTPPROXYCLIENT_EVENT_CONNECTED);
    return;
    
fail0:
    report_error(o, BHTTPPROXYCLIENT_EVENT_ERROR);
}

void connection_handler (BHttpProxyClient* o, int event)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state != STATE_CONNECTING)
    
    if (o->state == STATE_UP && event == BCONNECTION_EVENT_RECVCLOSED) {
        report_error(o, BHTTPPROXYCLIENT_EVENT_ERROR_CLOSED);
        return;
    }
    
    report_error(o, BHTTPPROXYCLIENT_EVENT_ERROR);
}

void continue_job_handler (BHttpProxyClient *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == STATE_CONNECTED_HANDLER)

    send_connect(o);
}

void recv_handler_done (BHttpProxyClient *o, int data_len)
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
        case STATE_SENT_REQUEST: {
            BLog(BLOG_DEBUG, "received reply header");

            if (memcmp(o->buffer, "HTTP/1.1 2", sizeof("HTTP/1.1 2") - 1) != 0 || o->buffer[12] != ' ') {
                BLog(BLOG_NOTICE, "invalid HTTP response");
                goto fail;
            }
            
            // receive the rest of the reply
            start_receive(o, (uint8_t *)o->buffer, 1);
            
            // set state
            o->state = STATE_RECEIVED_REPLY_HEADER;
            o->crlf_state = 0;
        } break;

        case STATE_RECEIVED_REPLY_HEADER: {
            if (o->buffer[0] == '\n') {
                o->crlf_state++;
            } else if (o->buffer[0] != '\r') {
                o->crlf_state = 0;
            }

            if (o->crlf_state < 2) {
                start_receive(o, (uint8_t *)o->buffer, 1);
                return;
            }
            // free buffer
            BFree(o->buffer);
            o->buffer = NULL;

            // free control I/O
            free_control_io(o);

            // init up I/O
            // We anyway don't allow the user to use these interfaces in that case.
            init_up_io(o);

            // set state
            o->state = STATE_UP;

            // call handler
            o->handler(o->user, BHTTPPROXYCLIENT_EVENT_UP);
            return;
        }
        default:
            ASSERT(0);
    }
    
    return;
    
fail:
    report_error(o, BHTTPPROXYCLIENT_EVENT_ERROR);
}

void send_handler_done (BHttpProxyClient *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->buffer)
    
    switch (o->state) {
        case STATE_SENDING_REQUEST: {
            BLog(BLOG_DEBUG, "sent request");
            
            // allocate buffer for receiving reply
            bsize_t size;
            size.value = 1024;
            size.is_overflow = 0;
            if (!reserve_buffer(o, size)) {
                goto fail;
            }
            
            // receive reply header
            start_receive(o, (uint8_t *)o->buffer, sizeof("HTTP/1.1 200 ") - 1);
            
            // set state
            o->state = STATE_SENT_REQUEST;
        } break;
        
        default:
            ASSERT(0);
    }
    
    return;
    
fail:
    report_error(o, BHTTPPROXYCLIENT_EVENT_ERROR);
}

void send_connect (BHttpProxyClient *o)
{
    ASSERT(o->dest_addr.type = BADDR_TYPE_IPV4 || o->dest_addr.type == BADDR_TYPE_IPV6)

    // allocate request buffer
    bsize_t size = bsize_fromsize(sizeof("CONNECT  HTTP/1.1\r\nHost: \r\n\r\n"));
    size = bsize_add(size, bsize_fromsize(BADDR_MAX_PRINT_LEN));
    size = bsize_add(size, bsize_fromsize(BADDR_MAX_PRINT_LEN));
    if (o->headers) {
        size = bsize_add(size, bsize_fromsize(strlen(o->headers)));
    }

    if (!reserve_buffer(o, size)) {
        report_error(o, BHTTPPROXYCLIENT_EVENT_ERROR);
        return;
    }

    memcpy(o->buffer, "CONNECT ", sizeof("CONNECT "));
    BAddr_Print(&o->dest_addr, o->buffer + strlen(o->buffer));
    sprintf(o->buffer + strlen(o->buffer), " HTTP/1.1\r\nHost: ");
    BAddr_Print(&o->dest_addr, o->buffer + strlen(o->buffer));
    sprintf(o->buffer + strlen(o->buffer), "\r\n%s\r\n", o->headers ? o->headers : "");

    size = bsize_fromsize(strlen(o->buffer));

    PacketPassInterface_Sender_Send(o->control.send_if, (uint8_t *)o->buffer, size.value);
    
    // set state
    o->state = STATE_SENDING_REQUEST;
}

int BHttpProxyClient_Init (BHttpProxyClient *o, BAddr server_addr,
    const char *username, const char *password, BAddr dest_addr,
    BHttpProxyClient_handler handler, void *user, BReactor *reactor)
{
    ASSERT(!BAddr_IsInvalid(&server_addr))
    
    // init arguments
    if (!init_auth(o, username, password)) {
        BLog(BLOG_ERROR, "Failed to allocate authentication buffer");
        return 0;
    }
    o->dest_addr = dest_addr;
    o->handler = handler;
    o->user = user;
    o->reactor = reactor;
    
    // set no buffer
    o->buffer = NULL;

    // init continue_job
    BPending_Init(&o->continue_job, BReactor_PendingGroup(o->reactor),
        (BPending_handler)continue_job_handler, o);
    
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
    BPending_Free(&o->continue_job);
    return 0;
}

int init_auth(BHttpProxyClient *o, const char *username, const char *password)
{
    o->headers = NULL;
    if (username == NULL || password == NULL) {
        return 1;
    }

    // Allocate buffer holding username:password (plus terminator)
    bsize_t auth_buf_size = bsize_fromsize(strlen(username));
    auth_buf_size = bsize_add(auth_buf_size, bsize_fromsize(strlen(password)));
    auth_buf_size = bsize_add(auth_buf_size, bsize_fromsize(2));
    if (auth_buf_size.is_overflow) {
        return 0;
    }
    char *auth_buf = BAlloc(auth_buf_size.value);
    if (!auth_buf) {
        return 0;
    }
    snprintf(auth_buf, auth_buf_size.value, "%s:%s", username, password);

    bsize_t header_buf_size = auth_buf_size;
    // Add one only due to terminator, dividing by 3 afterwards is then equivalent to dividing and ceiling
    header_buf_size = bsize_add(header_buf_size, bsize_fromsize(1));
    header_buf_size.value /= 3;
    header_buf_size = bsize_mul(header_buf_size, bsize_fromsize(4));
    bsize_t base64_size = header_buf_size; // Space needed for base64: ceil(len('username:password') / 3) * 4
    header_buf_size = bsize_add(header_buf_size, bsize_fromsize(sizeof("Proxy-Authorization: Basic \r\n")));
    printf("%zu", header_buf_size.value);

    if (header_buf_size.is_overflow) {
        goto fail_init_auth;
    }

    o->headers = BAlloc(header_buf_size.value);

    if (!o->headers) {
        goto fail_init_auth;
    }
    memcpy(o->headers, "Proxy-Authorization: Basic ", sizeof("Proxy-Authorization: Basic "));
    base64_encode((unsigned char*)auth_buf, o->headers + sizeof("Proxy-Authorization: Basic ") - 1);
    memcpy(o->headers + sizeof("Proxy-Authorization: Basic ") - 1 + base64_size.value, "\r\n\0", 3);

    BFree(auth_buf);
    return 1;

fail_init_auth:
    BFree(auth_buf);
    return 0;
}

void BHttpProxyClient_Free (BHttpProxyClient *o)
{
    DebugObject_Free(&o->d_obj);
    DebugError_Free(&o->d_err);
    
    if (o->state != STATE_CONNECTING) {
        if (o->state == STATE_UP) {
            // free up I/O
            free_up_io(o);
        } else {
            // free control I/O
            free_control_io(o);
        }
        
        // free connection
        BConnection_Free(&o->con);
    }
    
    // free connector
    BConnector_Free(&o->connector);
    
    // free continue job
    BPending_Free(&o->continue_job);

    // free buffer
    if (o->buffer) {
        BFree(o->buffer);
    }

    if (o->headers) {
        BFree(o->headers);
    }
}

StreamPassInterface * BHttpProxyClient_GetSendInterface (BHttpProxyClient *o)
{
    ASSERT(o->state == STATE_UP)
    DebugObject_Access(&o->d_obj);
    
    return BConnection_SendAsync_GetIf(&o->con);
}

StreamRecvInterface * BHttpProxyClient_GetRecvInterface (BHttpProxyClient *o)
{
    ASSERT(o->state == STATE_UP)
    DebugObject_Access(&o->d_obj);
    
    return BConnection_RecvAsync_GetIf(&o->con);
}
