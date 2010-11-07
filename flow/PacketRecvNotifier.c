/**
 * @file PacketRecvNotifier.c
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

#include <flow/PacketRecvNotifier.h>

static int call_handler (PacketRecvNotifier *o, uint8_t *data, int data_len);
static void output_handler_recv (PacketRecvNotifier *o, uint8_t *data);
static void input_handler_done (PacketRecvNotifier *o, int data_len);

int call_handler (PacketRecvNotifier *o, uint8_t *data, int data_len)
{
    ASSERT(o->handler)
    DebugIn_AmOut(&o->d_in_handler);
    DebugObject_Access(&o->d_obj);
    
    DebugIn_GoIn(&o->d_in_handler);
    DEAD_ENTER(o->dead)
    o->handler(o->handler_user, data, data_len);
    if (DEAD_LEAVE(o->dead)) {
        return -1;
    }
    DebugIn_GoOut(&o->d_in_handler);
    
    return 0;
}

void output_handler_recv (PacketRecvNotifier *o, uint8_t *data)
{
    DebugIn_AmOut(&o->d_in_handler);
    DebugObject_Access(&o->d_obj);
    
    // schedule receive
    o->out = data;
    PacketRecvInterface_Receiver_Recv(o->input, o->out);
}

void input_handler_done (PacketRecvNotifier *o, int data_len)
{
    DebugIn_AmOut(&o->d_in_handler);
    DebugObject_Access(&o->d_obj);
    
    // finish packet
    PacketRecvInterface_Done(&o->output, data_len);
    
    // if we have a handler, call it
    if (o->handler) {
        if (call_handler(o, o->out, data_len) < 0) {
            return;
        }
    }
}

void PacketRecvNotifier_Init (PacketRecvNotifier *o, PacketRecvInterface *input, BPendingGroup *pg)
{
    // set arguments
    o->input = input;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init output
    PacketRecvInterface_Init(&o->output, PacketRecvInterface_GetMTU(o->input), (PacketRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    // set no handler
    o->handler = NULL;
    
    DebugObject_Init(&o->d_obj);
    DebugIn_Init(&o->d_in_handler);
}

void PacketRecvNotifier_Free (PacketRecvNotifier *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free output
    PacketRecvInterface_Free(&o->output);
    
    // free dead var
    DEAD_KILL(o->dead);
}

PacketRecvInterface * PacketRecvNotifier_GetOutput (PacketRecvNotifier *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}

void PacketRecvNotifier_SetHandler (PacketRecvNotifier *o, PacketRecvNotifier_handler_notify handler, void *user)
{
    DebugObject_Access(&o->d_obj);
    
    o->handler = handler;
    o->handler_user = user;
}
