/**
 * @file PacketBuffer.c
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

#include <flow/PacketBuffer.h>

static void input_handler_done (PacketBuffer *buf, int in_len);
static void output_handler_done (PacketBuffer *buf);

void input_handler_done (PacketBuffer *buf, int in_len)
{
    ASSERT(in_len >= 0)
    ASSERT(in_len <= buf->input_mtu)
    DebugObject_Access(&buf->d_obj);
    
    // remember if buffer is empty
    int was_empty = (buf->buf.output_avail < 0);
    
    // submit packet to buffer
    ChunkBuffer2_SubmitPacket(&buf->buf, in_len);
    
    // if there is space, schedule receive
    if (buf->buf.input_avail >= buf->input_mtu) {
        PacketRecvInterface_Receiver_Recv(buf->input, buf->buf.input_dest);
    }
    
    // if buffer was empty, schedule send
    if (was_empty) {
        PacketPassInterface_Sender_Send(buf->output, buf->buf.output_dest, buf->buf.output_avail);
    }
}

void output_handler_done (PacketBuffer *buf)
{
    DebugObject_Access(&buf->d_obj);
    
    // remember if buffer is full
    int was_full = (buf->buf.input_avail < buf->input_mtu);
    
    // remove packet from buffer
    ChunkBuffer2_ConsumePacket(&buf->buf);
    
    // if buffer was full and there is space, schedule receive
    if (was_full && buf->buf.input_avail >= buf->input_mtu) {
        PacketRecvInterface_Receiver_Recv(buf->input, buf->buf.input_dest);
    }
    
    // if there is more data, schedule send
    if (buf->buf.output_avail >= 0) {
        PacketPassInterface_Sender_Send(buf->output, buf->buf.output_dest, buf->buf.output_avail);
    }
}

int PacketBuffer_Init (PacketBuffer *buf, PacketRecvInterface *input, PacketPassInterface *output, int num_packets, BPendingGroup *pg)
{
    ASSERT(PacketPassInterface_GetMTU(output) >= PacketRecvInterface_GetMTU(input))
    ASSERT(num_packets > 0)
    
    // init arguments
    buf->input = input;
    buf->output = output;
    
    // init input
    PacketRecvInterface_Receiver_Init(buf->input, (PacketRecvInterface_handler_done)input_handler_done, buf);
    
    // set input MTU
    buf->input_mtu = PacketRecvInterface_GetMTU(buf->input);
    
    // init output
    PacketPassInterface_Sender_Init(buf->output, (PacketPassInterface_handler_done)output_handler_done, buf);
    
    // allocate buffer
    int num_blocks = ChunkBuffer2_calc_blocks(buf->input_mtu, num_packets);
    if (num_blocks < 0) {
        goto fail0;
    }
    if (!(buf->buf_data = BAllocArray(num_blocks, sizeof(buf->buf_data[0])))) {
        goto fail0;
    }
    
    // init buffer
    ChunkBuffer2_Init(&buf->buf, buf->buf_data, num_blocks, buf->input_mtu);
    
    // schedule receive
    PacketRecvInterface_Receiver_Recv(buf->input, buf->buf.input_dest);
    
    DebugObject_Init(&buf->d_obj);
    
    return 1;
    
fail0:
    return 0;
}

void PacketBuffer_Free (PacketBuffer *buf)
{
    DebugObject_Free(&buf->d_obj);
    
    // free buffer
    BFree(buf->buf_data);
}
