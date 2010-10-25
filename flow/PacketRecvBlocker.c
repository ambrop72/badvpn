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

static int output_handler_recv (PacketRecvBlocker *o, uint8_t *data, int *data_len)
{
    ASSERT(!o->out_have)
    
    // remember packet
    o->out_have = 1;
    o->out = data;
    o->out_input_blocking = 0;
    
    return 0;
}

static void input_handler_done (PacketRecvBlocker *o, int data_len)
{
    ASSERT(o->out_have)
    ASSERT(o->out_input_blocking)
    
    // have no output packet
    o->out_have = 0;
    
    // inform output we received something
    PacketRecvInterface_Done(&o->output, data_len);
    return;
}

void PacketRecvBlocker_Init (PacketRecvBlocker *o, PacketRecvInterface *input)
{
    // init arguments
    o->input = input;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init output
    PacketRecvInterface_Init(&o->output, PacketRecvInterface_GetMTU(o->input), (PacketRecvInterface_handler_recv)output_handler_recv, o);
    
    // have no output packet
    o->out_have = 0;
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    // init debug object
    DebugObject_Init(&o->d_obj);
}

void PacketRecvBlocker_Free (PacketRecvBlocker *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);

    // free output
    PacketRecvInterface_Free(&o->output);
    
    // free dead var
    DEAD_KILL(o->dead);
}

PacketRecvInterface * PacketRecvBlocker_GetOutput (PacketRecvBlocker *o)
{
    return &o->output;
}

void PacketRecvBlocker_AllowBlockedPacket (PacketRecvBlocker *o)
{
    ASSERT(!PacketRecvInterface_InClient(o->input))
    
    if (!o->out_have || o->out_input_blocking) {
        return;
    }
    
    // receive from input
    int in_len;
    DEAD_ENTER(o->dead)
    int res = PacketRecvInterface_Receiver_Recv(o->input, o->out, &in_len);
    if (DEAD_LEAVE(o->dead)) {
        return;
    }
    
    ASSERT(res == 0 || res == 1)
    
    if (!res) {
        // input blocking, continue in input_handler_done
        o->out_input_blocking = 1;
        return;
    }
    
    // have no output packet
    o->out_have = 0;
    
    // inform output we received something
    PacketRecvInterface_Done(&o->output, in_len);
    return;
}
