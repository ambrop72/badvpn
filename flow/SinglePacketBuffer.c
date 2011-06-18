/**
 * @file SinglePacketBuffer.c
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

#include <stdlib.h>

#include <misc/debug.h>
#include <misc/balloc.h>

#include <flow/SinglePacketBuffer.h>

static void input_handler_done (SinglePacketBuffer *o, int in_len)
{
    DebugObject_Access(&o->d_obj);
    
    PacketPassInterface_Sender_Send(o->output, o->buf, in_len);
}

static void output_handler_done (SinglePacketBuffer *o)
{
    DebugObject_Access(&o->d_obj);
    
    PacketRecvInterface_Receiver_Recv(o->input, o->buf);
}

int SinglePacketBuffer_Init (SinglePacketBuffer *o, PacketRecvInterface *input, PacketPassInterface *output, BPendingGroup *pg) 
{
    ASSERT(PacketPassInterface_GetMTU(output) >= PacketRecvInterface_GetMTU(input))
    
    // init arguments
    o->input = input;
    o->output = output;
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    // init buffer
    if (!(o->buf = BAlloc(PacketRecvInterface_GetMTU(o->input)))) {
        goto fail1;
    }
    
    // schedule receive
    PacketRecvInterface_Receiver_Recv(o->input, o->buf);
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    return 0;
}

void SinglePacketBuffer_Free (SinglePacketBuffer *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free buffer
    BFree(o->buf);
}
