/**
 * @file PacketRecvConnector.c
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

#include <stddef.h>

#include <misc/debug.h>

#include <flow/PacketRecvConnector.h>

static void output_handler_recv (PacketRecvConnector *o, uint8_t *data)
{
    ASSERT(!o->out_have)
    DebugObject_Access(&o->d_obj);
    
    // remember output packet
    o->out_have = 1;
    o->out = data;
    
    if (o->input) {
        // schedule receive
        PacketRecvInterface_Receiver_Recv(o->input, o->out);
    }
}

static void input_handler_done (PacketRecvConnector *o, int data_len)
{
    ASSERT(o->out_have)
    ASSERT(o->input)
    DebugObject_Access(&o->d_obj);
    
    // have no output packet
    o->out_have = 0;
    
    // allow output to receive more packets
    PacketRecvInterface_Done(&o->output, data_len);
}

void PacketRecvConnector_Init (PacketRecvConnector *o, int mtu, BPendingGroup *pg)
{
    ASSERT(mtu >= 0)
    
    // init arguments
    o->output_mtu = mtu;
    
    // init output
    PacketRecvInterface_Init(&o->output, o->output_mtu, (PacketRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    // have no output packet
    o->out_have = 0;
    
    // have no input
    o->input = NULL;
    
    DebugObject_Init(&o->d_obj);
}

void PacketRecvConnector_Free (PacketRecvConnector *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free output
    PacketRecvInterface_Free(&o->output);
}

PacketRecvInterface * PacketRecvConnector_GetOutput (PacketRecvConnector *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}

void PacketRecvConnector_ConnectInput (PacketRecvConnector *o, PacketRecvInterface *input)
{
    ASSERT(!o->input)
    ASSERT(PacketRecvInterface_GetMTU(input) <= o->output_mtu)
    DebugObject_Access(&o->d_obj);
    
    // set input
    o->input = input;
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    // if we have an output packet, schedule receive
    if (o->out_have) {
        PacketRecvInterface_Receiver_Recv(o->input, o->out);
    }
}

void PacketRecvConnector_DisconnectInput (PacketRecvConnector *o)
{
    ASSERT(o->input)
    DebugObject_Access(&o->d_obj);
    
    // set no input
    o->input = NULL;
}
