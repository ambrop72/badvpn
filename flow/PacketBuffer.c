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

#include <flow/PacketBuffer.h>

static int call_recv (PacketBuffer *buf, uint8_t *data, int *len);
static int call_send (PacketBuffer *buf, uint8_t *data, int len);
static int try_recv (PacketBuffer *buf);
static int try_send (PacketBuffer *buf);
static void input_handler_done (PacketBuffer *buf, int in_len);
static void output_handler_done (PacketBuffer *buf);
static void job_handler (PacketBuffer *buf);

int call_recv (PacketBuffer *buf, uint8_t *data, int *len)
{
    ASSERT(!PacketRecvInterface_InClient(buf->input))
    
    DEAD_ENTER(buf->dead)
    int res = PacketRecvInterface_Receiver_Recv(buf->input, data, len);
    if (DEAD_LEAVE(buf->dead)) {
        return -1;
    }
    
    ASSERT(res == 0 || res == 1)
    if (res) {
        ASSERT(*len >= 0)
        ASSERT(*len <= buf->input_mtu)
    }
    
    return res;
}

int call_send (PacketBuffer *buf, uint8_t *data, int len)
{
    ASSERT(len >= 0)
    ASSERT(len <= buf->input_mtu)
    ASSERT(!PacketPassInterface_InClient(buf->output))
    
    DEAD_ENTER(buf->dead)
    int res = PacketPassInterface_Sender_Send(buf->output, data, len);
    if (DEAD_LEAVE(buf->dead)) {
        return -1;
    }
    
    ASSERT(res == 0 || res == 1)
    
    return res;
}

int try_recv (PacketBuffer *buf)
{
    ASSERT(buf->buf.input_avail >= buf->input_mtu)
    ASSERT(!PacketRecvInterface_InClient(buf->input))
    ASSERT(!PacketPassInterface_InClient(buf->output))
    
    do {
        // receive packet
        int in_len;
        int res;
        if ((res = call_recv(buf, buf->buf.input_dest, &in_len)) < 0) {
            return -1;
        }
        
        if (!res) {
            // input busy, continue in input_handler_done
            return 0;
        }
        
        // remember if buffer is empty
        int was_empty = (buf->buf.output_avail < 0);
        
        // submit packet to buffer
        ChunkBuffer2_SubmitPacket(&buf->buf, in_len);
        
        // if buffer was empty, start sending
        if (was_empty) {
            if (try_send(buf) < 0) {
                return -1;
            }
        }
    } while (buf->buf.input_avail >= buf->input_mtu);
    
    return 0;
}

int try_send (PacketBuffer *buf)
{
    ASSERT(buf->buf.output_avail >= 0)
    ASSERT(!PacketRecvInterface_InClient(buf->input))
    ASSERT(!PacketPassInterface_InClient(buf->output))
    
    do {
        // send packet
        int res;
        if ((res = call_send(buf, buf->buf.output_dest, buf->buf.output_avail)) < 0) {
            return -1;
        }
        
        if (!res) {
            // output busy, continue in output_handler_done
            return 0;
        }
        
        // remove packet from buffer
        ChunkBuffer2_ConsumePacket(&buf->buf);
    } while (buf->buf.output_avail >= 0);
    
    return 0;
}

void input_handler_done (PacketBuffer *buf, int in_len)
{
    ASSERT(in_len >= 0)
    ASSERT(in_len <= buf->input_mtu)
    ASSERT(!PacketRecvInterface_InClient(buf->input))
    ASSERT(!PacketPassInterface_InClient(buf->output))
    
    // remember if buffer is empty
    int was_empty = (buf->buf.output_avail < 0);
    
    // submit packet to buffer
    ChunkBuffer2_SubmitPacket(&buf->buf, in_len);
    
    // if buffer was empty, try sending
    if (was_empty) {
        if (try_send(buf) < 0) {
            return;
        }
    }
    
    // try receiving more
    if (buf->buf.input_avail >= buf->input_mtu) {
        try_recv(buf);
        return;
    }
}

void output_handler_done (PacketBuffer *buf)
{
    ASSERT(!PacketRecvInterface_InClient(buf->input))
    ASSERT(!PacketPassInterface_InClient(buf->output))
    
    // remember if buffer is full
    int was_full = (buf->buf.input_avail < buf->input_mtu);
    
    // remove packet from buffer
    ChunkBuffer2_ConsumePacket(&buf->buf);
    
    // try sending more
    if (buf->buf.output_avail >= 0) {
        if (try_send(buf) < 0) {
            return;
        }
    }
    
    // try receiving
    if (was_full && buf->buf.input_avail >= buf->input_mtu) {
        try_recv(buf);
        return;
    }
}

void job_handler (PacketBuffer *buf)
{
    try_recv(buf);
    return;
}

int PacketBuffer_Init (PacketBuffer *buf, PacketRecvInterface *input, PacketPassInterface *output, int num_packets, BPendingGroup *pg)
{
    ASSERT(PacketPassInterface_GetMTU(output) >= PacketRecvInterface_GetMTU(input))
    ASSERT(num_packets > 0)
    
    // init arguments
    buf->input = input;
    buf->output = output;
    
    // init dead var
    DEAD_INIT(buf->dead);
    
    // init input
    PacketRecvInterface_Receiver_Init(buf->input, (PacketRecvInterface_handler_done)input_handler_done, buf);
    
    // set input MTU
    buf->input_mtu = PacketRecvInterface_GetMTU(buf->input);
    
    // init output
    PacketPassInterface_Sender_Init(buf->output, (PacketPassInterface_handler_done)output_handler_done, buf);
    
    // allocate buffer
    int num_blocks = CHUNKBUFFER2_MAKE_NUMBLOCKS(buf->input_mtu, num_packets);
    if (!(buf->buf_data = malloc(num_blocks * sizeof(struct ChunkBuffer2_block)))) {
        goto fail0;
    }
    
    // init buffer
    ChunkBuffer2_Init(&buf->buf, buf->buf_data, num_blocks, buf->input_mtu);
    
    // init start job
    BPending_Init(&buf->start_job, pg, (BPending_handler)job_handler, buf);
    BPending_Set(&buf->start_job);
    
    // init debug object
    DebugObject_Init(&buf->d_obj);
    
    return 1;
    
fail0:
    return 0;
}

void PacketBuffer_Free (PacketBuffer *buf)
{
    // free debug object
    DebugObject_Free(&buf->d_obj);
    
    // free start job
    BPending_Free(&buf->start_job);
    
    // free buffer
    free(buf->buf_data);
    
    // free dead var
    DEAD_KILL(buf->dead);
}
