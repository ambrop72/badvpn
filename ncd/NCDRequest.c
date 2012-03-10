/**
 * @file NCDRequest.c
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

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include <misc/byteorder.h>
#include <misc/expstring.h>
#include <protocol/packetproto.h>
#include <protocol/requestproto.h>
#include <base/BLog.h>

#include "NCDRequest.h"

#include <generated/blog_channel_NCDRequest.h>

#define SEND_PAYLOAD_MTU 32768
#define RECV_PAYLOAD_MTU 32768

#define SEND_MTU (SEND_PAYLOAD_MTU + sizeof(struct requestproto_header))
#define RECV_MTU (RECV_PAYLOAD_MTU + sizeof(struct requestproto_header))

#define STATE_CONNECTING 1
#define STATE_CONNECTED 2

static int build_requestproto_packet (uint32_t request_id, uint32_t flags, NCDValue *payload_value, uint8_t **out_data, int *out_len);
static void report_finished (NCDRequest *o, int is_error);
static void connector_handler (NCDRequest *o, int is_error);
static void connection_handler (NCDRequest *o, int event);
static void decoder_handler_error (NCDRequest *o);
static void recv_if_handler_send (NCDRequest *o, uint8_t *data, int data_len);
static void send_sender_iface_handler_done (NCDRequest *o);

static int build_requestproto_packet (uint32_t request_id, uint32_t flags, NCDValue *payload_value, uint8_t **out_data, int *out_len)
{
    struct header {
        struct packetproto_header pp;
        struct requestproto_header rp;
    };
    
    ExpString str;
    if (!ExpString_Init(&str)) {
        BLog(BLOG_ERROR, "ExpString_Init failed");
        goto fail0;
    }
    
    if (!ExpString_AppendZeros(&str, sizeof(struct header))) {
        BLog(BLOG_ERROR, "ExpString_AppendBinary failed");
        goto fail1;
    }
    
    if (payload_value && !NCDValueGenerator_AppendGenerate(payload_value, &str)) {
        BLog(BLOG_ERROR, "NCDValueGenerator_AppendGenerate failed");
        goto fail1;
    }
    
    size_t len = ExpString_Length(&str);
    if (len > INT_MAX || len > PACKETPROTO_ENCLEN(SEND_MTU) || len - sizeof(struct packetproto_header) > UINT16_MAX) {
        BLog(BLOG_ERROR, "reply is too long");
        goto fail1;
    }
    
    uint8_t *packet = ExpString_Get(&str);
    
    struct header *header = (void *)packet;
    header->pp.len = htol16(len - sizeof(struct packetproto_header));
    header->rp.request_id = htol32(request_id);
    header->rp.flags = htol32(flags);
    
    *out_data = packet;
    *out_len = len;
    return 1;
    
fail1:
    ExpString_Free(&str);
fail0:
    return 0;
}

static void report_finished (NCDRequest *o, int is_error)
{
    DEBUGERROR(&o->d_err, o->handler_finished(o->user, is_error))
}

static void connector_handler (NCDRequest *o, int is_error)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == STATE_CONNECTING)
    
    // check error
    if (is_error) {
        BLog(BLOG_ERROR, "failed to connect to socket");
        goto fail0;
    }
    
    BPendingGroup *pg = BReactor_PendingGroup(o->reactor);
    
    // init connection
    if (!BConnection_Init(&o->con, BCONNECTION_SOURCE_CONNECTOR(&o->connector), o->reactor, o, (BConnection_handler)connection_handler)) {
        BLog(BLOG_ERROR, "BConnection_Init failed");
        goto fail0;
    }
    
    // init connection interfaces
    BConnection_SendAsync_Init(&o->con);
    BConnection_RecvAsync_Init(&o->con);
    StreamPassInterface *con_send_if = BConnection_SendAsync_GetIf(&o->con);
    StreamRecvInterface *con_recv_if = BConnection_RecvAsync_GetIf(&o->con);
    
    // init receive interface
    PacketPassInterface_Init(&o->recv_if, RECV_MTU, (PacketPassInterface_handler_send)recv_if_handler_send, o, pg);
    
    // init receive decoder
    if (!PacketProtoDecoder_Init(&o->recv_decoder, con_recv_if, &o->recv_if, pg, o, (PacketProtoDecoder_handler_error)decoder_handler_error)) {
        BLog(BLOG_ERROR, "PacketProtoDecoder_Init failed");
        goto fail1;
    }
    
    // init send sender
    PacketStreamSender_Init(&o->send_sender, con_send_if, PACKETPROTO_ENCLEN(SEND_MTU), pg);
    o->send_sender_iface = PacketStreamSender_GetInput(&o->send_sender);
    
    // init send interface
    PacketPassInterface_Sender_Init(o->send_sender_iface, (PacketPassInterface_handler_done)send_sender_iface_handler_done, o);
    
    // send request
    PacketPassInterface_Sender_Send(o->send_sender_iface, o->request_data, o->request_len);
    
    // set state connected
    o->state = STATE_CONNECTED;
    return;
    
fail1:
    PacketPassInterface_Free(&o->recv_if);
    BConnection_RecvAsync_Free(&o->con);
    BConnection_SendAsync_Free(&o->con);
    BConnection_Free(&o->con);
fail0:
    report_finished(o, 1);
}

static void connection_handler (NCDRequest *o, int event)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == STATE_CONNECTED)
    
    BLog(BLOG_ERROR, "connection error");
    
    report_finished(o, 1);
}

static void decoder_handler_error (NCDRequest *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == STATE_CONNECTED)
    
    BLog(BLOG_ERROR, "decoder error");
    
    report_finished(o, 1);
}

static void recv_if_handler_send (NCDRequest *o, uint8_t *data, int data_len)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == STATE_CONNECTED)
    ASSERT(!o->processing)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= RECV_MTU)
    
    if (data_len < sizeof(struct requestproto_header)) {
        BLog(BLOG_ERROR, "missing requestproto header");
        goto fail;
    }
    
    struct requestproto_header *header = (struct requestproto_header *)data;
    uint32_t request_id = ltoh32(header->request_id);
    uint32_t flags = ltoh32(header->flags);
    
    uint8_t *payload = data + sizeof(*header);
    int payload_len = data_len - sizeof(*header);
    
    if (request_id != o->request_id) {
        BLog(BLOG_ERROR, "invalid request ID");
        goto fail;
    }
    
    if (flags == REQUESTPROTO_REPLY_FLAG_DATA) {
        NCDValue value;
        if (!NCDValueParser_Parse(payload, payload_len, &value)) {
            BLog(BLOG_ERROR, "NCDValueParser_Parse failed");
            goto fail;
        }
        
        // set processing
        o->processing = 1;
        
        // call reply handler
        o->handler_reply(o->user, value);
        return;
    }
    
    if (flags == REQUESTPROTO_REPLY_FLAG_END) {
        if (payload_len != 0) {
            BLog(BLOG_ERROR, "end reply has non-empty payload");
            goto fail;
        }
        
        // call finished handler
        report_finished(o, 0);
        return;
    }
    
    BLog(BLOG_ERROR, "invalid requestproto flags");
    
fail:
    report_finished(o, 1);
}

static void send_sender_iface_handler_done (NCDRequest *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == STATE_CONNECTED)
}

int NCDRequest_Init (NCDRequest *o, const char *socket_path, NCDValue *payload_value, BReactor *reactor, void *user, NCDRequest_handler_finished handler_finished, NCDRequest_handler_reply handler_reply)
{
    ASSERT(socket_path)
    NCDValue_Type(payload_value);
    ASSERT(handler_finished)
    ASSERT(handler_reply)
    
    // init arguments
    o->reactor = reactor;
    o->user = user;
    o->handler_finished = handler_finished;
    o->handler_reply = handler_reply;
    
    // choose request ID
    o->request_id = 175;
    
    // build request
    if (!build_requestproto_packet(o->request_id, REQUESTPROTO_REQUEST_FLAG, payload_value, &o->request_data, &o->request_len)) {
        BLog(BLOG_ERROR, "failed to build request");
        goto fail0;
    }
    
    // init connector
    if (!BConnector_InitUnix(&o->connector, socket_path, reactor, o, (BConnector_handler)connector_handler)) {
        BLog(BLOG_ERROR, "BConnector_InitUnix failed");
        goto fail1;
    }
    
    // set state connecting
    o->state = STATE_CONNECTING;
    
    // set not processing
    o->processing = 0;
    
    DebugError_Init(&o->d_err, BReactor_PendingGroup(reactor));
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail1:
    free(o->request_data);
fail0:
    return 0;
}

void NCDRequest_Free (NCDRequest *o)
{
    DebugObject_Free(&o->d_obj);
    DebugError_Free(&o->d_err);
    
    if (o->state == STATE_CONNECTED) {
        PacketStreamSender_Free(&o->send_sender);
        PacketProtoDecoder_Free(&o->recv_decoder);
        PacketPassInterface_Free(&o->recv_if);
        BConnection_RecvAsync_Free(&o->con);
        BConnection_SendAsync_Free(&o->con);
        BConnection_Free(&o->con);
    }
    
    BConnector_Free(&o->connector);
    free(o->request_data);
}

void NCDRequest_Next (NCDRequest *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == STATE_CONNECTED)
    ASSERT(o->processing)
    
    // set not processing
    o->processing = 0;
    
    // accept received packet
    PacketPassInterface_Done(&o->recv_if);
}
