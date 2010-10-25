/**
 * @file PacketStreamSender.c
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

#include <flow/PacketStreamSender.h>

static int send_data (PacketStreamSender *s)
{
    ASSERT(s->in_len >= 0)
    
    while (s->in_used < s->in_len) {
        // attempt to send something
        DEAD_ENTER(s->dead)
        int res = StreamPassInterface_Sender_Send(s->output, s->in + s->in_used, s->in_len - s->in_used);
        if (DEAD_LEAVE(s->dead)) {
            return -1;
        }
        
        ASSERT(res >= 0)
        ASSERT(res <= s->in_len - s->in_used)
        
        if (res == 0) {
            // output busy, continue in output_handler_done
            return 0;
        }
        
        // update number of bytes sent
        s->in_used += res;
    }
    
    // everything sent
    s->in_len = -1;
    
    return 0;
}

static int input_handler_send (PacketStreamSender *s, uint8_t *data, int data_len)
{
    ASSERT(s->in_len == -1)
    ASSERT(data_len >= 0)
    
    // set input packet
    s->in_len = data_len;
    s->in = data;
    s->in_used = 0;
    
    // try sending
    if (send_data(s) < 0) {
        return -1;
    }
    
    // if we couldn't send everything, block input
    if (s->in_len >= 0) {
        return 0;
    }
    
    return 1;
}

static void output_handler_done (PacketStreamSender *s, int data_len)
{
    ASSERT(s->in_len >= 0)
    ASSERT(data_len > 0)
    ASSERT(data_len <= s->in_len - s->in_used)
    
    // update number of bytes sent
    s->in_used += data_len;
    
    // continue sending
    if (send_data(s) < 0) {
        return;
    }
    
    // if we couldn't send everything, keep input blocked
    if (s->in_len >= 0) {
        return;
    }
    
    // allow more input
    PacketPassInterface_Done(&s->input);
    return;
}

void PacketStreamSender_Init (PacketStreamSender *s, StreamPassInterface *output, int mtu)
{
    ASSERT(mtu >= 0)
    
    // init arguments
    s->output = output;
    
    // init dead var
    DEAD_INIT(s->dead);
    
    // init input
    PacketPassInterface_Init(&s->input, mtu, (PacketPassInterface_handler_send)input_handler_send, s);
    
    // init output
    StreamPassInterface_Sender_Init(s->output, (StreamPassInterface_handler_done)output_handler_done, s);
    
    // have no input packet
    s->in_len = -1;
    
    // init debug object
    DebugObject_Init(&s->d_obj);
}

void PacketStreamSender_Free (PacketStreamSender *s)
{
    // free debug object
    DebugObject_Free(&s->d_obj);
    
    // free input
    PacketPassInterface_Free(&s->input);
    
    // free dead var
    DEAD_KILL(s->dead);
}

PacketPassInterface * PacketStreamSender_GetInput (PacketStreamSender *s)
{
    return &s->input;
}
