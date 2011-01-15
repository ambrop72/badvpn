/**
 * @file PacketPassNotifier.c
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

#include <flow/PacketPassNotifier.h>

void input_handler_send (PacketPassNotifier *o, uint8_t *data, int data_len)
{
    DebugObject_Access(&o->d_obj);
    
    // schedule send
    PacketPassInterface_Sender_Send(o->output, data, data_len);
    
    // if we have a handler, call it
    if (o->handler) {
        o->handler(o->handler_user, data, data_len);
        return;
    }
}

void input_handler_cancel (PacketPassNotifier *o)
{
    DebugObject_Access(&o->d_obj);
    
    PacketPassInterface_Sender_Cancel(o->output);
}

void output_handler_done (PacketPassNotifier *o)
{
    DebugObject_Access(&o->d_obj);
    
    PacketPassInterface_Done(&o->input);
}

void PacketPassNotifier_Init (PacketPassNotifier *o, PacketPassInterface *output, BPendingGroup *pg)
{
    // init arguments
    o->output = output;
    
    // init input
    PacketPassInterface_Init(&o->input, PacketPassInterface_GetMTU(o->output), (PacketPassInterface_handler_send)input_handler_send, o, pg);
    if (PacketPassInterface_HasCancel(o->output)) {
        PacketPassInterface_EnableCancel(&o->input, (PacketPassInterface_handler_cancel)input_handler_cancel);
    }
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    // set no handler
    o->handler = NULL;
    
    DebugObject_Init(&o->d_obj);
}

void PacketPassNotifier_Free (PacketPassNotifier *o)
{
    DebugObject_Free(&o->d_obj);

    // free input
    PacketPassInterface_Free(&o->input);
}

PacketPassInterface * PacketPassNotifier_GetInput (PacketPassNotifier *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->input;
}

void PacketPassNotifier_SetHandler (PacketPassNotifier *o, PacketPassNotifier_handler_notify handler, void *user)
{
    DebugObject_Access(&o->d_obj);
    
    o->handler = handler;
    o->handler_user = user;
}
