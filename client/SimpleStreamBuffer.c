/**
 * @file SimpleStreamBuffer.c
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
#include <stddef.h>

#include <misc/balloc.h>
#include <misc/minmax.h>

#include "SimpleStreamBuffer.h"

static void try_output (SimpleStreamBuffer *o)
{
    ASSERT(o->output_data_len > 0)
    
    // calculate number of bytes to output
    int bytes = bmin_int(o->output_data_len, o->buf_used);
    if (bytes == 0) {
        return;
    }
    
    // copy bytes to output
    memcpy(o->output_data, o->buf, bytes);
    
    // shift buffer
    memmove(o->buf, o->buf + bytes, o->buf_used - bytes);
    o->buf_used -= bytes;
    
    // forget data
    o->output_data_len = -1;
    
    // done
    StreamRecvInterface_Done(&o->output, bytes);
}

static void output_handler_recv (SimpleStreamBuffer *o, uint8_t *data, int data_len)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->output_data_len == -1)
    ASSERT(data)
    ASSERT(data_len > 0)
    
    // remember data
    o->output_data = data;
    o->output_data_len = data_len;
    
    try_output(o);
}

int SimpleStreamBuffer_Init (SimpleStreamBuffer *o, int buf_size, BPendingGroup *pg)
{
    ASSERT(buf_size > 0)
    
    // init arguments
    o->buf_size = buf_size;
    
    // init output
    StreamRecvInterface_Init(&o->output, (StreamRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    // allocate buffer
    if (!(o->buf = BAlloc(buf_size))) {
        goto fail1;
    }
    
    // init buffer state
    o->buf_used = 0;
    
    // set no output data
    o->output_data_len = -1;
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail1:
    StreamRecvInterface_Free(&o->output);
    return 0;
}

void SimpleStreamBuffer_Free (SimpleStreamBuffer *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free buffer
    BFree(o->buf);
    
    // free output
    StreamRecvInterface_Free(&o->output);
}

StreamRecvInterface * SimpleStreamBuffer_GetOutput (SimpleStreamBuffer *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}

int SimpleStreamBuffer_Write (SimpleStreamBuffer *o, uint8_t *data, int data_len)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(data_len >= 0)
    
    if (data_len > o->buf_size - o->buf_used) {
        return 0;
    }
    
    // copy to buffer
    memcpy(o->buf + o->buf_used, data, data_len);
    
    // update buffer state
    o->buf_used += data_len;
    
    // continue outputting
    if (o->output_data_len > 0) {
        try_output(o);
    }
    
    return 1;
}
