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
static int input_handler_send (PacketPassNotifier *o, uint8_t *data, int data_len);
static void input_handler_cancel (PacketPassNotifier *o);
static void output_handler_done (PacketPassNotifier *o);

int call_handler (PacketPassNotifier *o, uint8_t *data, int data_len)
{
    ASSERT(o->handler)
    ASSERT(!o->in_handler)
    
    #ifndef NDEBUG
    o->in_handler = 1;
    #endif
    
    DEAD_ENTER(o->dead)
    o->handler(o->handler_user, data, data_len);
    if (DEAD_LEAVE(o->dead)) {
        return -1;
    }
    
    #ifndef NDEBUG
    o->in_handler = 0;
    #endif
    
    return 0;
}

int input_handler_send (PacketPassNotifier *o, uint8_t *data, int data_len)
{
    ASSERT(!o->in_have)
    ASSERT(!o->in_handler)
    
    // if we have a handler, call it
    if (o->handler) {
        if (call_handler(o, data, data_len) < 0) {
            return -1;
        }
    }
    
    // call send on output
    DEAD_ENTER(o->dead)
    int res = PacketPassInterface_Sender_Send(o->output, data, data_len);
    if (DEAD_LEAVE(o->dead)) {
        return -1;
    }
    
    ASSERT(res == 0 || res == 1)
    
    if (!res) {
        // output blocking, continue in output_handler_done
        #ifndef NDEBUG
        o->in_have = 1;
        #endif
        return 0;
    }
    
    return 1;
}

void input_handler_cancel (PacketPassNotifier *o)
{
    ASSERT(o->in_have)
    ASSERT(!o->in_handler)
    
    #ifndef NDEBUG
    o->in_have = 0;
    #endif
    
    PacketPassInterface_Sender_Cancel(o->output);
    return;
}

void output_handler_done (PacketPassNotifier *o)
{
    ASSERT(o->in_have)
    ASSERT(!o->in_handler)
    
    #ifndef NDEBUG
    o->in_have = 0;
    #endif
    
    PacketPassInterface_Done(&o->input);
    return;
}

void PacketPassNotifier_Init (PacketPassNotifier *o, PacketPassInterface *output)
{
    // init arguments
    o->output = output;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init input
    PacketPassInterface_Init(&o->input, PacketPassInterface_GetMTU(o->output), (PacketPassInterface_handler_send)input_handler_send, o);
    if (PacketPassInterface_HasCancel(o->output)) {
        PacketPassInterface_EnableCancel(&o->input, (PacketPassInterface_handler_cancel)input_handler_cancel);
    }
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    // set no handler
    o->handler = NULL;
    
    // init debugging
    #ifndef NDEBUG
    o->in_have = 0;
    o->in_handler = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&o->d_obj);
}

void PacketPassNotifier_Free (PacketPassNotifier *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);

    // free input
    PacketPassInterface_Free(&o->input);
    
    // free dead var
    DEAD_KILL(o->dead);
}

PacketPassInterface * PacketPassNotifier_GetInput (PacketPassNotifier *o)
{
    return &o->input;
}

void PacketPassNotifier_SetHandler (PacketPassNotifier *o, PacketPassNotifier_handler_notify handler, void *user)
{
    o->handler = handler;
    o->handler_user = user;
}
