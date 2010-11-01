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
static int call_recv (PacketProtoDecoder *enc, uint8_t *data, int avail);
static int call_send (PacketProtoDecoder *enc, uint8_t *data, int len);
static void receive_data (PacketProtoDecoder *enc);
static void input_handler_done (PacketProtoDecoder *enc, int data_len);
static int parse_and_send (PacketProtoDecoder *enc);
static void output_handler_done (PacketProtoDecoder *enc);
static void job_handler (PacketProtoDecoder *enc);

void report_error (PacketProtoDecoder *enc, int error)
{
    #ifndef NDEBUG
    DEAD_ENTER(enc->dead)
    #endif
    
    FlowErrorReporter_ReportError(&enc->rep, &error);
    
    #ifndef NDEBUG
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(enc->dead);
    #endif
}

int call_recv (PacketProtoDecoder *enc, uint8_t *data, int avail)
{
    ASSERT(avail > 0)
    ASSERT(!StreamRecvInterface_InClient(enc->input))
    
    DEAD_ENTER(enc->dead)
    int res = StreamRecvInterface_Receiver_Recv(enc->input, data, avail);
    if (DEAD_LEAVE(enc->dead)) {
        return -1;
    }
    
    ASSERT(res >= 0)
    ASSERT(res <= avail)
    
    return res;
}

int call_send (PacketProtoDecoder *enc, uint8_t *data, int len)
{
    ASSERT(len >= 0)
    ASSERT(len <= enc->output_mtu)
    ASSERT(!PacketPassInterface_InClient(enc->output))
    
    DEAD_ENTER(enc->dead)
    int res = PacketPassInterface_Sender_Send(enc->output, data, len);
    if (DEAD_LEAVE(enc->dead)) {
        return -1;
    }
    
    ASSERT(res == 0 || res == 1)
    
    return res;
}

void receive_data (PacketProtoDecoder *enc)
{
    ASSERT(!enc->receiving)
    ASSERT(!enc->sending)
    ASSERT(enc->buf_start + enc->buf_used < enc->buf_size)
    ASSERT(!StreamRecvInterface_InClient(enc->input))
    ASSERT(!PacketPassInterface_InClient(enc->output))
    
    do {
        // receive data
        int res;
        if ((res = call_recv(
            enc,
            enc->buf + (enc->buf_start + enc->buf_used),
            enc->buf_size - (enc->buf_start + enc->buf_used)
        )) < 0) {
            return;
        }
        
        if (res == 0) {
            // input busy, continue in input_handler_done
            enc->receiving = 1;
            break;
        }
        
        // update buffer
        enc->buf_used += res;
        
        // parse and send data
        if (parse_and_send(enc) < 0) {
            return;
        }
    } while (!enc->sending && enc->buf_start + enc->buf_used < enc->buf_size);
}

static void input_handler_done (PacketProtoDecoder *enc, int data_len)
{
    ASSERT(enc->receiving)
    ASSERT(!enc->sending)
    ASSERT(enc->buf_start + enc->buf_used < enc->buf_size)
    ASSERT(data_len > 0)
    ASSERT(data_len <= enc->buf_size - (enc->buf_start + enc->buf_used))
    ASSERT(!StreamRecvInterface_InClient(enc->input))
    ASSERT(!PacketPassInterface_InClient(enc->output))
    
    // set not receiving
    enc->receiving = 0;
    
    // update buffer
    enc->buf_used += data_len;
    
    // parse and send data
    if (parse_and_send(enc) < 0) {
        return;
    }
    
    // continue receiving
    if (!enc->sending && enc->buf_start + enc->buf_used < enc->buf_size) {
        receive_data(enc);
        return;
    }
}

int parse_and_send (PacketProtoDecoder *enc)
{
    ASSERT(!enc->sending)
    ASSERT(!StreamRecvInterface_InClient(enc->input))
    ASSERT(!PacketPassInterface_InClient(enc->output))
    
    while (1) {
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
            return -1;
        }
        
        // check if whole packet was received
        if (left < data_len) {
            break;
        }
        
        // update buffer
        enc->buf_start += sizeof(struct packetproto_header) + data_len;
        enc->buf_used -= sizeof(struct packetproto_header) + data_len;
        
        // submit packet
        int res;
        if ((res = call_send(enc, data, data_len)) < 0) {
            return -1;
        }
        
        if (!res) {
            // output busy, continue in output_handler_done
            enc->sending = 1;
            return 0;
        }
    }
    
    // if we reached the end of the buffer, wrap around to allow more data to be received
    if (enc->buf_start + enc->buf_used == enc->buf_size) {
        memmove(enc->buf, enc->buf + enc->buf_start, enc->buf_used);
        enc->buf_start = 0;
    }
    
    return 0;
}

void output_handler_done (PacketProtoDecoder *enc)
{
    ASSERT(enc->sending)
    ASSERT(!StreamRecvInterface_InClient(enc->input))
    ASSERT(!PacketPassInterface_InClient(enc->output))
    
    // set not sending
    enc->sending = 0;
    
    // continue parsing and sending
    if (parse_and_send(enc) < 0) {
        return;
    }
    
    // continue receiving
    if (!enc->receiving && !enc->sending && enc->buf_start + enc->buf_used < enc->buf_size) {
        receive_data(enc);
        return;
    }
}

void job_handler (PacketProtoDecoder *enc)
{
    receive_data(enc);
    return;
}

int PacketProtoDecoder_Init (PacketProtoDecoder *enc, FlowErrorReporter rep, StreamRecvInterface *input, PacketPassInterface *output, BPendingGroup *pg) 
{
    // init arguments
    enc->rep = rep;
    enc->input = input;
    enc->output = output;
    
    // init dead var
    DEAD_INIT(enc->dead);
    
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
    
    // set not receiving
    enc->receiving = 0;
    
    // set not sending
    enc->sending = 0;
    
    // init start job
    BPending_Init(&enc->start_job, pg, (BPending_handler)job_handler, enc);
    BPending_Set(&enc->start_job);
    
    // init debug object
    DebugObject_Init(&enc->d_obj);
    
    return 1;
    
fail0:
    return 0;
}

void PacketProtoDecoder_Free (PacketProtoDecoder *enc)
{
    // free debug object
    DebugObject_Free(&enc->d_obj);
    
    // free start job
    BPending_Free(&enc->start_job);
    
    // free buffer
    free(enc->buf);
    
    // free dead var
    DEAD_KILL(enc->dead);
}

void PacketProtoDecoder_Reset (PacketProtoDecoder *enc)
{
    enc->buf_start += enc->buf_used;
    enc->buf_used = 0;
}
