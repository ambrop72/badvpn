/**
 * @file PacketProtoFlow.c
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

#include <protocol/packetproto.h>
#include <misc/debug.h>

#include <flow/PacketProtoFlow.h>

int PacketProtoFlow_Init (PacketProtoFlow *o, int input_mtu, int num_packets, PacketPassInterface *output, BPendingGroup *pg)
{
    ASSERT(input_mtu >= 0)
    ASSERT(input_mtu <= PACKETPROTO_MAXPAYLOAD)
    ASSERT(num_packets > 0)
    ASSERT(PacketPassInterface_GetMTU(output) >= PACKETPROTO_ENCLEN(input_mtu))
    
    // init async input
    BufferWriter_Init(&o->ainput, input_mtu, pg);
    
    // init encoder
    PacketProtoEncoder_Init(&o->encoder, BufferWriter_GetOutput(&o->ainput), pg);
    
    // init buffer
    if (!PacketBuffer_Init(&o->buffer, PacketProtoEncoder_GetOutput(&o->encoder), output, num_packets, pg)) {
        goto fail0;
    }
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail0:
    PacketProtoEncoder_Free(&o->encoder);
    BufferWriter_Free(&o->ainput);
    return 0;
}

void PacketProtoFlow_Free (PacketProtoFlow *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free buffer
    PacketBuffer_Free(&o->buffer);
    
    // free encoder
    PacketProtoEncoder_Free(&o->encoder);
    
    // free async input
    BufferWriter_Free(&o->ainput);
}

BufferWriter * PacketProtoFlow_GetInput (PacketProtoFlow *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->ainput;
}
