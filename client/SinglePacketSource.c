/**
 * @file SinglePacketSource.c
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

#include <string.h>

#include <misc/debug.h>

#include "SinglePacketSource.h"

static void output_handler_recv (SinglePacketSource *o, uint8_t *data)
{
    DebugObject_Access(&o->d_obj);
    
    // if we already sent one packet, stop
    if (o->sent) {
        return;
    }
    
    // set sent
    o->sent = 1;
    
    // write packet
    memcpy(data, o->packet, o->packet_len);
    
    // done
    PacketRecvInterface_Done(&o->output, o->packet_len);
}

void SinglePacketSource_Init (SinglePacketSource *o, uint8_t *packet, int packet_len, BPendingGroup *pg)
{
    ASSERT(packet_len >= 0)
    
    // init arguments
    o->packet = packet;
    o->packet_len = packet_len;
    
    // set not sent
    o->sent = 0;
    
    // init output
    PacketRecvInterface_Init(&o->output, o->packet_len, (PacketRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    DebugObject_Init(&o->d_obj);
}

void SinglePacketSource_Free (SinglePacketSource *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free output
    PacketRecvInterface_Free(&o->output);
}

PacketRecvInterface * SinglePacketSource_GetOutput (SinglePacketSource *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}
