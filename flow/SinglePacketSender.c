/**
 * @file SinglePacketSender.c
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

#include <flow/SinglePacketSender.h>

static void call_handler (SinglePacketSender *o)
{
    #ifndef NDEBUG
    DEAD_ENTER(o->dead)
    #endif
    
    o->handler(o->user);
    
    #ifndef NDEBUG
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(o->dead);
    #endif
}

static void output_handler_done (SinglePacketSender *o)
{
    DebugObject_Access(&o->d_obj);
    
    // notify user
    call_handler(o);
    return;
}

static void job_handler (SinglePacketSender *o)
{
    DebugObject_Access(&o->d_obj);
    
    DEAD_ENTER(o->dead)
    int res = PacketPassInterface_Sender_Send(o->output, o->packet, o->packet_len);
    if (DEAD_LEAVE(o->dead)) {
        return;
    }
    
    ASSERT(res == 0 || res == 1)
    
    if (!res) {
        return;
    }
    
    // notify user
    call_handler(o);
    return;
}

void SinglePacketSender_Init (SinglePacketSender *o, uint8_t *packet, int packet_len, PacketPassInterface *output, SinglePacketSender_handler handler, void *user, BPendingGroup *pg)
{
    ASSERT(packet_len >= 0)
    ASSERT(packet_len <= PacketPassInterface_GetMTU(output))
    
    // init arguments
    o->packet = packet;
    o->packet_len = packet_len;
    o->output = output;
    o->handler = handler;
    o->user = user;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    // init start job
    BPending_Init(&o->start_job, pg, (BPending_handler)job_handler, o);
    BPending_Set(&o->start_job);
    
    DebugObject_Init(&o->d_obj);
}

void SinglePacketSender_Free (SinglePacketSender *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free start job
    BPending_Free(&o->start_job);
    
    // free dead var
    DEAD_KILL(o->dead);
}
