/**
 * @file StreamPacketSender.c
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

#include "StreamPacketSender.h"

static void input_handler_send (StreamPacketSender *o, uint8_t *data, int data_len)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(data_len > 0)
    
    // limit length to MTU and remember
    if (data_len > o->output_mtu) {
        o->sending_len = o->output_mtu;
    } else {
        o->sending_len = data_len;
    }
    
    // send
    PacketPassInterface_Sender_Send(o->output, data, o->sending_len);
}

static void output_handler_done (StreamPacketSender *o)
{
    DebugObject_Access(&o->d_obj);
    
    // done
    StreamPassInterface_Done(&o->input, o->sending_len);
}

void StreamPacketSender_Init (StreamPacketSender *o, PacketPassInterface *output, BPendingGroup *pg)
{
    ASSERT(PacketPassInterface_GetMTU(output) > 0)
    
    // init arguments
    o->output = output;
    
    // remember output MTU
    o->output_mtu = PacketPassInterface_GetMTU(output);
    
    // init input
    StreamPassInterface_Init(&o->input, (StreamPassInterface_handler_send)input_handler_send, o, pg);
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    DebugObject_Init(&o->d_obj);
}

void StreamPacketSender_Free (StreamPacketSender *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free input
    StreamPassInterface_Free(&o->input);
}

StreamPassInterface * StreamPacketSender_GetInput (StreamPacketSender *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->input;
}
