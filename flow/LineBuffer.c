/**
 * @file LineBuffer.c
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

#include <flow/LineBuffer.h>

static void input_handler_done (LineBuffer *o, int data_len)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(data_len > 0)
    ASSERT(data_len <= o->buf_size - o->buf_used)
    
    // update buffer
    o->buf_used += data_len;
    
    // look for newline
    int i;
    for (i = o->buf_used - data_len; i < o->buf_used; i++) {
        if (o->buf[i] == o->nl_char) {
            break;
        }
    }
    
    if (i < o->buf_used || o->buf_used == o->buf_size) {
        // pass to output
        o->buf_consumed = (i < o->buf_used ? i + 1 : i);
        PacketPassInterface_Sender_Send(o->output, o->buf, o->buf_consumed);
    } else {
        // receive more data
        StreamRecvInterface_Receiver_Recv(o->input, o->buf + o->buf_used, o->buf_size - o->buf_used);
    }
}

static void output_handler_done (LineBuffer *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->buf_consumed > 0)
    ASSERT(o->buf_consumed <= o->buf_used)
    
    // update buffer
    memmove(o->buf, o->buf + o->buf_consumed, o->buf_used - o->buf_consumed);
    o->buf_used -= o->buf_consumed;
    
    // look for newline
    int i;
    for (i = 0; i < o->buf_used; i++) {
        if (o->buf[i] == o->nl_char) {
            break;
        }
    }
    
    if (i < o->buf_used || o->buf_used == o->buf_size) {
        // pass to output
        o->buf_consumed = (i < o->buf_used ? i + 1 : i);
        PacketPassInterface_Sender_Send(o->output, o->buf, o->buf_consumed);
    } else {
        // receive more data
        StreamRecvInterface_Receiver_Recv(o->input, o->buf + o->buf_used, o->buf_size - o->buf_used);
    }
}

int LineBuffer_Init (LineBuffer *o, StreamRecvInterface *input, PacketPassInterface *output, int buf_size, uint8_t nl_char)
{
    ASSERT(buf_size > 0)
    ASSERT(PacketPassInterface_GetMTU(output) >= buf_size)
    
    // init arguments
    o->input = input;
    o->output = output;
    o->buf_size = buf_size;
    o->nl_char = nl_char;
    
    // init input
    StreamRecvInterface_Receiver_Init(o->input, (StreamRecvInterface_handler_done)input_handler_done, o);
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    // set buffer empty
    o->buf_used = 0;
    
    // allocate buffer
    if (!(o->buf = malloc(o->buf_size))) {
        DEBUG("malloc failed");
        goto fail0;
    }
    
    // start receiving
    StreamRecvInterface_Receiver_Recv(o->input, o->buf, o->buf_size);
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail0:
    return 0;
}

void LineBuffer_Free (LineBuffer *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free buffer
    free(o->buf);
}
