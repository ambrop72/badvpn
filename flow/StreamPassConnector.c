/**
 * @file StreamPassConnector.c
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

#include <flow/StreamPassConnector.h>

static void input_handler_send (StreamPassConnector *o, uint8_t *data, int data_len)
{
    ASSERT(data_len > 0)
    ASSERT(o->in_len == -1)
    DebugObject_Access(&o->d_obj);
    
    // remember input packet
    o->in_len = data_len;
    o->in = data;
    
    if (o->output) {
        // schedule send
        StreamPassInterface_Sender_Send(o->output, o->in, o->in_len);
    }
}

static void output_handler_done (StreamPassConnector *o, int data_len)
{
    ASSERT(data_len > 0)
    ASSERT(data_len <= o->in_len)
    ASSERT(o->in_len > 0)
    ASSERT(o->output)
    DebugObject_Access(&o->d_obj);
    
    // have no input packet
    o->in_len = -1;
    
    // allow input to send more packets
    StreamPassInterface_Done(&o->input, data_len);
}

void StreamPassConnector_Init (StreamPassConnector *o, BPendingGroup *pg)
{
    // init output
    StreamPassInterface_Init(&o->input, (StreamPassInterface_handler_send)input_handler_send, o, pg);
    
    // have no input packet
    o->in_len = -1;
    
    // have no output
    o->output = NULL;
    
    DebugObject_Init(&o->d_obj);
}

void StreamPassConnector_Free (StreamPassConnector *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free output
    StreamPassInterface_Free(&o->input);
}

StreamPassInterface * StreamPassConnector_GetInput (StreamPassConnector *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->input;
}

void StreamPassConnector_ConnectOutput (StreamPassConnector *o, StreamPassInterface *output)
{
    ASSERT(!o->output)
    DebugObject_Access(&o->d_obj);
    
    // set output
    o->output = output;
    
    // init output
    StreamPassInterface_Sender_Init(o->output, (StreamPassInterface_handler_done)output_handler_done, o);
    
    // if we have an input packet, schedule send
    if (o->in_len > 0) {
        StreamPassInterface_Sender_Send(o->output, o->in, o->in_len);
    }
}

void StreamPassConnector_DisconnectOutput (StreamPassConnector *o)
{
    ASSERT(o->output)
    DebugObject_Access(&o->d_obj);
    
    // set no output
    o->output = NULL;
}
