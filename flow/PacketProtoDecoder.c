/**
 * @file PacketProtoDecoder.c
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
#include <string.h>

#include <misc/debug.h>
#include <misc/byteorder.h>
#include <misc/minmax.h>

#include <flow/PacketProtoDecoder.h>

static void report_error (PacketProtoDecoder *enc, int error);
static void process_data (PacketProtoDecoder *enc);
static void input_handler_done (PacketProtoDecoder *enc, int data_len);
static void output_handler_done (PacketProtoDecoder *enc);

void report_error (PacketProtoDecoder *enc, int error)
{
    #ifndef NDEBUG
    DEAD_ENTER(enc->d_dead)
    #endif
    
    FlowErrorReporter_ReportError(&enc->rep, &error);
    
    #ifndef NDEBUG
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(enc->d_dead);
    #endif
}

void process_data (PacketProtoDecoder *enc)
{
    do {
        uint8_t *data = enc->buf + enc->buf_start;
        int left = enc->buf_used;
        
        // check if header was received
        if (left < sizeof(struct packetproto_header)) {
            break;
        }
        struct packetproto_header *header = (struct packetproto_header *)data;
        data += sizeof(struct packetproto_header);
        left -= sizeof(struct packetproto_header);
        int data_len = ltoh16(header->len);
        
        // check data length
        if (data_len > enc->output_mtu) {
            report_error(enc, PACKETPROTODECODER_ERROR_TOOLONG);
            return;
        }
        
        // check if whole packet was received
        if (left < data_len) {
            break;
        }
        
        // update buffer
        enc->buf_start += sizeof(struct packetproto_header) + data_len;
        enc->buf_used -= sizeof(struct packetproto_header) + data_len;
        
        // submit packet
        PacketPassInterface_Sender_Send(enc->output, data, data_len);
        return;
    } while (0);
    
    // if we reached the end of the buffer, wrap around to allow more data to be received
    if (enc->buf_start + enc->buf_used == enc->buf_size) {
        memmove(enc->buf, enc->buf + enc->buf_start, enc->buf_used);
        enc->buf_start = 0;
    }
    
    // receive data
    StreamRecvInterface_Receiver_Recv(enc->input, enc->buf + (enc->buf_start + enc->buf_used), enc->buf_size - ((enc->buf_start + enc->buf_used)));
}

static void input_handler_done (PacketProtoDecoder *enc, int data_len)
{
    ASSERT(data_len > 0)
    ASSERT(data_len <= enc->buf_size - (enc->buf_start + enc->buf_used))
    DebugObject_Access(&enc->d_obj);
    
    // update buffer
    enc->buf_used += data_len;
    
    // process data
    process_data(enc);
    return;
}

void output_handler_done (PacketProtoDecoder *enc)
{
    DebugObject_Access(&enc->d_obj);
    
    // process data
    process_data(enc);
    return;
}

int PacketProtoDecoder_Init (PacketProtoDecoder *enc, FlowErrorReporter rep, StreamRecvInterface *input, PacketPassInterface *output, BPendingGroup *pg) 
{
    // init arguments
    enc->rep = rep;
    enc->input = input;
    enc->output = output;
    
    // init input
    StreamRecvInterface_Receiver_Init(enc->input, (StreamRecvInterface_handler_done)input_handler_done, enc);
    
    // init output
    PacketPassInterface_Sender_Init(enc->output, (PacketPassInterface_handler_done)output_handler_done, enc);
    
    // set output MTU, limit by maximum payload size
    enc->output_mtu = BMIN(PacketPassInterface_GetMTU(enc->output), PACKETPROTO_MAXPAYLOAD);
    
    // init buffer state
    enc->buf_size = PACKETPROTO_ENCLEN(enc->output_mtu);
    enc->buf_start = 0;
    enc->buf_used = 0;
    
    // allocate buffer
    if (!(enc->buf = malloc(enc->buf_size))) {
        goto fail0;
    }
    
    // start receiving
    StreamRecvInterface_Receiver_Recv(enc->input, enc->buf, enc->buf_size);
    
    DebugObject_Init(&enc->d_obj);
    #ifndef NDEBUG
    DEAD_INIT(enc->d_dead);
    #endif
    
    return 1;
    
fail0:
    return 0;
}

void PacketProtoDecoder_Free (PacketProtoDecoder *enc)
{
    DebugObject_Free(&enc->d_obj);
    #ifndef NDEBUG
    DEAD_KILL(enc->d_dead);
    #endif
    
    // free buffer
    free(enc->buf);
}

void PacketProtoDecoder_Reset (PacketProtoDecoder *enc)
{
    DebugObject_Access(&enc->d_obj);
    
    enc->buf_start += enc->buf_used;
    enc->buf_used = 0;
}
