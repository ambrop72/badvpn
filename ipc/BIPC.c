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
#define COMPONENT_DECODER 3

static void error_handler (BIPC *o, int component, const void *data)
{
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

static int init_io (BIPC *o, int send_mtu, PacketPassInterface *recv_if, BReactor *reactor)
{
    // init error domain
    FlowErrorDomain_Init(&o->domain, (FlowErrorDomain_handler)error_handler, o);
    
    // init sending
    StreamSocketSink_Init(&o->send_sink, FlowErrorReporter_Create(&o->domain, COMPONENT_SINK), &o->sock);
    PacketStreamSender_Init(&o->send_pss, StreamSocketSink_GetInput(&o->send_sink), PACKETPROTO_ENCLEN(send_mtu));
    PacketCopier_Init(&o->send_copier, send_mtu, BReactor_PendingGroup(reactor));
    PacketProtoEncoder_Init(&o->send_encoder, PacketCopier_GetOutput(&o->send_copier));
    if (!SinglePacketBuffer_Init(&o->send_buf, PacketProtoEncoder_GetOutput(&o->send_encoder), PacketStreamSender_GetInput(&o->send_pss), BReactor_PendingGroup(reactor))) {
        goto fail1;
    }
    
    // init receiving
    StreamSocketSource_Init(&o->recv_source, FlowErrorReporter_Create(&o->domain, COMPONENT_SOURCE), &o->sock);
    if (!PacketProtoDecoder_Init(&o->recv_decoder, FlowErrorReporter_Create(&o->domain, COMPONENT_DECODER), StreamSocketSource_GetOutput(&o->recv_source), recv_if, BReactor_PendingGroup(reactor))) {
        goto fail2;
    }
    
    return 1;
    
fail2:
    StreamSocketSource_Free(&o->recv_source);
    SinglePacketBuffer_Free(&o->send_buf);
fail1:
    PacketProtoEncoder_Free(&o->send_encoder);
    PacketCopier_Free(&o->send_copier);
    PacketStreamSender_Free(&o->send_pss);
    StreamSocketSink_Free(&o->send_sink);
    return 0;
}

static void free_io (BIPC *o)
{
    // free receiving
    PacketProtoDecoder_Free(&o->recv_decoder);
    StreamSocketSource_Free(&o->recv_source);
    
    // free sending
    SinglePacketBuffer_Free(&o->send_buf);
    PacketProtoEncoder_Free(&o->send_encoder);
    PacketCopier_Free(&o->send_copier);
    PacketStreamSender_Free(&o->send_pss);
    StreamSocketSink_Free(&o->send_sink);
}

int BIPC_InitConnect (BIPC *o, const char *path, int send_mtu, PacketPassInterface *recv_if, BIPC_handler handler, void *user, BReactor *reactor)
{
    ASSERT(send_mtu >= 0)
    ASSERT(send_mtu <= PACKETPROTO_MAXPAYLOAD)
    ASSERT(PacketPassInterface_GetMTU(recv_if) >= 0)
    ASSERT(PacketPassInterface_GetMTU(recv_if) <= PACKETPROTO_MAXPAYLOAD)
    
    // init arguments
    o->handler = handler;
    o->user = user;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init socket
    if (BSocket_Init(&o->sock, reactor, BADDR_TYPE_UNIX, BSOCKET_TYPE_STREAM) < 0) {
        DEBUG("BSocket_Init failed");
        goto fail0;
    }
    
    // connect socket
    if (BSocket_ConnectUnix(&o->sock, path) < 0) {
        DEBUG("BSocket_ConnectUnix failed (%d)", BSocket_GetError(&o->sock));
        goto fail1;
    }
    
    // init I/O
    if (!init_io(o, send_mtu, recv_if, reactor)) {
        goto fail1;
    }
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    BSocket_Free(&o->sock);
fail0:
    return 0;
}

int BIPC_InitAccept (BIPC *o, BIPCServer *server, int send_mtu, PacketPassInterface *recv_if, BIPC_handler handler, void *user, BReactor *reactor)
{
    ASSERT(send_mtu >= 0)
    ASSERT(send_mtu <= PACKETPROTO_MAXPAYLOAD)
    ASSERT(PacketPassInterface_GetMTU(recv_if) >= 0)
    ASSERT(PacketPassInterface_GetMTU(recv_if) <= PACKETPROTO_MAXPAYLOAD)
    
    // init arguments
    o->handler = handler;
    o->user = user;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // accept socket
    if (!Listener_Accept(&server->listener, &o->sock, NULL)) {
        DEBUG("Listener_Accept failed");
        goto fail0;
    }
    
    // init I/O
    if (!init_io(o, send_mtu, recv_if, reactor)) {
        goto fail1;
    }
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    BSocket_Free(&o->sock);
fail0:
    return 0;
}

void BIPC_Free (BIPC *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free I/O
    free_io(o);
    
    // free socket
    BSocket_Free(&o->sock);
    
    // free dead var
    DEAD_KILL(o->dead);
}

PacketPassInterface * BIPC_GetSendInterface (BIPC *o)
{
    DebugObject_Access(&o->d_obj);
    
    return PacketCopier_GetInput(&o->send_copier);
}
