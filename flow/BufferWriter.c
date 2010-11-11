/**
 * @file BufferWriter.c
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

#include <flow/BufferWriter.h>

static void output_handler_recv (BufferWriter *o, uint8_t *data)
{
    ASSERT(!o->out_have)
    
    // set output packet
    o->out_have = 1;
    o->out = data;
}

void BufferWriter_Init (BufferWriter *o, int mtu, BPendingGroup *pg)
{
    ASSERT(mtu >= 0)
    
    // init output
    PacketRecvInterface_Init(&o->recv_interface, mtu, (PacketRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    // set no output packet
    o->out_have = 0;
    
    DebugObject_Init(&o->d_obj);
    #ifndef NDEBUG
    o->d_mtu = mtu;
    o->d_writing = 0;
    #endif
}

void BufferWriter_Free (BufferWriter *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free output
    PacketRecvInterface_Free(&o->recv_interface);
}

PacketRecvInterface * BufferWriter_GetOutput (BufferWriter *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->recv_interface;
}

int BufferWriter_StartPacket (BufferWriter *o, uint8_t **buf)
{
    ASSERT(!o->d_writing)
    DebugObject_Access(&o->d_obj);
    
    if (!o->out_have) {
        return 0;
    }
    
    if (buf) {
        *buf = o->out;
    }
    
    #ifndef NDEBUG
    o->d_writing = 1;
    #endif
    
    return 1;
}

void BufferWriter_EndPacket (BufferWriter *o, int len)
{
    ASSERT(len >= 0)
    ASSERT(len <= o->d_mtu)
    ASSERT(o->out_have)
    ASSERT(o->d_writing)
    DebugObject_Access(&o->d_obj);
    
    // set no output packet
    o->out_have = 0;
    
    // finish packet
    PacketRecvInterface_Done(&o->recv_interface, len);
    
    #ifndef NDEBUG
    o->d_writing = 0;
    #endif
}
