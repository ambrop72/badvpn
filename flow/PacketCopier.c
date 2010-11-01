/**
 * @file PacketCopier.c
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

#include <misc/debug.h>

#include <flow/PacketCopier.h>

static int input_handler_send (PacketCopier *o, uint8_t *data, int data_len)
{
    ASSERT(o->in_len == -1)
    ASSERT(data_len >= 0)
    
    if (!o->out_have || o->out_got_len >= 0) {
        o->in_len = data_len;
        o->in = data;
        o->in_got = 0;
        return 0;
    }
    
    memcpy(o->out, data, data_len);
    
    o->out_got_len = data_len;
    
    BPending_Set(&o->continue_job_output);
    
    return 1;
}

static void input_handler_cancel (PacketCopier *o)
{
    ASSERT(o->in_len >= 0)
    ASSERT(!o->out_have)
    
    o->in_len = -1;
}

static int output_handler_recv (PacketCopier *o, uint8_t *data, int *data_len)
{
    ASSERT(!o->out_have)
    
    if (o->in_len < 0 || o->in_got) {
        o->out_have = 1;
        o->out = data;
        o->out_got_len = -1;
        return 0;
    }
    
    memcpy(data, o->in, o->in_len);
    
    o->in_got =  1;
    
    BPending_Set(&o->continue_job_input);
    
    *data_len = o->in_len;
    return 1;
}

static void input_job_handler (PacketCopier *o)
{
    ASSERT(o->in_len >= 0)
    ASSERT(o->in_got)
    
    o->in_len = -1;
    
    PacketPassInterface_Done(&o->input);
    return;
}

static void output_job_handler (PacketCopier *o)
{
    ASSERT(o->out_have)
    ASSERT(o->out_got_len >= 0)
    
    o->out_have = 0;
    
    PacketRecvInterface_Done(&o->output, o->out_got_len);
    return;
}

void PacketCopier_Init (PacketCopier *o, int mtu, BPendingGroup *pg)
{
    ASSERT(mtu >= 0)
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init input
    PacketPassInterface_Init(&o->input, mtu, (PacketPassInterface_handler_send)input_handler_send, o);
    PacketPassInterface_EnableCancel(&o->input, (PacketPassInterface_handler_cancel)input_handler_cancel);
    
    // init output
    PacketRecvInterface_Init(&o->output, mtu, (PacketRecvInterface_handler_recv)output_handler_recv, o);
    
    // set no input packet
    o->in_len = -1;
    
    // set no output packet
    o->out_have = 0;
    
    // init continue jobs
    BPending_Init(&o->continue_job_input, pg, (BPending_handler)input_job_handler, o);
    BPending_Init(&o->continue_job_output, pg, (BPending_handler)output_job_handler, o);
    
    // init debug object
    DebugObject_Init(&o->d_obj);
}

void PacketCopier_Free (PacketCopier *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);
    
    // free continue jobs
    BPending_Free(&o->continue_job_output);
    BPending_Free(&o->continue_job_input);
    
    // free output
    PacketRecvInterface_Free(&o->output);
    
    // free input
    PacketPassInterface_Free(&o->input);
    
    // free dead var
    DEAD_KILL(o->dead);
}

PacketPassInterface * PacketCopier_GetInput (PacketCopier *o)
{
    return &o->input;
}

PacketRecvInterface * PacketCopier_GetOutput (PacketCopier *o)
{
    return &o->output;
}
