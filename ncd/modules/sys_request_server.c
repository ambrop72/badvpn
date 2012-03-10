/**
 * @file sys_request_server.c
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
 * 
 * @section DESCRIPTION
 * 
 * A simple a IPC interface for NCD to talk to other processes over a Unix socket.
 * 
 * Synopsis:
 *   sys.request_server(string socket_path, string request_handler_template, list args)
 * 
 * Description:
 *   Initializes a request server on the given socket path. Requests are served by
 *   starting a template process for every request. Multiple such processes may
 *   exist simultaneously. Termination of these processess may be initiated at
 *   any time if the request server no longer needs the request in question served.
 *   The payload of a request is a value, and can be accessed as _request.data
 *   from within the handler process. Replies to the request can be sent using
 *   _request->reply(data); replies are values too. Finally, _request->finish()
 *   should be called to indicate that no further replies will be sent. Calling
 *   finish() will immediately initiate termination of the handler process.
 *   Requests can be sent to NCD using the badvpn-ncd-request program.
 * 
 * Synopsis:
 *   sys.request_server.request::reply(reply_data)
 * 
 * Synopsis:
 *   sys.request_server.request::finish()
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>

#include <misc/offset.h>
#include <misc/debug.h>
#include <misc/byteorder.h>
#include <protocol/packetproto.h>
#include <protocol/requestproto.h>
#include <structure/LinkedList0.h>
#include <system/BConnection.h>
#include <flow/PacketProtoDecoder.h>
#include <flow/PacketStreamSender.h>
#include <flow/PacketPassFifoQueue.h>
#include <ncd/NCDValueParser.h>
#include <ncd/NCDValueGenerator.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_sys_request_server.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define SEND_PAYLOAD_MTU 32768
#define RECV_PAYLOAD_MTU 32768

#define SEND_MTU (SEND_PAYLOAD_MTU + sizeof(struct requestproto_header))
#define RECV_MTU (RECV_PAYLOAD_MTU + sizeof(struct requestproto_header))

struct instance {
    NCDModuleInst *i;
    char *socket_path;
    char *request_handler_template;
    NCDValue *args;
    BListener listener;
    LinkedList0 connections_list;
    int dying;
};

#define CONNECTION_STATE_RUNNING 1
#define CONNECTION_STATE_TERMINATING 2

struct connection {
    struct instance *inst;
    LinkedList0Node connections_list_node;
    BConnection con;
    PacketProtoDecoder recv_decoder;
    PacketPassInterface recv_if;
    PacketPassFifoQueue send_queue;
    PacketStreamSender send_pss;
    LinkedList0 requests_list;
    LinkedList0 replies_list;
    int state;
};

struct request {
    struct connection *con;
    uint32_t request_id;
    LinkedList0Node requests_list_node;
    NCDValue request_data;
    NCDModuleProcess process;
    BPending finish_job;
    int finished;
};

struct reply {
    struct connection *con;
    LinkedList0Node replies_list_node;
    PacketPassFifoQueueFlow send_qflow;
    PacketPassInterface *send_qflow_if;
    uint8_t *send_buf;
};

static void listener_handler (struct instance *o);
static void connection_free (struct connection *c);
static void connection_free_link (struct connection *c);
static void connection_terminate (struct connection *c);
static void connection_con_handler (struct connection *c, int event);
static void connection_recv_decoder_handler_error (struct connection *c);
static void connection_recv_if_handler_send (struct connection *c, uint8_t *data, int data_len);
static int request_init (struct connection *c, uint32_t request_id, const uint8_t *data, int data_len);
static void request_free (struct request *r);
static void request_process_handler_event (struct request *r, int event);
static int request_process_func_getspecialobj (struct request *r, const char *name, NCDObject *out_object);
static int request_process_request_obj_func_getvar (struct request *r, const char *name, NCDValue *out_value);
static void request_finish_job_handler (struct request *r);
static int reply_init (struct connection *c, uint32_t request_id, uint32_t flags, NCDValue *reply_data);
static void reply_free (struct reply *r);
static void reply_send_qflow_if_handler_done (struct reply *r);
static void instance_free (struct instance *o);

static void listener_handler (struct instance *o)
{
    ASSERT(!o->dying)
    
    BReactor *reactor = o->i->params->reactor;
    BPendingGroup *pg = BReactor_PendingGroup(reactor);
    
    struct connection *c = malloc(sizeof(*c));
    if (!c) {
        ModuleLog(o->i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    
    c->inst = o;
    
    LinkedList0_Prepend(&o->connections_list, &c->connections_list_node);
    
    if (!BConnection_Init(&c->con, BCONNECTION_SOURCE_LISTENER(&o->listener, NULL), reactor, c, (BConnection_handler)connection_con_handler)) {
        ModuleLog(o->i, BLOG_ERROR, "BConnection_Init failed");
        goto fail1;
    }
    
    BConnection_SendAsync_Init(&c->con);
    BConnection_RecvAsync_Init(&c->con);
    StreamPassInterface *con_send_if = BConnection_SendAsync_GetIf(&c->con);
    StreamRecvInterface *con_recv_if = BConnection_RecvAsync_GetIf(&c->con);
    
    PacketPassInterface_Init(&c->recv_if, RECV_MTU, (PacketPassInterface_handler_send)connection_recv_if_handler_send, c, pg);
    
    if (!PacketProtoDecoder_Init(&c->recv_decoder, con_recv_if, &c->recv_if, pg, c, (PacketProtoDecoder_handler_error)connection_recv_decoder_handler_error)) {
        ModuleLog(o->i, BLOG_ERROR, "PacketProtoDecoder_Init failed");
        goto fail2;
    }
    
    PacketStreamSender_Init(&c->send_pss, con_send_if, PACKETPROTO_ENCLEN(SEND_MTU), pg);
    
    PacketPassFifoQueue_Init(&c->send_queue, PacketStreamSender_GetInput(&c->send_pss), pg);
    
    LinkedList0_Init(&c->requests_list);
    
    LinkedList0_Init(&c->replies_list);
    
    c->state = CONNECTION_STATE_RUNNING;
    
    ModuleLog(o->i, BLOG_INFO, "connection initialized");
    return;
    
fail3:
    PacketStreamSender_Free(&c->send_pss);
    PacketProtoDecoder_Free(&c->recv_decoder);
fail2:
    PacketPassInterface_Free(&c->recv_if);
    BConnection_RecvAsync_Free(&c->con);
    BConnection_SendAsync_Free(&c->con);
    BConnection_Free(&c->con);
fail1:
    LinkedList0_Remove(&o->connections_list, &c->connections_list_node);
    free(c);
fail0:
    return;
}

static void connection_free (struct connection *c)
{
    struct instance *o = c->inst;
    ASSERT(c->state == CONNECTION_STATE_TERMINATING)
    ASSERT(LinkedList0_IsEmpty(&c->requests_list))
    ASSERT(LinkedList0_IsEmpty(&c->replies_list))
    
    LinkedList0_Remove(&o->connections_list, &c->connections_list_node);
    free(c);
}

static void connection_free_link (struct connection *c)
{
    PacketPassFifoQueue_PrepareFree(&c->send_queue);
    
    LinkedList0Node *ln;
    while (ln = LinkedList0_GetFirst(&c->replies_list)) {
        struct reply *r = UPPER_OBJECT(ln, struct reply, replies_list_node);
        ASSERT(r->con == c)
        reply_free(r);
    }
    
    PacketPassFifoQueue_Free(&c->send_queue);
    PacketStreamSender_Free(&c->send_pss);
    PacketProtoDecoder_Free(&c->recv_decoder);
    PacketPassInterface_Free(&c->recv_if);
    BConnection_RecvAsync_Free(&c->con);
    BConnection_SendAsync_Free(&c->con);
    BConnection_Free(&c->con);
}

static void connection_terminate (struct connection *c)
{
    ASSERT(c->state == CONNECTION_STATE_RUNNING)
    
    for (LinkedList0Node *ln = LinkedList0_GetFirst(&c->requests_list); ln; ln = LinkedList0Node_Next(ln)) {
        struct request *r = UPPER_OBJECT(ln, struct request, requests_list_node);
        
        if (!r->finished) {
            NCDModuleProcess_Terminate(&r->process);
        }
        BPending_Unset(&r->finish_job);
    }
    
    connection_free_link(c);
    
    c->state = CONNECTION_STATE_TERMINATING;
    
    if (LinkedList0_IsEmpty(&c->requests_list)) {
        connection_free(c);
        return;
    }
}

static void connection_con_handler (struct connection *c, int event)
{
    struct instance *o = c->inst;
    ASSERT(c->state == CONNECTION_STATE_RUNNING)
    
    ModuleLog(o->i, BLOG_INFO, "connection closed");
    
    connection_terminate(c);
}

static void connection_recv_decoder_handler_error (struct connection *c)
{
    struct instance *o = c->inst;
    ASSERT(c->state == CONNECTION_STATE_RUNNING)
    
    ModuleLog(o->i, BLOG_INFO, "decoder error");
    
    connection_terminate(c);
}

static void connection_recv_if_handler_send (struct connection *c, uint8_t *data, int data_len)
{
    struct instance *o = c->inst;
    ASSERT(c->state == CONNECTION_STATE_RUNNING)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= RECV_MTU)
    
    PacketPassInterface_Done(&c->recv_if);
    
    if (data_len < sizeof(struct requestproto_header)) {
        ModuleLog(o->i, BLOG_INFO, "missing requestproto header");
        return;
    }
    
    struct requestproto_header *header = (struct requestproto_header *)data;
    uint32_t request_id = ltoh32(header->request_id);
    uint32_t flags = ltoh32(header->flags);
    
    if (flags != REQUESTPROTO_REQUEST_FLAG) {
        ModuleLog(o->i, BLOG_INFO, "invalid requestproto flags");
        return;
    }
    
    request_init(c, request_id, data + sizeof(*header), data_len - sizeof(*header));
}

static int request_init (struct connection *c, uint32_t request_id, const uint8_t *data, int data_len)
{
    struct instance *o = c->inst;
    ASSERT(c->state == CONNECTION_STATE_RUNNING)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= RECV_PAYLOAD_MTU)
    
    struct request *r = malloc(sizeof(*r));
    if (!r) {
        ModuleLog(o->i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    
    r->con = c;
    r->request_id = request_id;
    
    LinkedList0_Prepend(&c->requests_list, &r->requests_list_node);
    
    if (!NCDValueParser_Parse(data, data_len, &r->request_data)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValueParser_Parse failed");
        goto fail1;
    }
    
    NCDValue args;
    if (!NCDValue_InitCopy(&args, o->args)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail2;
    }
    
    if (!NCDModuleProcess_Init(&r->process, o->i, o->request_handler_template, args, r, (NCDModuleProcess_handler_event)request_process_handler_event)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDModuleProcess_Init failed");
        NCDValue_Free(&args);
        goto fail2;
    }
    
    NCDModuleProcess_SetSpecialFuncs(&r->process, (NCDModuleProcess_func_getspecialobj)request_process_func_getspecialobj);
    
    BPending_Init(&r->finish_job, BReactor_PendingGroup(o->i->params->reactor), (BPending_handler)request_finish_job_handler, r);
    
    r->finished = 0;
    
    ModuleLog(o->i, BLOG_INFO, "request initialized");
    return 1;
    
fail2:
    NCDValue_Free(&r->request_data);
fail1:
    LinkedList0_Remove(&c->requests_list, &r->requests_list_node);
    free(r);
fail0:
    return 0;
}

static void request_free (struct request *r)
{
    struct connection *c = r->con;
    
    BPending_Free(&r->finish_job);
    NCDModuleProcess_Free(&r->process);
    NCDValue_Free(&r->request_data);
    LinkedList0_Remove(&c->requests_list, &r->requests_list_node);
    free(r);
}

static void request_process_handler_event (struct request *r, int event)
{
    struct connection *c = r->con;
    struct instance *o = c->inst;
    
    switch (event) {
        case NCDMODULEPROCESS_EVENT_UP: {
            ASSERT(c->state == CONNECTION_STATE_RUNNING)
        } break;
        
        case NCDMODULEPROCESS_EVENT_DOWN: {
            NCDModuleProcess_Continue(&r->process);
        } break;
        
        case NCDMODULEPROCESS_EVENT_TERMINATED: {
            ASSERT(r->finished || c->state == CONNECTION_STATE_TERMINATING)
            
            request_free(r);
            
            if (c->state == CONNECTION_STATE_TERMINATING && LinkedList0_IsEmpty(&c->requests_list)) {
                connection_free(c);
                
                if (o->dying && LinkedList0_IsEmpty(&o->connections_list)) {
                    instance_free(o);
                    return;
                }
            }
        } break;
        
        default: ASSERT(0);
    }
}

static int request_process_func_getspecialobj (struct request *r, const char *name, NCDObject *out_object)
{
    if (!strcmp(name, "_request")) {
        *out_object = NCDObject_Build("sys.request_server.request", r, (NCDObject_func_getvar)request_process_request_obj_func_getvar, NULL);
        return 1;
    }
    
    return 0;
}

static int request_process_request_obj_func_getvar (struct request *r, const char *name, NCDValue *out_value)
{
    struct instance *o = r->con->inst;
    
    if (!strcmp(name, "data")) {
        if (!NCDValue_InitCopy(out_value, &r->request_data)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
            return 0;
        }
        return 1;
    }
    
    return 0;
}

static void request_finish_job_handler (struct request *r)
{
    struct connection *c = r->con;
    ASSERT(c->state == CONNECTION_STATE_RUNNING)
    ASSERT(!r->finished)
    
    NCDModuleProcess_Terminate(&r->process);
    
    r->finished = 1;
}

static int reply_init (struct connection *c, uint32_t request_id, uint32_t flags, NCDValue *reply_data)
{
    struct instance *o = c->inst;
    ASSERT(c->state == CONNECTION_STATE_RUNNING)
    
    struct reply *r = malloc(sizeof(*r));
    if (!r) {
        ModuleLog(o->i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    
    r->con = c;
    
    LinkedList0_Prepend(&c->replies_list, &r->replies_list_node);
    
    PacketPassFifoQueueFlow_Init(&r->send_qflow, &c->send_queue);
    
    r->send_qflow_if = PacketPassFifoQueueFlow_GetInput(&r->send_qflow);
    PacketPassInterface_Sender_Init(r->send_qflow_if, (PacketPassInterface_handler_done)reply_send_qflow_if_handler_done, r);
    
    struct reply_header {
        struct packetproto_header pp;
        struct requestproto_header rp;
    };
    
    ExpString str;
    if (!ExpString_Init(&str)) {
        ModuleLog(o->i, BLOG_ERROR, "ExpString_Init failed");
        goto fail1;
    }
    
    if (!ExpString_AppendZeros(&str, sizeof(struct reply_header))) {
        ModuleLog(o->i, BLOG_ERROR, "ExpString_AppendBinary failed");
        goto fail2;
    }
    
    if (reply_data && !NCDValueGenerator_AppendGenerate(reply_data, &str)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValueGenerator_AppendGenerate failed");
        goto fail2;
    }
    
    size_t len = ExpString_Length(&str);
    if (len > INT_MAX || len > PACKETPROTO_ENCLEN(SEND_MTU) || len - sizeof(struct packetproto_header) > UINT16_MAX) {
        ModuleLog(o->i, BLOG_ERROR, "reply is too long");
        goto fail2;
    }
    
    r->send_buf = ExpString_Get(&str);
    
    struct reply_header *header = (struct reply_header *)r->send_buf;
    header->pp.len = htol16(len - sizeof(struct packetproto_header));
    header->rp.request_id = htol32(request_id);
    header->rp.flags = htol32(flags);
    
    PacketPassInterface_Sender_Send(r->send_qflow_if, r->send_buf, len);
    return 1;
    
fail2:
    ExpString_Free(&str);
fail1:
    PacketPassFifoQueueFlow_Free(&r->send_qflow);
    LinkedList0_Remove(&c->replies_list, &r->replies_list_node);
    free(r);
fail0:
    return 0;
}

static void reply_free (struct reply *r)
{
    struct connection *c = r->con;
    PacketPassFifoQueueFlow_AssertFree(&r->send_qflow);
    
    free(r->send_buf);
    PacketPassFifoQueueFlow_Free(&r->send_qflow);
    LinkedList0_Remove(&c->replies_list, &r->replies_list_node);
    free(r);
}

static void reply_send_qflow_if_handler_done (struct reply *r)
{
    reply_free(r);
}

static void func_new (NCDModuleInst *i)
{
    // allocate structure
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // check arguments
    NCDValue *socket_path_arg;
    NCDValue *request_handler_template_arg;
    NCDValue *args_arg;
    if (!NCDValue_ListRead(i->args, 3, &socket_path_arg, &request_handler_template_arg, &args_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(socket_path_arg) != NCDVALUE_STRING || NCDValue_Type(request_handler_template_arg) != NCDVALUE_STRING ||
        NCDValue_Type(args_arg) != NCDVALUE_LIST) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->socket_path = NCDValue_StringValue(socket_path_arg);
    o->request_handler_template = NCDValue_StringValue(request_handler_template_arg);
    o->args = args_arg;
    
    // make sure socket file doesn't exist
    if (unlink(o->socket_path) < 0 && errno != ENOENT) {
        ModuleLog(o->i, BLOG_ERROR, "unlink failed");
        goto fail1;
    }
    
    // init listener
    if (!BListener_InitUnix(&o->listener, o->socket_path, i->params->reactor, o, (BListener_handler)listener_handler)) {
        ModuleLog(o->i, BLOG_ERROR, "BListener_InitUnix failed");
        goto fail1;
    }
    
    // init connections list
    LinkedList0_Init(&o->connections_list);
    
    // set not dying
    o->dying = 0;
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void instance_free (struct instance *o)
{
    NCDModuleInst *i = o->i;
    ASSERT(o->dying)
    ASSERT(LinkedList0_IsEmpty(&o->connections_list))
    
    // free structure
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    NCDModuleInst *i = o->i;
    ASSERT(!o->dying)
    
    // free listener
    BListener_Free(&o->listener);
    
    // remove socket file
    if (unlink(o->socket_path)) {
        ModuleLog(o->i, BLOG_ERROR, "unlink failed");
    }
    
    // terminate connections
    LinkedList0Node *next_ln;
    for (LinkedList0Node *ln = LinkedList0_GetFirst(&o->connections_list); ln && (next_ln = LinkedList0Node_Next(ln)), ln; ln = next_ln) { 
        struct connection *c = UPPER_OBJECT(ln, struct connection, connections_list_node);
        ASSERT(c->inst == o)
        
        if (c->state != CONNECTION_STATE_TERMINATING) {
            connection_terminate(c);
        }
    }
    
    // set dying
    o->dying = 1;
    
    // if no connections, die right away
    if (LinkedList0_IsEmpty(&o->connections_list)) {
        instance_free(o);
        return;
    }
}

static void reply_func_new (NCDModuleInst *i)
{
    NCDValue *reply_data;
    if (!NCDValue_ListRead(i->args, 1, &reply_data)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail;
    }
    
    NCDModuleInst_Backend_Up(i);
    
    struct request *r = i->method_user;
    struct connection *c = r->con;
    
    if (c->state != CONNECTION_STATE_RUNNING) {
        ModuleLog(i, BLOG_ERROR, "connection is terminating, cannot submit reply");
        goto fail;
    }
    
    if (r->finished || BPending_IsSet(&r->finish_job)) {
        ModuleLog(i, BLOG_ERROR, "request is already finished, cannot submit reply");
        goto fail;
    }
    
    if (!reply_init(c, r->request_id, REQUESTPROTO_REPLY_FLAG_DATA, reply_data)) {
        ModuleLog(i, BLOG_ERROR, "failed to submit reply");
        goto fail;
    }
    
    return;
    
fail:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void finish_func_new (NCDModuleInst *i)
{
    if (!NCDValue_ListRead(i->args, 0)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail;
    }
    
    NCDModuleInst_Backend_Up(i);
    
    struct request *r = i->method_user;
    struct connection *c = r->con;
    
    if (c->state != CONNECTION_STATE_RUNNING) {
        ModuleLog(i, BLOG_ERROR, "connection is terminating, cannot submit reply");
        goto fail;
    }
    
    if (r->finished || BPending_IsSet(&r->finish_job)) {
        ModuleLog(i, BLOG_ERROR, "request is already finished, cannot submit reply");
        goto fail;
    }
    
    BPending_Set(&r->finish_job);
    
    if (!reply_init(c, r->request_id, REQUESTPROTO_REPLY_FLAG_END, NULL)) {
        ModuleLog(i, BLOG_ERROR, "failed to submit reply");
        BPending_Unset(&r->finish_job); // don't terminate request process!
        goto fail;
    }
    
    return;
    
fail:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static const struct NCDModule modules[] = {
    {
        .type = "sys.request_server",
        .func_new = func_new,
        .func_die = func_die
    }, {
        .type = "sys.request_server.request::reply",
        .func_new = reply_func_new
    }, {
        .type = "sys.request_server.request::finish",
        .func_new = finish_func_new
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_sys_request_server = {
    .modules = modules
};
