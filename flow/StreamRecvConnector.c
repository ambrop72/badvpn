/**
 * @file StreamRecvConnector.c
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

#include <flow/StreamRecvConnector.h>

static void output_handler_recv (StreamRecvConnector *o, uint8_t *data, int data_avail)
{
    ASSERT(data_avail > 0)
    ASSERT(o->out_avail == -1)
    ASSERT(!o->input || !o->in_blocking)
    
    // remember output packet
    o->out_avail = data_avail;
    o->out = data;
    
    if (o->input) {
        // schedule receive
        StreamRecvInterface_Receiver_Recv(o->input, o->out, o->out_avail);
        o->in_blocking = 1;
    }
}

static void input_handler_done (StreamRecvConnector *o, int data_len)
{
    ASSERT(data_len > 0)
    ASSERT(data_len <= o->out_avail)
    ASSERT(o->out_avail > 0)
    ASSERT(o->input)
    ASSERT(o->in_blocking)
    
    // input not blocking any more
    o->in_blocking = 0;
    
    // have no output packet
    o->out_avail = -1;
    
    // allow output to receive more packets
    StreamRecvInterface_Done(&o->output, data_len);
}

void StreamRecvConnector_Init (StreamRecvConnector *o, BPendingGroup *pg)
{
    // init output
    StreamRecvInterface_Init(&o->output, (StreamRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    // have no output packet
    o->out_avail = -1;
    
    // have no input
    o->input = NULL;
    
    DebugObject_Init(&o->d_obj);
}

void StreamRecvConnector_Free (StreamRecvConnector *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free output
    StreamRecvInterface_Free(&o->output);
}

StreamRecvInterface * StreamRecvConnector_GetOutput (StreamRecvConnector *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}

void StreamRecvConnector_ConnectInput (StreamRecvConnector *o, StreamRecvInterface *input)
{
    ASSERT(!o->input)
    DebugObject_Access(&o->d_obj);
    
    // set input
    o->input = input;
    
    // init input
    StreamRecvInterface_Receiver_Init(o->input, (StreamRecvInterface_handler_done)input_handler_done, o);
    
    // set input not blocking
    o->in_blocking = 0;
    
    // if we have an output packet, schedule receive
    if (o->out_avail > 0) {
        StreamRecvInterface_Receiver_Recv(o->input, o->out, o->out_avail);
        o->in_blocking = 1;
    }
}

void StreamRecvConnector_DisconnectInput (StreamRecvConnector *o)
{
    ASSERT(o->input)
    DebugObject_Access(&o->d_obj);
    
    // set no input
    o->input = NULL;
}
