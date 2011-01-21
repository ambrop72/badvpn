/**
 * @file FragmentProtoDisassembler.c
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

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <misc/debug.h>
#include <misc/byteorder.h>
#include <misc/minmax.h>

#include <flow/FragmentProtoDisassembler.h>

static void write_chunks (FragmentProtoDisassembler *o)
{
    #define IN_AVAIL (o->in_len - o->in_used)
    #define OUT_AVAIL ((o->output_mtu - o->out_used) - (int)sizeof(struct fragmentproto_chunk_header))
    
    ASSERT(o->in_len >= 0)
    ASSERT(o->out)
    ASSERT(OUT_AVAIL > 0)
    
    // write chunks to output packet
    do {
        // calculate chunk length
        int chunk_len = BMIN(IN_AVAIL, OUT_AVAIL);
        if (o->chunk_mtu > 0) {
            chunk_len = BMIN(chunk_len, o->chunk_mtu);
        }
        
        // write chunk header
        struct fragmentproto_chunk_header *header = (struct fragmentproto_chunk_header *)(o->out + o->out_used);
        header->frame_id = htol16(o->frame_id);
        header->chunk_start = htol16(o->in_used);
        header->chunk_len = htol16(chunk_len);
        header->is_last = (chunk_len == IN_AVAIL);
        
        // write chunk data
        memcpy(o->out + o->out_used + sizeof(struct fragmentproto_chunk_header), o->in + o->in_used, chunk_len);
        
        // increment pointers
        o->in_used += chunk_len;
        o->out_used += sizeof(struct fragmentproto_chunk_header) + chunk_len;
    } while (IN_AVAIL > 0 && OUT_AVAIL > 0);
    
    // have we finished the input packet?
    if (IN_AVAIL == 0) {
        // set no input packet
        o->in_len = -1;
        
        // increment frame ID
        o->frame_id++;
        
        // finish input
        PacketPassInterface_Done(&o->input);
    }
    
    // should we finish the output packet?
    if (OUT_AVAIL <= 0 || o->latency < 0) {
        // set no output packet
        o->out = NULL;
        
        // stop timer (if it's running)
        if (o->latency >= 0) {
            BReactor_RemoveTimer(o->reactor, &o->timer);
        }
        
        // finish output
        PacketRecvInterface_Done(&o->output, o->out_used);
    } else {
        // start timer if we have output and it's not running (output was empty before)
        if (!BTimer_IsRunning(&o->timer)) {
            BReactor_SetTimer(o->reactor, &o->timer);
        }
    }
}

static void input_handler_send (FragmentProtoDisassembler *o, uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(o->in_len == -1)
    
    // set input packet
    o->in_len = data_len;
    o->in = data;
    o->in_used = 0;
    
    // if there is no output, wait for it
    if (!o->out) {
        return;
    }
    
    write_chunks(o);
}

static void input_handler_cancel (FragmentProtoDisassembler *o)
{
    ASSERT(o->in_len >= 0)
    ASSERT(!o->out)
    
    // set no input packet
    o->in_len = -1;
}

static void output_handler_recv (FragmentProtoDisassembler *o, uint8_t *data)
{
    ASSERT(data)
    ASSERT(!o->out)
    
    // set output packet
    o->out = data;
    o->out_used = 0;
    
    // if there is no input, wait for it
    if (o->in_len < 0) {
        return;
    }
    
    write_chunks(o);
}

static void timer_handler (FragmentProtoDisassembler *o)
{
    ASSERT(o->latency >= 0)
    ASSERT(o->out)
    ASSERT(o->in_len == -1)
    
    // set no output packet
    o->out = NULL;
    
    // finish output
    PacketRecvInterface_Done(&o->output, o->out_used);
}

void FragmentProtoDisassembler_Init (FragmentProtoDisassembler *o, BReactor *reactor, int input_mtu, int output_mtu, int chunk_mtu, btime_t latency)
{
    ASSERT(input_mtu >= 0)
    ASSERT(input_mtu <= UINT16_MAX)
    ASSERT(output_mtu > sizeof(struct fragmentproto_chunk_header))
    ASSERT(chunk_mtu > 0 || chunk_mtu < 0)
    
    // init arguments
    o->reactor = reactor;
    o->output_mtu = output_mtu;
    o->chunk_mtu = chunk_mtu;
    o->latency = latency;
    
    // init input
    PacketPassInterface_Init(&o->input, input_mtu, (PacketPassInterface_handler_send)input_handler_send, o, BReactor_PendingGroup(reactor));
    PacketPassInterface_EnableCancel(&o->input, (PacketPassInterface_handler_cancel)input_handler_cancel);
    
    // init output
    PacketRecvInterface_Init(&o->output, o->output_mtu, (PacketRecvInterface_handler_recv)output_handler_recv, o, BReactor_PendingGroup(reactor));
    
    // init timer
    if (o->latency >= 0) {
        BTimer_Init(&o->timer, o->latency, (BTimer_handler)timer_handler, o);
    }
    
    // have no input packet
    o->in_len = -1;
    
    // have no output packet
    o->out = NULL;
    
    // start with zero frame ID
    o->frame_id = 0;
    
    DebugObject_Init(&o->d_obj);
}

void FragmentProtoDisassembler_Free (FragmentProtoDisassembler *o)
{
    DebugObject_Free(&o->d_obj);

    // free timer
    if (o->latency >= 0) {
        BReactor_RemoveTimer(o->reactor, &o->timer);
    }
    
    // free output
    PacketRecvInterface_Free(&o->output);
    
    // free input
    PacketPassInterface_Free(&o->input);
}

PacketPassInterface * FragmentProtoDisassembler_GetInput (FragmentProtoDisassembler *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->input;
}

PacketRecvInterface * FragmentProtoDisassembler_GetOutput (FragmentProtoDisassembler *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}
