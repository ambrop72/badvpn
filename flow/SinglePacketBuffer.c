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

#include <flow/SinglePacketBuffer.h>

static int io_loop (SinglePacketBuffer *o)
{
    DEAD_DECLARE
    int res;
    
    while (1) {
        // receive packet
        int in_len;
        DEAD_ENTER2(o->dead)
        res = PacketRecvInterface_Receiver_Recv(o->input, o->buf, &in_len);
        if (DEAD_LEAVE(o->dead)) {
            return -1;
        }
        
        ASSERT(res == 0 || res == 1)
        
        if (!res) {
            // input blocking, continue in input_handler_done
            return 0;
        }
        
        // send packet
        DEAD_ENTER2(o->dead)
        res = PacketPassInterface_Sender_Send(o->output, o->buf, in_len);
        if (DEAD_LEAVE(o->dead)) {
            return -1;
        }
        
        ASSERT(res == 0 || res == 1)
        
        if (!res) {
            // output blocking, continue in output_handler_done
            return 0;
        }
    }
}

static void input_handler_done (SinglePacketBuffer *o, int in_len)
{
    // send packet
    DEAD_ENTER(o->dead)
    int res = PacketPassInterface_Sender_Send(o->output, o->buf, in_len);
    if (DEAD_LEAVE(o->dead)) {
        return;
    }
    
    ASSERT(res == 0 || res == 1)
    
    if (!res) {
        // output blocking, continue in output_handler_done
        return;
    }
    
    io_loop(o);
    return;
}

static void output_handler_done (SinglePacketBuffer *o)
{
    io_loop(o);
    return;
}

static void job_handler (SinglePacketBuffer *o)
{
    io_loop(o);
    return;
}

int SinglePacketBuffer_Init (SinglePacketBuffer *o, PacketRecvInterface *input, PacketPassInterface *output, BPendingGroup *pg) 
{
    ASSERT(PacketPassInterface_GetMTU(output) >= PacketRecvInterface_GetMTU(input))
    
    // init arguments
    o->input = input;
    o->output = output;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    // init buffer
    if (!(o->buf = malloc(PacketRecvInterface_GetMTU(o->input)))) {
        goto fail1;
    }
    
    // init start job
    BPending_Init(&o->start_job, pg, (BPending_handler)job_handler, o);
    BPending_Set(&o->start_job);
    
    // init debug object
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    return 0;
}

void SinglePacketBuffer_Free (SinglePacketBuffer *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);
    
    // free start job
    BPending_Free(&o->start_job);
    
    // free buffer
    free(o->buf);
    
    // free dead var
    DEAD_KILL(o->dead);
}
