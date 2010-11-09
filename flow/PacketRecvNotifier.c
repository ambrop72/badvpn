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

static void output_handler_recv (PacketRecvNotifier *o, uint8_t *data);
static void input_handler_done (PacketRecvNotifier *o, int data_len);

void output_handler_recv (PacketRecvNotifier *o, uint8_t *data)
{
    DebugObject_Access(&o->d_obj);
    
    // schedule receive
    o->out = data;
    PacketRecvInterface_Receiver_Recv(o->input, o->out);
}

void input_handler_done (PacketRecvNotifier *o, int data_len)
{
    DebugObject_Access(&o->d_obj);
    
    // finish packet
    PacketRecvInterface_Done(&o->output, data_len);
    
    // if we have a handler, call it
    if (o->handler) {
        o->handler(o->handler_user, o->out, data_len);
        return;
    }
}

void PacketRecvNotifier_Init (PacketRecvNotifier *o, PacketRecvInterface *input, BPendingGroup *pg)
{
    // set arguments
    o->input = input;
    
    // init output
    PacketRecvInterface_Init(&o->output, PacketRecvInterface_GetMTU(o->input), (PacketRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    // set no handler
    o->handler = NULL;
    
    DebugObject_Init(&o->d_obj);
}

void PacketRecvNotifier_Free (PacketRecvNotifier *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free output
    PacketRecvInterface_Free(&o->output);
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
