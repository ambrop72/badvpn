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

static int input_handler_send (PacketCopier *o, uint8_t *data, int data_len)
{
    ASSERT(o->in_len == -1)
    ASSERT(data_len >= 0)
    
    if (!o->out_have) {
        o->in_len = data_len;
        o->in = data;
        return 0;
    }
    
    memcpy(o->out, data, data_len);
    
    o->out_have = 0;
    
    DEAD_ENTER(o->dead)
    PacketRecvInterface_Done(&o->output, data_len);
    if (DEAD_LEAVE(o->dead)) {
        return -1;
    }
    
    return 1;
}

static void input_handler_cancel (PacketCopier *o)
{
    ASSERT(o->in_len >= 0)
    ASSERT(!o->out_have)
    
    o->in_len = -1;
}

static int output_handler_recv (PacketCopier *o, uint8_t *data, int *data_len)
{
    ASSERT(!o->out_have)
    
    if (o->in_len < 0) {
        o->out_have = 1;
        o->out = data;
        return 0;
    }
    
    int len = o->in_len;
    
    memcpy(data, o->in, len);
    
    o->in_len = -1;
    
    DEAD_ENTER(o->dead)
    PacketPassInterface_Done(&o->input);
    if (DEAD_LEAVE(o->dead)) {
        return -1;
    }
    
    *data_len = len;
    return 1;
}

void PacketCopier_Init (PacketCopier *o, int mtu)
{
    ASSERT(mtu >= 0)
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init input
    PacketPassInterface_Init(&o->input, mtu, (PacketPassInterface_handler_send)input_handler_send, o);
    PacketPassInterface_EnableCancel(&o->input, (PacketPassInterface_handler_cancel)input_handler_cancel);
    
    // init output
    PacketRecvInterface_Init(&o->output, mtu, (PacketRecvInterface_handler_recv)output_handler_recv, o);
    
    // set no input packet
    o->in_len = -1;
    
    // set no output packet
    o->out_have = 0;
    
    // init debug object
    DebugObject_Init(&o->d_obj);
}

void PacketCopier_Free (PacketCopier *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);

    // free output
    PacketRecvInterface_Free(&o->output);
    
    // free input
    PacketPassInterface_Free(&o->input);
    
    // free dead var
    DEAD_KILL(o->dead);
}

PacketPassInterface * PacketCopier_GetInput (PacketCopier *o)
{
    return &o->input;
}

PacketRecvInterface * PacketCopier_GetOutput (PacketCopier *o)
{
    return &o->output;
}
