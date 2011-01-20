/**
 * @file PacketPassConnector.c
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

#include <flow/PacketPassConnector.h>

static void input_handler_send (PacketPassConnector *o, uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= o->input_mtu)
    ASSERT(o->in_len == -1)
    DebugObject_Access(&o->d_obj);
    
    // remember input packet
    o->in_len = data_len;
    o->in = data;
    
    if (o->output) {
        // schedule send
        PacketPassInterface_Sender_Send(o->output, o->in, o->in_len);
    }
}

static void output_handler_done (PacketPassConnector *o)
{
    ASSERT(o->in_len >= 0)
    ASSERT(o->output)
    DebugObject_Access(&o->d_obj);
    
    // have no input packet
    o->in_len = -1;
    
    // allow input to send more packets
    PacketPassInterface_Done(&o->input);
}

void PacketPassConnector_Init (PacketPassConnector *o, int mtu, BPendingGroup *pg)
{
    ASSERT(mtu >= 0)
    
    // init arguments
    o->input_mtu = mtu;
    
    // init input
    PacketPassInterface_Init(&o->input, o->input_mtu, (PacketPassInterface_handler_send)input_handler_send, o, pg);
    
    // have no input packet
    o->in_len = -1;
    
    // have no output
    o->output = NULL;
    
    DebugObject_Init(&o->d_obj);
}

void PacketPassConnector_Free (PacketPassConnector *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free input
    PacketPassInterface_Free(&o->input);
}

PacketPassInterface * PacketPassConnector_GetInput (PacketPassConnector *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->input;
}

void PacketPassConnector_ConnectOutput (PacketPassConnector *o, PacketPassInterface *output)
{
    ASSERT(!o->output)
    ASSERT(PacketPassInterface_GetMTU(output) >= o->input_mtu)
    DebugObject_Access(&o->d_obj);
    
    // set output
    o->output = output;
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    // if we have an input packet, schedule send
    if (o->in_len >= 0) {
        PacketPassInterface_Sender_Send(o->output, o->in, o->in_len);
    }
}

void PacketPassConnector_DisconnectOutput (PacketPassConnector *o)
{
    ASSERT(o->output)
    DebugObject_Access(&o->d_obj);
    
    // set no output
    o->output = NULL;
}
