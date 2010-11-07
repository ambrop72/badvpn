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

static int call_handler (PacketPassNotifier *o, uint8_t *data, int data_len);
static void input_handler_send (PacketPassNotifier *o, uint8_t *data, int data_len);
static void input_handler_cancel (PacketPassNotifier *o);
static void output_handler_done (PacketPassNotifier *o);

int call_handler (PacketPassNotifier *o, uint8_t *data, int data_len)
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

void input_handler_send (PacketPassNotifier *o, uint8_t *data, int data_len)
{
    ASSERT(!o->d_in_have)
    DebugIn_AmOut(&o->d_in_handler);
    DebugObject_Access(&o->d_obj);
    
    // schedule send
    PacketPassInterface_Sender_Send(o->output, data, data_len);
    
    // if we have a handler, call it
    if (o->handler) {
        if (call_handler(o, data, data_len) < 0) {
            return;
        }
    }
    
    #ifndef NDEBUG
    o->d_in_have = 1;
    #endif
}

void input_handler_cancel (PacketPassNotifier *o)
{
    ASSERT(o->d_in_have)
    DebugIn_AmOut(&o->d_in_handler);
    DebugObject_Access(&o->d_obj);
    
    PacketPassInterface_Sender_Cancel(o->output);
    
    #ifndef NDEBUG
    o->d_in_have = 0;
    #endif
}

void output_handler_done (PacketPassNotifier *o)
{
    ASSERT(o->d_in_have)
    DebugIn_AmOut(&o->d_in_handler);
    DebugObject_Access(&o->d_obj);
    
    PacketPassInterface_Done(&o->input);
    
    #ifndef NDEBUG
    o->d_in_have = 0;
    #endif
}

void PacketPassNotifier_Init (PacketPassNotifier *o, PacketPassInterface *output, BPendingGroup *pg)
{
    // init arguments
    o->output = output;
    
    // init dead var
    DEAD_INIT(o->dead);
    
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
    DebugIn_Init(&o->d_in_handler);
    #ifndef NDEBUG
    o->d_in_have = 0;
    #endif
}

void PacketPassNotifier_Free (PacketPassNotifier *o)
{
    DebugObject_Free(&o->d_obj);

    // free input
    PacketPassInterface_Free(&o->input);
    
    // free dead var
    DEAD_KILL(o->dead);
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
