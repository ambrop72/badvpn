/**
 * @file PacketRecvBlocker.c
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

#include <flow/PacketRecvBlocker.h>

static void output_handler_recv (PacketRecvBlocker *o, uint8_t *data)
{
    ASSERT(!o->out_have)
    DebugObject_Access(&o->d_obj);
    
    // remember packet
    o->out_have = 1;
    o->out = data;
    o->out_input_blocking = 0;
}

static void input_handler_done (PacketRecvBlocker *o, int data_len)
{
    ASSERT(o->out_have)
    ASSERT(o->out_input_blocking)
    DebugObject_Access(&o->d_obj);
    
    // have no output packet
    o->out_have = 0;
    PacketRecvInterface_Done(&o->output, data_len);
}

void PacketRecvBlocker_Init (PacketRecvBlocker *o, PacketRecvInterface *input, BPendingGroup *pg)
{
    // init arguments
    o->input = input;
    
    // init output
    PacketRecvInterface_Init(&o->output, PacketRecvInterface_GetMTU(o->input), (PacketRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    // have no output packet
    o->out_have = 0;
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    DebugObject_Init(&o->d_obj);
}

void PacketRecvBlocker_Free (PacketRecvBlocker *o)
{
    DebugObject_Free(&o->d_obj);

    // free output
    PacketRecvInterface_Free(&o->output);
}

PacketRecvInterface * PacketRecvBlocker_GetOutput (PacketRecvBlocker *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}

void PacketRecvBlocker_AllowBlockedPacket (PacketRecvBlocker *o)
{
    DebugObject_Access(&o->d_obj);
    
    if (!o->out_have || o->out_input_blocking) {
        return;
    }
    
    // schedule receive
    PacketRecvInterface_Receiver_Recv(o->input, o->out);
    o->out_input_blocking = 1;
}
