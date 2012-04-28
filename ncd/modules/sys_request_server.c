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
 *   sys.request_server(listen_address, string request_handler_template, list args)
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
 *   The listen_address argument must be one of:
 *   - {"unix", socket_path}
 *     Listens on a Unix socket.
 *   - {"tcp", ip_address, port_number}
 *     Listens on a TCP socket. The address must be numeric and not a name.
 *     For IPv6, the address must be enclosed in [].
 * 
 * Predefined variables in request_handler_template:
 *   _request.data - the request payload as sent by the client
 *   _request.client_addr_type - type of client address; "none", "ipv4" or "ipv6"
 *   _request.client_addr - client IP address. IPv4 addresses are standard dotted-decimal
 *     without leading zeros, e.g. "14.6.0.251". IPv6 addresses are full 8
 *     lower-case-hexadecimal numbers separated by 7 colons, without leading zeros,
 *     e.g. "61:71a4:81f:98aa:57:0:5efa:17". If client_addr_type=="none", this too
 *     is "none".
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
#include <misc/parse_number.h>
#include <protocol/packetproto.h>
#include <protocol/requestproto.h>
#include <structure/LinkedList0.h>
#include <system/BConnection.h>
#include <system/BAddr.h>
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
    char *unix_socket_path;
    char *request_handler_template;
    NCDValue *args;
    BListener listener;
    LinkedList0 connections_list;
    int dying;
};

#define CONNECTION_STATE_RUNNING 1
#define CONNECTION_STATE_TERMINATING 2

struct reply;

struct connection {
    struct instance *inst;
    LinkedList0Node connections_list_node;
    BConnection con;
    BAddr addr;
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
    struct reply *end_reply;
    NCDModuleProcess process;
    int terminating;
    int got_finished;
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
static struct request * find_request (struct connection *c, uint32_t request_id);
static void request_process_handler_event (struct request *r, int event);
static int request_process_func_getspecialobj (struct request *r, const char *name, NCDObject *out_object);
static int request_process_request_obj_func_getvar (struct request *r, const char *name, NCDValue *out_value);
static void request_terminate (struct request *r);
static struct reply * reply_init (struct connection *c, uint32_t request_id, NCDValue *reply_data);
static void reply_start (struct reply *r, uint32_t type);
static void reply_free (struct reply *r);
static void reply_send_qflow_if_handler_done (struct reply *r);
static int init_listen (struct instance *o, NCDValue *listen_addr_arg);
static void instance_free (struct instance *o);

static void listener_handler (struct instance *o)
{
    ASSERT(!o->dying)
    
    BReactor *reactor = o->i->iparams->reactor;
    BPendingGroup *pg = BReactor_PendingGroup(reactor);
    
    struct connection *c = malloc(sizeof(*c));
    if (!c) {
        ModuleLog(o->i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    
    c->inst = o;
    
    LinkedList0_Prepend(&o->connections_list, &c->connections_list_node);
    
    if (!BConnection_Init(&c->con, BCONNECTION_SOURCE_LISTENER(&o->listener, &c->addr), reactor, c, (BConnection_handler)connection_con_handler)) {
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
        
        if (!r->terminating) {
            request_terminate(r);
        }
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
    
    ModuleLog(o->i, BLOG_ERROR, "decoder error");
    
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
        ModuleLog(o->i, BLOG_ERROR, "missing requestproto header");
        goto fail;
    }
    
    struct requestproto_header *header = (void *)data;
    uint32_t request_id = ltoh32(header->request_id);
    uint32_t type = ltoh32(header->type);
    
    switch (type) {
        case REQUESTPROTO_TYPE_CLIENT_REQUEST: {
            if (find_request(c, request_id)) {
                ModuleLog(o->i, BLOG_ERROR, "request with the same ID already exists");
                goto fail;
            }
            
            if (!request_init(c, request_id, data + sizeof(*header), data_len - sizeof(*header))) {
                goto fail;
            }
        } break;
        
        case REQUESTPROTO_TYPE_CLIENT_ABORT: {
            struct request *r = find_request(c, request_id);
            if (!r) {
                // this is expected if we finish before we get the abort
                return;
            }
            
            if (!r->terminating) {
                request_terminate(r);
            }
        } break;
        
        default:
            ModuleLog(o->i, BLOG_ERROR, "invalid requestproto type");
            goto fail;
    }
    
    return;
    
fail:
    connection_terminate(c);
}

static int request_init (struct connection *c, uint32_t request_id, const uint8_t *data, int data_len)
{
    struct instance *o = c->inst;
    ASSERT(c->state == CONNECTION_STATE_RUNNING)
    ASSERT(!find_request(c, request_id))
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
    
    if (!NCDValueParser_Parse((const char *)data, data_len, &r->request_data)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValueParser_Parse failed");
        goto fail1;
    }
    
    if (!(r->end_reply = reply_init(c, request_id, NULL))) {
        goto fail2;
    }
    
    NCDValue args;
    if (!NCDValue_InitCopy(&args, o->args)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail3;
    }
    
    if (!NCDModuleProcess_Init(&r->process, o->i, o->request_handler_template, args, r, (NCDModuleProcess_handler_event)request_process_handler_event)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDModuleProcess_Init failed");
        NCDValue_Free(&args);
        goto fail3;
    }
    
    NCDModuleProcess_SetSpecialFuncs(&r->process, (NCDModuleProcess_func_getspecialobj)request_process_func_getspecialobj);
    
    r->terminating = 0;
    r->got_finished = 0;
    
    ModuleLog(o->i, BLOG_INFO, "request initialized");
    return 1;
    
fail3:
    reply_free(r->end_reply);
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
    NCDModuleProcess_AssertFree(&r->process);
    
    if (c->state != CONNECTION_STATE_TERMINATING) {
        uint32_t type = r->got_finished ? REQUESTPROTO_TYPE_SERVER_FINISHED : REQUESTPROTO_TYPE_SERVER_ERROR;
        reply_start(r->end_reply, type);
    }
    
    NCDModuleProcess_Free(&r->process);
    NCDValue_Free(&r->request_data);
    LinkedList0_Remove(&c->requests_list, &r->requests_list_node);
    free(r);
}

static struct request * find_request (struct connection *c, uint32_t request_id)
{
    for (LinkedList0Node *ln = LinkedList0_GetFirst(&c->requests_list); ln; ln = LinkedList0Node_Next(ln)) {
        struct request *r = UPPER_OBJECT(ln, struct request, requests_list_node);
        if (!r->terminating && r->request_id == request_id) {
            return r;
        }
    }
    
    return NULL;
}

static void request_process_handler_event (struct request *r, int event)
{
    struct connection *c = r->con;
    struct instance *o = c->inst;
    
    switch (event) {
        case NCDMODULEPROCESS_EVENT_UP: {
            ASSERT(!r->terminating)
        } break;
        
        case NCDMODULEPROCESS_EVENT_DOWN: {
            ASSERT(!r->terminating)
            
            NCDModuleProcess_Continue(&r->process);
        } break;
        
        case NCDMODULEPROCESS_EVENT_TERMINATED: {
            ASSERT(r->terminating)
            
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
    
    if (!strcmp(name, "client_addr_type")) {
        const char *str = "none";
        switch (r->con->addr.type) {
            case BADDR_TYPE_IPV4:
                str = "ipv4";
                break;
            case BADDR_TYPE_IPV6:
                str = "ipv6";
                break;
        }
        
        if (!NCDValue_InitString(out_value, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        return 1;
    }
    
    if (!strcmp(name, "client_addr")) {
        char str[BIPADDR_MAX_PRINT_LEN] = "none";
        
        switch (r->con->addr.type) {
            case BADDR_TYPE_IPV4:
            case BADDR_TYPE_IPV6: {
                BIPAddr ipaddr;
                BAddr_GetIPAddr(&r->con->addr, &ipaddr);
                BIPAddr_Print(&ipaddr, str);
            } break;
        }
        
        if (!NCDValue_InitString(out_value, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        return 1;
    }
    
    return 0;
}

static void request_terminate (struct request *r)
{
    ASSERT(!r->terminating)
    
    NCDModuleProcess_Terminate(&r->process);
    
    r->terminating = 1;
}

static struct reply * reply_init (struct connection *c, uint32_t request_id, NCDValue *reply_data)
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
    } __attribute__((packed));
    
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
    
    r->send_buf = (uint8_t *)ExpString_Get(&str);
    
    struct reply_header *header = (void *)r->send_buf;
    header->pp.len = htol16(len - sizeof(header->pp));
    header->rp.request_id = htol32(request_id);
    
    return r;
    
fail2:
    ExpString_Free(&str);
fail1:
    PacketPassFifoQueueFlow_Free(&r->send_qflow);
    LinkedList0_Remove(&c->replies_list, &r->replies_list_node);
    free(r);
fail0:
    return NULL;
}

static void reply_start (struct reply *r, uint32_t type)
{
    struct reply_header {
        struct packetproto_header pp;
        struct requestproto_header rp;
    } __attribute__((packed));
    
    struct reply_header *header = (void *)r->send_buf;
    header->rp.type = htol32(type);
    int len = ltoh16(header->pp.len) + sizeof(struct packetproto_header);
    
    PacketPassInterface_Sender_Send(r->send_qflow_if, r->send_buf, len);
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

static int init_listen (struct instance *o, NCDValue *listen_addr_arg)
{
    if (NCDValue_Type(listen_addr_arg) != NCDVALUE_LIST) {
        goto bad;
    }
    
    if (NCDValue_ListCount(listen_addr_arg) < 1) {
        goto bad;
    }
    NCDValue *type_arg = NCDValue_ListFirst(listen_addr_arg);
    
    if (!NCDValue_IsStringNoNulls(type_arg)) {
        goto bad;
    }
    const char *type = NCDValue_StringValue(type_arg);
    
    o->unix_socket_path = NULL;
    
    if (!strcmp(type, "unix")) {
        NCDValue *socket_path_arg;
        if (!NCDValue_ListRead(listen_addr_arg, 2, &type_arg, &socket_path_arg)) {
            goto bad;
        }
        
        if (!NCDValue_IsStringNoNulls(socket_path_arg)) {
            goto bad;
        }
        
        // remember socket path
        o->unix_socket_path = NCDValue_StringValue(socket_path_arg);
        
        // make sure socket file doesn't exist
        if (unlink(o->unix_socket_path) < 0 && errno != ENOENT) {
            ModuleLog(o->i, BLOG_ERROR, "unlink failed");
            return 0;
        }
        
        // init listener
        if (!BListener_InitUnix(&o->listener, o->unix_socket_path, o->i->iparams->reactor, o, (BListener_handler)listener_handler)) {
            ModuleLog(o->i, BLOG_ERROR, "BListener_InitUnix failed");
            return 0;
        }
    }
    else if (!strcmp(type, "tcp")) {
        NCDValue *ip_address_arg;
        NCDValue *port_number_arg;
        if (!NCDValue_ListRead(listen_addr_arg, 3, &type_arg, &ip_address_arg, &port_number_arg)) {
            goto bad;
        }
        
        if (!NCDValue_IsStringNoNulls(ip_address_arg) || !NCDValue_IsStringNoNulls(port_number_arg)) {
            goto bad;
        }
        
        BIPAddr ipaddr;
        if (!BIPAddr_Resolve(&ipaddr, NCDValue_StringValue(ip_address_arg), 1)) {
            goto bad;
        }
        
        uintmax_t port;
        if (!parse_unsigned_integer(NCDValue_StringValue(port_number_arg), &port) || port > UINT16_MAX) {
            goto bad;
        }
        
        BAddr addr;
        BAddr_InitFromIpaddrAndPort(&addr, ipaddr, hton16(port));
        
        // init listener
        if (!BListener_Init(&o->listener, addr, o->i->iparams->reactor, o, (BListener_handler)listener_handler)) {
            ModuleLog(o->i, BLOG_ERROR, "BListener_InitUnix failed");
            return 0;
        }
    }
    else {
        goto bad;
    }
    
    return 1;
    
bad:
    ModuleLog(o->i, BLOG_ERROR, "bad listen address argument");
    return 0;
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
    NCDValue *listen_addr_arg;
    NCDValue *request_handler_template_arg;
    NCDValue *args_arg;
    if (!NCDValue_ListRead(i->args, 3, &listen_addr_arg, &request_handler_template_arg, &args_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDValue_IsStringNoNulls(request_handler_template_arg) || NCDValue_Type(args_arg) != NCDVALUE_LIST) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->request_handler_template = NCDValue_StringValue(request_handler_template_arg);
    o->args = args_arg;
    
    // init listener
    if (!init_listen(o, listen_addr_arg)) {
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
    
    if (o->unix_socket_path) {
        // remove socket file
        if (unlink(o->unix_socket_path)) {
            ModuleLog(o->i, BLOG_ERROR, "unlink failed");
        }
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
    
    if (r->terminating) {
        ModuleLog(i, BLOG_ERROR, "request is dying, cannot submit reply");
        goto fail;
    }
    
    struct reply *rpl = reply_init(c, r->request_id, reply_data);
    if (!rpl) {
        ModuleLog(i, BLOG_ERROR, "failed to submit reply");
        goto fail;
    }
    
    reply_start(rpl, REQUESTPROTO_TYPE_SERVER_REPLY);
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
    
    if (r->terminating) {
        ModuleLog(i, BLOG_ERROR, "request is dying, cannot submit finished");
        goto fail;
    }
    
    r->got_finished = 1;
    
    request_terminate(r);
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
