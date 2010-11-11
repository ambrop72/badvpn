/**
 * @file KeepaliveIO.c
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

#include <misc/debug.h>

#include <flow/KeepaliveIO.h>

static void keepalive_handler (KeepaliveIO *o)
{
    DebugObject_Access(&o->d_obj);
    
    PacketRecvBlocker_AllowBlockedPacket(&o->ka_blocker);
}

int KeepaliveIO_Init (KeepaliveIO *o, BReactor *reactor, PacketPassInterface *output, PacketRecvInterface *keepalive_input, btime_t keepalive_interval_ms)
{
    ASSERT(PacketRecvInterface_GetMTU(keepalive_input) <= PacketPassInterface_GetMTU(output))
    ASSERT(keepalive_interval_ms > 0)
    
    // set arguments
    o->reactor = reactor;
    
    // init keep-alive sender
    PacketPassInactivityMonitor_Init(&o->kasender, output, o->reactor, keepalive_interval_ms, (PacketPassInactivityMonitor_handler)keepalive_handler, o);
    
    // init queue
    PacketPassPriorityQueue_Init(&o->queue, PacketPassInactivityMonitor_GetInput(&o->kasender), BReactor_PendingGroup(o->reactor), 0);
    
    // init keepalive flow
    PacketPassPriorityQueueFlow_Init(&o->ka_qflow, &o->queue, -1);
    
    // init keepalive blocker
    PacketRecvBlocker_Init(&o->ka_blocker, keepalive_input, BReactor_PendingGroup(reactor));
    
    // init keepalive buffer
    if (!SinglePacketBuffer_Init(&o->ka_buffer, PacketRecvBlocker_GetOutput(&o->ka_blocker), PacketPassPriorityQueueFlow_GetInput(&o->ka_qflow), BReactor_PendingGroup(o->reactor))) {
        goto fail1;
    }
    
    // init user flow
    PacketPassPriorityQueueFlow_Init(&o->user_qflow, &o->queue, 0);
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    PacketRecvBlocker_Free(&o->ka_blocker);
    PacketPassPriorityQueueFlow_Free(&o->ka_qflow);
    PacketPassPriorityQueue_Free(&o->queue);
    PacketPassInactivityMonitor_Free(&o->kasender);
    return 0;
}

void KeepaliveIO_Free (KeepaliveIO *o)
{
    DebugObject_Free(&o->d_obj);

    // allow freeing queue flows
    PacketPassPriorityQueue_PrepareFree(&o->queue);
    
    // free user flow
    PacketPassPriorityQueueFlow_Free(&o->user_qflow);
    
    // free keepalive buffer
    SinglePacketBuffer_Free(&o->ka_buffer);
    
    // free keepalive blocker
    PacketRecvBlocker_Free(&o->ka_blocker);
    
    // free keepalive flow
    PacketPassPriorityQueueFlow_Free(&o->ka_qflow);
    
    // free queue
    PacketPassPriorityQueue_Free(&o->queue);
    
    // free keep-alive sender
    PacketPassInactivityMonitor_Free(&o->kasender);
}

PacketPassInterface * KeepaliveIO_GetInput (KeepaliveIO *o)
{
    DebugObject_Access(&o->d_obj);
    
    return PacketPassPriorityQueueFlow_GetInput(&o->user_qflow);
}
