/**
 * @file PacketCopier.c
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

#include <flow/PacketCopier.h>

static void input_handler_send (PacketCopier *o, uint8_t *data, int data_len)
{
    ASSERT(o->in_len == -1)
    ASSERT(data_len >= 0)
    DebugObject_Access(&o->d_obj);
    
    if (!o->out_have) {
        o->in_len = data_len;
        o->in = data;
        return;
    }
    
    memcpy(o->out, data, data_len);
    
    // finish output packet
    PacketRecvInterface_Done(&o->output, data_len);
    
    // finish input packet
    PacketPassInterface_Done(&o->input);
    
    o->out_have = 0;
}

static void input_handler_cancel (PacketCopier *o)
{
    ASSERT(o->in_len >= 0)
    ASSERT(!o->out_have)
    DebugObject_Access(&o->d_obj);
    
    o->in_len = -1;
}

static void output_handler_recv (PacketCopier *o, uint8_t *data)
{
    ASSERT(!o->out_have)
    DebugObject_Access(&o->d_obj);
    
    if (o->in_len < 0) {
        o->out_have = 1;
        o->out = data;
        return;
    }
    
    memcpy(data, o->in, o->in_len);
    
    // finish input packet
    PacketPassInterface_Done(&o->input);
    
    // finish output packet
    PacketRecvInterface_Done(&o->output, o->in_len);
    
    o->in_len = -1;
}

void PacketCopier_Init (PacketCopier *o, int mtu, BPendingGroup *pg)
{
    ASSERT(mtu >= 0)
    
    // init input
    PacketPassInterface_Init(&o->input, mtu, (PacketPassInterface_handler_send)input_handler_send, o, pg);
    PacketPassInterface_EnableCancel(&o->input, (PacketPassInterface_handler_cancel)input_handler_cancel);
    
    // init output
    PacketRecvInterface_Init(&o->output, mtu, (PacketRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    // set no input packet
    o->in_len = -1;
    
    // set no output packet
    o->out_have = 0;
    
    DebugObject_Init(&o->d_obj);
}

void PacketCopier_Free (PacketCopier *o)
{
    DebugObject_Free(&o->d_obj);

    // free output
    PacketRecvInterface_Free(&o->output);
    
    // free input
    PacketPassInterface_Free(&o->input);
}

PacketPassInterface * PacketCopier_GetInput (PacketCopier *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->input;
}

PacketRecvInterface * PacketCopier_GetOutput (PacketCopier *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}
