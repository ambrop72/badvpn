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
    DEBUGERROR(&o->d_err, o->handler(o->user));
}

static void output_handler_done (SinglePacketSender *o)
{
    DebugObject_Access(&o->d_obj);
    
    // notify user
    call_handler(o);
    return;
}

void SinglePacketSender_Init (SinglePacketSender *o, uint8_t *packet, int packet_len, PacketPassInterface *output, SinglePacketSender_handler handler, void *user, BPendingGroup *pg)
{
    ASSERT(packet_len >= 0)
    ASSERT(packet_len <= PacketPassInterface_GetMTU(output))
    
    // init arguments
    o->output = output;
    o->handler = handler;
    o->user = user;
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    // schedule send
    PacketPassInterface_Sender_Send(o->output, packet, packet_len);
    
    DebugObject_Init(&o->d_obj);
    DebugError_Init(&o->d_err);
}

void SinglePacketSender_Free (SinglePacketSender *o)
{
    DebugError_Free(&o->d_err);
    DebugObject_Free(&o->d_obj);
}
