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
static int output_handler_recv (PacketRecvNotifier *o, uint8_t *data, int *data_len);
static void input_handler_done (PacketRecvNotifier *o, int data_len);

int call_handler (PacketRecvNotifier *o, uint8_t *data, int data_len)
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

int output_handler_recv (PacketRecvNotifier *o, uint8_t *data, int *data_len)
{
    ASSERT(!o->out_have)
    ASSERT(!o->in_handler)
    
    DEAD_DECLARE
    
    // call recv on input
    DEAD_ENTER2(o->dead)
    int res = PacketRecvInterface_Receiver_Recv(o->input, data, data_len);
    if (DEAD_LEAVE(o->dead)) {
        return -1;
    }
    
    ASSERT(res == 0 || res == 1)
    
    if (!res) {
        // input blocking, continue in input_handler_done
        #ifndef NDEBUG
        o->out_have = 1;
        #endif
        o->out = data;
        return 0;
    }
    
    // if we have a handler, call it
    if (o->handler) {
        if (call_handler(o, data, *data_len) < 0) {
            return -1;
        }
    }
    
    return 1;
}

void input_handler_done (PacketRecvNotifier *o, int data_len)
{
    ASSERT(o->out_have)
    ASSERT(!o->in_handler)
    
    #ifndef NDEBUG
    o->out_have = 0;
    #endif
    
    // if we have a handler, call it
    if (o->handler) {
        if (call_handler(o, o->out, data_len) < 0) {
            return;
        }
    }
    
    PacketRecvInterface_Done(&o->output, data_len);
    return;
}

void PacketRecvNotifier_Init (PacketRecvNotifier *o, PacketRecvInterface *input)
{
    // set arguments
    o->input = input;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init output
    PacketRecvInterface_Init(&o->output, PacketRecvInterface_GetMTU(o->input), (PacketRecvInterface_handler_recv)output_handler_recv, o);
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    // set no handler
    o->handler = NULL;
    
    // init debugging
    #ifndef NDEBUG
    o->out_have = 0;
    o->in_handler = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&o->d_obj);
}

void PacketRecvNotifier_Free (PacketRecvNotifier *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);

    // free output
    PacketRecvInterface_Free(&o->output);
    
    // free dead var
    DEAD_KILL(o->dead);
}

PacketRecvInterface * PacketRecvNotifier_GetOutput (PacketRecvNotifier *o)
{
    return &o->output;
}

void PacketRecvNotifier_SetHandler (PacketRecvNotifier *o, PacketRecvNotifier_handler_notify handler, void *user)
{
    o->handler = handler;
    o->handler_user = user;
}
