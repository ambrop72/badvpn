/**
 * @file SingleStreamSender.c
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

#include <misc/debug.h>

#include "SingleStreamSender.h"

static void output_handler_done (SingleStreamSender *o, int data_len)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(data_len > 0)
    ASSERT(data_len <= o->packet_len - o->pos)
    
    // update position
    o->pos += data_len;
    
    // if everything was sent, notify user
    if (o->pos == o->packet_len) {
        DEBUGERROR(&o->d_err, o->handler(o->user));
        return;
    }
    
    // send more
    StreamPassInterface_Sender_Send(o->output, o->packet + o->pos, o->packet_len - o->pos);
}

void SingleStreamSender_Init (SingleStreamSender *o, uint8_t *packet, int packet_len, StreamPassInterface *output, BPendingGroup *pg, void *user, SingleStreamSender_handler handler)
{
    ASSERT(packet_len > 0)
    ASSERT(handler)
    
    // init arguments
    o->packet = packet;
    o->packet_len = packet_len;
    o->output = output;
    o->user = user;
    o->handler = handler;
    
    // set position zero
    o->pos = 0;
    
    // init output
    StreamPassInterface_Sender_Init(o->output, (StreamPassInterface_handler_done)output_handler_done, o);
    
    // start sending
    StreamPassInterface_Sender_Send(o->output, o->packet + o->pos, o->packet_len - o->pos);
    
    DebugError_Init(&o->d_err, pg);
    DebugObject_Init(&o->d_obj);
}

void SingleStreamSender_Free (SingleStreamSender *o)
{
    DebugObject_Free(&o->d_obj);
    DebugError_Free(&o->d_err);
}
