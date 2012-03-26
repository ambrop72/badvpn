/**
 * @file NCDRequestClient.c
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

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <inttypes.h>

#include <misc/byteorder.h>
#include <misc/expstring.h>
#include <misc/offset.h>
#include <protocol/packetproto.h>
#include <protocol/requestproto.h>
#include <base/BLog.h>

#include "NCDRequestClient.h"

#include <generated/blog_channel_NCDRequestClient.h>

#define SEND_PAYLOAD_MTU 32768
#define RECV_PAYLOAD_MTU 32768

#define SEND_MTU (SEND_PAYLOAD_MTU + sizeof(struct requestproto_header))
#define RECV_MTU (RECV_PAYLOAD_MTU + sizeof(struct requestproto_header))

#define CSTATE_CONNECTING 1
#define CSTATE_CONNECTED 2

#define RSTATE_SENDING_REQUEST 1
#define RSTATE_READY 2
#define RSTATE_SENDING_REQUEST_ABORT 3
#define RSTATE_SENDING_ABORT 4
#define RSTATE_WAITING_END 5
#define RSTATE_DEAD_SENDING 6

static int uint32_comparator (void *unused, void *vv1, void *vv2);
static void report_error (NCDRequestClient *o);
static void request_report_finished (NCDRequestClientRequest *o, int is_error);
static void connector_handler (NCDRequestClient *o, int is_error);
static void connection_handler (NCDRequestClient *o, int event);
static void decoder_handler_error (NCDRequestClient *o);
static void recv_if_handler_send (NCDRequestClient *o, uint8_t *data, int data_len);
static struct NCDRequestClient_req * find_req (NCDRequestClient *o, uint32_t request_id);
static int get_free_request_id (NCDRequestClient *o, uint32_t *out);
static int build_requestproto_packet (uint32_t request_id, uint32_t type, NCDValue *payload_value, uint8_t **out_data, int *out_len);
static void build_nodata_packet (uint32_t request_id, uint32_t type, uint8_t *data, int *out_len);
static int req_is_aborted (struct NCDRequestClient_req *req);
static void req_abort (struct NCDRequestClient_req *req);
static void req_free (struct NCDRequestClient_req *req);
static void req_send_abort (struct NCDRequestClient_req *req);
static void req_qflow_send_iface_handler_done (struct NCDRequestClient_req *req);

static int uint32_comparator (void *unused, void *vv1, void *vv2)
{
    uint32_t *v1 = vv1;
    uint32_t *v2 = vv2;
    return (*v1 > *v2) - (*v1 < *v2);
}

static void report_error (NCDRequestClient *o)
{
    DEBUGERROR(&o->d_err, o->handler_error(o->user))
}

static void request_report_finished (NCDRequestClientRequest *o, int is_error)
{
    o->req = NULL;
    
    DEBUGERROR(&o->d_err, o->handler_finished(o->user, is_error))
}

static void connector_handler (NCDRequestClient *o, int is_error)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == CSTATE_CONNECTING)
    
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
    
    // init send queue
    PacketPassFifoQueue_Init(&o->send_queue, PacketStreamSender_GetInput(&o->send_sender), pg);
    
    // set state connected
    o->state = CSTATE_CONNECTED;
    
    // call connected handler
    o->handler_connected(o->user);
    return;
    
fail1:
    PacketPassInterface_Free(&o->recv_if);
    BConnection_RecvAsync_Free(&o->con);
    BConnection_SendAsync_Free(&o->con);
    BConnection_Free(&o->con);
fail0:
    report_error(o);
}

static void connection_handler (NCDRequestClient *o, int event)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == CSTATE_CONNECTED)
    
    BLog(BLOG_ERROR, "connection error");
    
    report_error(o);
}

static void decoder_handler_error (NCDRequestClient *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == CSTATE_CONNECTED)
    
    BLog(BLOG_ERROR, "decoder error");
    
    report_error(o);
}

static void recv_if_handler_send (NCDRequestClient *o, uint8_t *data, int data_len)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == CSTATE_CONNECTED)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= RECV_MTU)
    
    // accept packet
    PacketPassInterface_Done(&o->recv_if);
    
    if (data_len < sizeof(struct requestproto_header)) {
        BLog(BLOG_ERROR, "missing requestproto header");
        goto fail;
    }
    
    struct requestproto_header *header = (struct requestproto_header *)data;
    uint32_t request_id = ltoh32(header->request_id);
    uint32_t type = ltoh32(header->type);
    
    uint8_t *payload = data + sizeof(*header);
    int payload_len = data_len - sizeof(*header);
    
    // find request
    struct NCDRequestClient_req *req = find_req(o, request_id);
    if (!req) {
        BLog(BLOG_ERROR, "received packet with unknown request ID");
        goto fail;
    }
    
    switch (type) {
        case REQUESTPROTO_TYPE_SERVER_REPLY: {
            switch (o->state) {
                case RSTATE_READY: {
                    // parse payload
                    NCDValue payload_value;
                    if (!NCDValueParser_Parse((char *)payload, payload_len, &payload_value)) {
                        BLog(BLOG_ERROR, "failed to parse reply payload");
                        goto fail;
                    }
                    
                    // call reply handler
                    req->creq->handler_reply(req->creq->user, payload_value);
                    return;
                } break;
                
                case RSTATE_SENDING_ABORT:
                case RSTATE_WAITING_END:
                    return;
                
                default:
                    BLog(BLOG_ERROR, "received unexpected reply");
                    goto fail;
            }
        } break;
        
        case REQUESTPROTO_TYPE_SERVER_FINISHED:
        case REQUESTPROTO_TYPE_SERVER_ERROR: {
            if (payload_len != 0) {
                BLog(BLOG_ERROR, "finshed/aborted message has non-empty payload");
                goto fail;
            }
            
            NCDRequestClientRequest *creq = req->creq;
            req->creq = NULL;
            
            switch (req->state) {
                case RSTATE_SENDING_ABORT: {
                    // set state dying send
                    req->state = RSTATE_DEAD_SENDING;
                } break;
                
                case RSTATE_WAITING_END:
                case RSTATE_READY: {
                    // free req
                    req_free(req);
                } break;
                
                default:
                    BLog(BLOG_ERROR, "received unexpected finished/aborted");
                    goto fail;
            }
            
            // report finished
            if (creq) {
                request_report_finished(creq, type == REQUESTPROTO_TYPE_SERVER_ERROR);
            }
            return;
        } break;
        
        default:
            BLog(BLOG_ERROR, "received invalid message type");
            goto fail;
    }
    
    ASSERT(0)
    
fail:
    report_error(o);
}

static struct NCDRequestClient_req * find_req (NCDRequestClient *o, uint32_t request_id)
{
    BAVLNode *tn = BAVL_LookupExact(&o->reqs_tree, &request_id);
    if (!tn) {
        return NULL;
    }
    
    struct NCDRequestClient_req *req = UPPER_OBJECT(tn, struct NCDRequestClient_req, reqs_tree_node);
    ASSERT(req->request_id == request_id)
    
    return req;
}

static int get_free_request_id (NCDRequestClient *o, uint32_t *out)
{
    uint32_t first = o->next_request_id;
    
    do {
        if (!find_req(o, o->next_request_id)) {
            *out = o->next_request_id;
            return 1;
        }
        o->next_request_id++;
    } while (o->next_request_id != first);
    
    return 0;
}

static int build_requestproto_packet (uint32_t request_id, uint32_t type, NCDValue *payload_value, uint8_t **out_data, int *out_len)
{
    struct header {
        struct packetproto_header pp;
        struct requestproto_header rp;
    } __attribute__((packed));
    
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
    
    uint8_t *packet = (uint8_t *)ExpString_Get(&str);
    
    struct header *header = (void *)packet;
    header->pp.len = htol16(len - sizeof(struct packetproto_header));
    header->rp.request_id = htol32(request_id);
    header->rp.type = htol32(type);
    
    *out_data = packet;
    *out_len = len;
    return 1;
    
fail1:
    ExpString_Free(&str);
fail0:
    return 0;
}

static void build_nodata_packet (uint32_t request_id, uint32_t type, uint8_t *data, int *out_len)
{
    struct header {
        struct packetproto_header pp;
        struct requestproto_header rp;
    } __attribute__((packed));
    
    struct header *header = (void *)data;
    header->pp.len = htol16(sizeof(header->rp));
    header->rp.request_id = htol32(request_id);
    header->rp.type = htol32(type);
    
    *out_len = sizeof(*header);
}

static int req_is_aborted (struct NCDRequestClient_req *req)
{
    switch (req->state) {
        case RSTATE_SENDING_REQUEST:
        case RSTATE_READY:
            return 0;
        default:
            return 1;
    }
}

static void req_abort (struct NCDRequestClient_req *req)
{
    ASSERT(!req_is_aborted(req))
    
    switch (req->state) {
        case RSTATE_SENDING_REQUEST: {
            req->state = RSTATE_SENDING_REQUEST_ABORT;
        } break;
        
        case RSTATE_READY: {
            req_send_abort(req);
        } break;
        
        default: ASSERT(0);
    }
}

static void req_free (struct NCDRequestClient_req *req)
{
    NCDRequestClient *client = req->client;
    PacketPassFifoQueueFlow_AssertFree(&req->send_qflow);
    ASSERT(!req->creq)
    
    // free queue flow
    PacketPassFifoQueueFlow_Free(&req->send_qflow);
    
    // free request data
    free(req->request_data);
    
    // remove from reqs tree
    BAVL_Remove(&client->reqs_tree, &req->reqs_tree_node);
    
    // free structure
    free(req);
}

static void req_send_abort (struct NCDRequestClient_req *req)
{
    // build packet
    build_nodata_packet(req->request_id, REQUESTPROTO_TYPE_CLIENT_ABORT, req->request_data, &req->request_len);
    
    // start sending
    PacketPassInterface_Sender_Send(req->send_qflow_iface, req->request_data, req->request_len);
    
    // set state sending abort
    req->state = RSTATE_SENDING_ABORT;
}

static void req_qflow_send_iface_handler_done (struct NCDRequestClient_req *req)
{
    switch (req->state) {
        case RSTATE_SENDING_REQUEST: {
            // set state ready
            req->state = RSTATE_READY;
            
            // call sent handler
            req->creq->handler_sent(req->creq->user);
            return;
        } break;
        
        case RSTATE_SENDING_REQUEST_ABORT: {
            // send abort
            req_send_abort(req);
        } break;
        
        case RSTATE_SENDING_ABORT: {
            // set state waiting end
            req->state = RSTATE_WAITING_END;
        } break;
        
        case RSTATE_DEAD_SENDING: {
            // free req
            req_free(req);
        } break;
        
        default: ASSERT(0);
    }
}

int NCDRequestClient_Init (NCDRequestClient *o, struct NCDRequestClient_addr addr, BReactor *reactor, void *user,
                           NCDRequestClient_handler_error handler_error,
                           NCDRequestClient_handler_connected handler_connected)
{
    ASSERT(handler_error)
    ASSERT(handler_connected)
    
    // init arguments
    o->reactor = reactor;
    o->user = user;
    o->handler_error = handler_error;
    o->handler_connected = handler_connected;
    
    // init connector
    switch (addr.type) {
        case NCDREQUESTCLIENT_ADDR_TYPE_UNIX: {
            ASSERT(addr.u.unix_socket_path)
            
            if (!BConnector_InitUnix(&o->connector, addr.u.unix_socket_path, reactor, o, (BConnector_handler)connector_handler)) {
                BLog(BLOG_ERROR, "BConnector_InitUnix failed");
                goto fail0;
            }
        } break;
        
        case NCDREQUESTCLIENT_ADDR_TYPE_TCP: {
            BAddr_Assert(&addr.u.tcp_baddr);
            
            if (!BConnector_Init(&o->connector, addr.u.tcp_baddr, reactor, o, (BConnector_handler)connector_handler)) {
                BLog(BLOG_ERROR, "BConnector_InitUnix failed");
                goto fail0;
            }
        } break;
        
        default: ASSERT(0);
    }
    
    // init reqs tree
    BAVL_Init(&o->reqs_tree, OFFSET_DIFF(struct NCDRequestClient_req, request_id, reqs_tree_node), uint32_comparator, NULL);
    
    // set next request ID
    o->next_request_id = 0;
    
    // set state connecting
    o->state = CSTATE_CONNECTING;
    
    DebugCounter_Init(&o->d_reqests_ctr);
    DebugError_Init(&o->d_err, BReactor_PendingGroup(reactor));
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail0:
    return 0;
}

void NCDRequestClient_Free (NCDRequestClient *o)
{
    DebugObject_Free(&o->d_obj);
    DebugError_Free(&o->d_err);
    DebugCounter_Free(&o->d_reqests_ctr);
    
    if (o->state == CSTATE_CONNECTED) {
        // allow freeing queue flow
        PacketPassFifoQueue_PrepareFree(&o->send_queue);
        
        // free remaining reqs
        BAVLNode *tn;
        while (tn = BAVL_GetFirst(&o->reqs_tree)) {
            struct NCDRequestClient_req *req = UPPER_OBJECT(tn, struct NCDRequestClient_req, reqs_tree_node);
            ASSERT(!req->creq)
            ASSERT(req->state != RSTATE_SENDING_REQUEST)
            ASSERT(req->state != RSTATE_READY)
            req_free(req);
        }
        
        // free connection stuff
        PacketPassFifoQueue_Free(&o->send_queue);
        PacketStreamSender_Free(&o->send_sender);
        PacketProtoDecoder_Free(&o->recv_decoder);
        PacketPassInterface_Free(&o->recv_if);
        BConnection_RecvAsync_Free(&o->con);
        BConnection_SendAsync_Free(&o->con);
        BConnection_Free(&o->con);
    }
    
    // free connector
    BConnector_Free(&o->connector);
}

int NCDRequestClientRequest_Init (NCDRequestClientRequest *o, NCDRequestClient *client, NCDValue *payload_value, void *user,
                                  NCDRequestClientRequest_handler_sent handler_sent,
                                  NCDRequestClientRequest_handler_reply handler_reply,
                                  NCDRequestClientRequest_handler_finished handler_finished)
{
    ASSERT(client->state == CSTATE_CONNECTED)
    DebugError_AssertNoError(&client->d_err);
    ASSERT(payload_value)
    ASSERT(handler_sent)
    ASSERT(handler_reply)
    ASSERT(handler_finished)
    
    // init arguments
    o->client = client;
    o->user = user;
    o->handler_sent = handler_sent;
    o->handler_reply = handler_reply;
    o->handler_finished = handler_finished;
    
    // allocate req structure
    struct NCDRequestClient_req *req = malloc(sizeof(*req));
    if (!req) {
        BLog(BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    
    // allocate request ID
    if (!get_free_request_id(client, &req->request_id)) {
        BLog(BLOG_ERROR, "failed to allocate request ID");
        goto fail1;
    }
    
    // insert to reqs tree
    int res = BAVL_Insert(&client->reqs_tree, &req->reqs_tree_node, NULL);
    ASSERT(res)
    
    // set pointers
    o->req = req;
    req->creq = o;
    req->client = client;
    
    // build request
    if (!build_requestproto_packet(req->request_id, REQUESTPROTO_TYPE_CLIENT_REQUEST, payload_value, &req->request_data, &req->request_len)) {
        BLog(BLOG_ERROR, "failed to build request");
        goto fail2;
    }
    
    // init queue flow
    PacketPassFifoQueueFlow_Init(&req->send_qflow, &client->send_queue);
    
    // init send interface
    req->send_qflow_iface = PacketPassFifoQueueFlow_GetInput(&req->send_qflow);
    PacketPassInterface_Sender_Init(req->send_qflow_iface, (PacketPassInterface_handler_done)req_qflow_send_iface_handler_done, req);
    
    // start sending request
    PacketPassInterface_Sender_Send(req->send_qflow_iface, req->request_data, req->request_len);
    
    // set state sending request
    req->state = RSTATE_SENDING_REQUEST;
    
    DebugCounter_Increment(&client->d_reqests_ctr);
    DebugError_Init(&o->d_err, BReactor_PendingGroup(client->reactor));
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail2:
    BAVL_Remove(&client->reqs_tree, &req->reqs_tree_node);
fail1:
    free(req);
fail0:
    return 0;
}

void NCDRequestClientRequest_Free (NCDRequestClientRequest *o)
{
    struct NCDRequestClient_req *req = o->req;
    DebugObject_Free(&o->d_obj);
    DebugError_Free(&o->d_err);
    DebugCounter_Decrement(&o->client->d_reqests_ctr);
    
    if (req) {
        ASSERT(req->creq == o)
        
        // remove reference to us
        req->creq = NULL;
        
        // abort req if not already
        if (!req_is_aborted(req)) {
            req_abort(req);
        }
    }
}

void NCDRequestClientRequest_Abort (NCDRequestClientRequest *o)
{
    struct NCDRequestClient_req *req = o->req;
    DebugObject_Access(&o->d_obj);
    DebugError_AssertNoError(&o->d_err);
    ASSERT(req)
    ASSERT(req->creq == o)
    ASSERT(!req_is_aborted(req))
    
    // abort req
    req_abort(req);
}
