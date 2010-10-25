/**
 * @file PacketRecvConnector.c
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

#include <stddef.h>

#include <misc/debug.h>

#include <flow/PacketRecvConnector.h>

static int output_handler_recv (PacketRecvConnector *o, uint8_t *data, int *data_len)
{
    ASSERT(!o->out_have)
    ASSERT(!o->input || !o->in_blocking)
    
    // if we have no input, remember output packet
    if (!o->input) {
        o->out_have = 1;
        o->out = data;
        return 0;
    }
    
    // try to receive the packet
    int res;
    while (1) {
        DEAD_ENTER_N(obj, o->dead)
        DEAD_ENTER_N(inp, o->input_dead)
        res = PacketRecvInterface_Receiver_Recv(o->input, data, data_len);
        DEAD_LEAVE_N(obj, o->dead);
        DEAD_LEAVE_N(inp, o->input_dead);
        if (DEAD_KILLED_N(obj)) {
            return -1;
        }
        if (DEAD_KILLED_N(inp)) {
            if (!o->input) {
                // lost input
                o->out_have = 1;
                o->out = data;
                return 0;
            }
            // got a new input, retry
            continue;
        }
        break;
    };
    
    ASSERT(res == 0 || res == 1)
    if (res) {
        ASSERT(*data_len >= 0)
        ASSERT(*data_len <= o->output_mtu)
    }
    
    if (!res) {
        // input blocking
        o->out_have = 1;
        o->out = data;
        o->in_blocking = 1;
        return 0;
    }
    
    return 1;
}

static void input_handler_done (PacketRecvConnector *o, int data_len)
{
    ASSERT(o->out_have)
    ASSERT(o->input)
    ASSERT(o->in_blocking)
    
    // have no output packet
    o->out_have = 0;
    
    // input not blocking any more
    o->in_blocking = 0;
    
    // allow output to receive more packets
    PacketRecvInterface_Done(&o->output, data_len);
    return;
}

static void job_handler (PacketRecvConnector *o)
{
    ASSERT(o->input)
    ASSERT(!o->in_blocking)
    ASSERT(o->out_have)
    
    // try to receive the packet
    int in_len;
    DEAD_ENTER_N(obj, o->dead)
    DEAD_ENTER_N(inp, o->input_dead)
    int res = PacketRecvInterface_Receiver_Recv(o->input, o->out, &in_len);
    DEAD_LEAVE_N(obj, o->dead);
    DEAD_LEAVE_N(inp, o->input_dead);
    if (DEAD_KILLED_N(obj)) {
        return;
    }
    if (DEAD_KILLED_N(inp)) {
        // lost current input. Do nothing here.
        // If we gained a new one, its own job is responsible for it.
        return;
    }
    
    ASSERT(res == 0 || res == 1)
    if (res) {
        ASSERT(in_len >= 0)
        ASSERT(in_len <= o->output_mtu)
    }
    
    if (!res) {
        // input blocking
        o->in_blocking = 1;
        return;
    }
    
    // have no output packet
    o->out_have = 0;
    
    // allow output to receive more packets
    PacketRecvInterface_Done(&o->output, in_len);
    return;
}

void PacketRecvConnector_Init (PacketRecvConnector *o, int mtu, BPendingGroup *pg)
{
    ASSERT(mtu >= 0)
    
    // init arguments
    o->output_mtu = mtu;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init output
    PacketRecvInterface_Init(&o->output, o->output_mtu, (PacketRecvInterface_handler_recv)output_handler_recv, o);
    
    // have no output packet
    o->out_have = 0;
    
    // have no input
    o->input = NULL;
    
    // init continue job
    BPending_Init(&o->continue_job, pg, (BPending_handler)job_handler, o);
    
    // init debug object
    DebugObject_Init(&o->d_obj);
}

void PacketRecvConnector_Free (PacketRecvConnector *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);
    
    // free continue job
    BPending_Free(&o->continue_job);
    
    // free input dead var
    if (o->input) {
        DEAD_KILL(o->input_dead);
    }
    
    // free output
    PacketRecvInterface_Free(&o->output);
    
    // free dead var
    DEAD_KILL(o->dead);
}

PacketRecvInterface * PacketRecvConnector_GetOutput (PacketRecvConnector *o)
{
    return &o->output;
}

void PacketRecvConnector_ConnectInput (PacketRecvConnector *o, PacketRecvInterface *input)
{
    ASSERT(!o->input)
    ASSERT(PacketRecvInterface_GetMTU(input) <= o->output_mtu)
    
    // set input
    o->input = input;
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    // init input dead var
    DEAD_INIT(o->input_dead);
    
    // set input not blocking
    o->in_blocking = 0;
    
    // if we have an input packet, set continue job
    if (o->out_have) {
        BPending_Set(&o->continue_job);
    }
}

void PacketRecvConnector_DisconnectInput (PacketRecvConnector *o)
{
    ASSERT(o->input)
    
    // unset continue job (in case it wasn't called yet)
    BPending_Unset(&o->continue_job);
    
    // free dead var
    DEAD_KILL(o->input_dead);
    
    // set no input
    o->input = NULL;
}
