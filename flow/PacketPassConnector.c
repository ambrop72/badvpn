/**
 * @file PacketPassConnector.c
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

#include <flow/PacketPassConnector.h>

static int input_handler_send (PacketPassConnector *o, uint8_t *data, int data_len)
{
    ASSERT(o->in_len == -1)
    ASSERT(!(o->output) || !o->out_blocking)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= o->input_mtu)
    
    // if we have no output, remember input packet
    if (!o->output) {
        o->in_len = data_len;
        o->in = data;
        return 0;
    }
    
    // try to send the packet
    int res;
    while (1) {
        DEAD_ENTER_N(obj, o->dead)
        DEAD_ENTER_N(out, o->output_dead)
        res = PacketPassInterface_Sender_Send(o->output, data, data_len);
        DEAD_LEAVE_N(obj, o->dead);
        DEAD_LEAVE_N(out, o->output_dead);
        if (DEAD_KILLED_N(obj)) {
            return -1;
        }
        if (DEAD_KILLED_N(out)) {
            if (!o->output) {
                // lost output
                o->in_len = data_len;
                o->in = data;
                return 0;
            }
            // got a new output, retry
            continue;
        }
        break;
    }
    
    ASSERT(res == 0 || res == 1)
    
    if (!res) {
        // output blocking
        o->in_len = data_len;
        o->in = data;
        o->out_blocking = 1;
        return 0;
    }
    
    return 1;
}

static void output_handler_done (PacketPassConnector *o)
{
    ASSERT(o->in_len >= 0)
    ASSERT(o->output)
    ASSERT(o->out_blocking)
    
    // have no input packet
    o->in_len = -1;
    
    // output not blocking any more
    o->out_blocking = 0;
    
    // allow input to send more packets
    PacketPassInterface_Done(&o->input);
    return;
}

static void job_handler (PacketPassConnector *o)
{
    ASSERT(o->output)
    ASSERT(!o->out_blocking)
    ASSERT(o->in_len >= 0)
    
    // try to send the packet
    DEAD_ENTER_N(obj, o->dead)
    DEAD_ENTER_N(out, o->output_dead)
    int res = PacketPassInterface_Sender_Send(o->output, o->in, o->in_len);
    DEAD_LEAVE_N(obj, o->dead);
    DEAD_LEAVE_N(out, o->output_dead);
    if (DEAD_KILLED_N(obj)) {
        return;
    }
    if (DEAD_KILLED_N(out)) {
        // lost current output. Do nothing here.
        // If we gained a new one, its own job is responsible for it.
        return;
    }
    
    ASSERT(res == 0 || res == 1)
    
    if (!res) {
        // output blocking
        o->out_blocking = 1;
        return;
    }
    
    // have no input packet
    o->in_len = -1;
    
    // allow input to send more packets
    PacketPassInterface_Done(&o->input);
    return;
}

void PacketPassConnector_Init (PacketPassConnector *o, int mtu, BPendingGroup *pg)
{
    ASSERT(mtu >= 0)
    
    // init arguments
    o->input_mtu = mtu;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init input
    PacketPassInterface_Init(&o->input, o->input_mtu, (PacketPassInterface_handler_send)input_handler_send, o);
    
    // have no input packet
    o->in_len = -1;
    
    // have no output
    o->output = NULL;
    
    // init continue job
    BPending_Init(&o->continue_job, pg, (BPending_handler)job_handler, o);
    
    // init debug object
    DebugObject_Init(&o->d_obj);
}

void PacketPassConnector_Free (PacketPassConnector *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);
    
    // free continue job
    BPending_Free(&o->continue_job);
    
    // free output dead var
    if (o->output) {
        DEAD_KILL(o->output_dead);
    }
    
    // free input
    PacketPassInterface_Free(&o->input);
    
    // free dead var
    DEAD_KILL(o->dead);
}

PacketPassInterface * PacketPassConnector_GetInput (PacketPassConnector *o)
{
    return &o->input;
}

void PacketPassConnector_ConnectOutput (PacketPassConnector *o, PacketPassInterface *output)
{
    ASSERT(!o->output)
    ASSERT(PacketPassInterface_GetMTU(output) >= o->input_mtu)
    
    // set output
    o->output = output;
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    // init output dead var
    DEAD_INIT(o->output_dead);
    
    // set output not blocking
    o->out_blocking = 0;
    
    // if we have an input packet, set continue job
    if (o->in_len >= 0) {
        BPending_Set(&o->continue_job);
    }
}

void PacketPassConnector_DisconnectOutput (PacketPassConnector *o)
{
    ASSERT(o->output)
    
    // unset continue job (in case it wasn't called yet)
    BPending_Unset(&o->continue_job);
    
    // free dead var
    DEAD_KILL(o->output_dead);
    
    // set no output
    o->output = NULL;
}
