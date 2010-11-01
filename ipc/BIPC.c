/**
 * @file BIPC.c
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

#include <ipc/BIPC.h>

#define COMPONENT_SOURCE 1
#define COMPONENT_SINK 2

static void error_handler (BIPC *o, int component, const void *data)
{
    ASSERT(component == COMPONENT_SOURCE || component == COMPONENT_SINK)
    DebugObject_Access(&o->d_obj);
    
    #ifndef NDEBUG
    DEAD_ENTER(o->dead)
    #endif
    
    o->handler(o->user);
    
    #ifndef NDEBUG
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(o->dead);
    #endif
}

int BIPC_InitConnect (BIPC *o, const char *path, int send_mtu, int recv_mtu, BIPC_handler handler, void *user, BReactor *reactor)
{
    ASSERT(send_mtu >= 0)
    ASSERT(recv_mtu >= 0)
    
    // init arguments
    o->handler = handler;
    o->user = user;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init socket
    if (BSocket_Init(&o->sock, reactor, BADDR_TYPE_UNIX, BSOCKET_TYPE_SEQPACKET) < 0) {
        DEBUG("BSocket_Init failed");
        goto fail0;
    }
    
    // connect socket
    if (BSocket_ConnectUnix(&o->sock, path) < 0) {
        DEBUG("BSocket_ConnectUnix failed (%d)", BSocket_GetError(&o->sock));
        goto fail1;
    }
    
    // init error domain
    FlowErrorDomain_Init(&o->domain, (FlowErrorDomain_handler)error_handler, o);
    
    // init sink
    SeqPacketSocketSink_Init(&o->sink, FlowErrorReporter_Create(&o->domain, COMPONENT_SINK), &o->sock, send_mtu);
    
    // init source
    SeqPacketSocketSource_Init(&o->source, FlowErrorReporter_Create(&o->domain, COMPONENT_SOURCE), &o->sock, recv_mtu);
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    BSocket_Free(&o->sock);
fail0:
    return 0;
}

int BIPC_InitAccept (BIPC *o, BIPCServer *server, int send_mtu, int recv_mtu, BIPC_handler handler, void *user)
{
    ASSERT(send_mtu >= 0)
    ASSERT(recv_mtu >= 0)
    
    // init arguments
    o->handler = handler;
    o->user = user;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // accept socket
    if (Listener_Accept(&server->listener, &o->sock, NULL) < 0) {
        DEBUG("Listener_Accept failed");
        goto fail0;
    }
    
    // init error domain
    FlowErrorDomain_Init(&o->domain, (FlowErrorDomain_handler)error_handler, o);
    
    // init sink
    SeqPacketSocketSink_Init(&o->sink, FlowErrorReporter_Create(&o->domain, COMPONENT_SINK), &o->sock, send_mtu);
    
    // init source
    SeqPacketSocketSource_Init(&o->source, FlowErrorReporter_Create(&o->domain, COMPONENT_SOURCE), &o->sock, recv_mtu);
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail0:
    return 0;
}

void BIPC_Free (BIPC *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free source
    SeqPacketSocketSource_Free(&o->source);
    
    // free sink
    SeqPacketSocketSink_Free(&o->sink);
    
    // free socket
    BSocket_Free(&o->sock);
    
    // free dead var
    DEAD_KILL(o->dead);
}

PacketPassInterface * BIPC_GetSendInterface (BIPC *o)
{
    DebugObject_Access(&o->d_obj);
    
    return SeqPacketSocketSink_GetInput(&o->sink);
}

PacketRecvInterface * BIPC_GetRecvInterface (BIPC *o)
{
    DebugObject_Access(&o->d_obj);
    
    return SeqPacketSocketSource_GetOutput(&o->source);
}
