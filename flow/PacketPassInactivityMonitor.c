/**
 * @file PacketPassInactivityMonitor.c
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

#include <flow/PacketPassInactivityMonitor.h>

static int input_handler_send (PacketPassInactivityMonitor *o, uint8_t *data, int data_len)
{
    // set send called
    o->send_called = 1;
    
    // call send
    DEAD_ENTER(o->dead)
    int res = PacketPassInterface_Sender_Send(o->output, data, data_len);
    if (DEAD_LEAVE(o->dead)) {
        return -1;
    }
    
    ASSERT(res == 0 || res == 1)
    
    if (res == 0) {
        // output busy, stop timer
        BReactor_RemoveTimer(o->reactor, &o->timer);
    } else {
        // output accepted packet, restart timer
        BReactor_SetTimer(o->reactor, &o->timer);
    }
    
    return res;
}

static void input_handler_cancel (PacketPassInactivityMonitor *o)
{
    // output no longer busy, restart timer
    BReactor_SetTimer(o->reactor, &o->timer);
    
    // call cancel
    PacketPassInterface_Sender_Cancel(o->output);
    return;
}

static void output_handler_done (PacketPassInactivityMonitor *o)
{
    // output no longer busy, restart timer
    BReactor_SetTimer(o->reactor, &o->timer);
    
    // call done
    PacketPassInterface_Done(&o->input);
    return;
}

static void timer_handler (PacketPassInactivityMonitor *o)
{
    // restart timer
    BReactor_SetTimer(o->reactor, &o->timer);
    
    // call handler
    o->handler(o->user);
    return;
}

void PacketPassInactivityMonitor_Init (PacketPassInactivityMonitor *o, PacketPassInterface *output, BReactor *reactor, btime_t interval, PacketPassInactivityMonitor_handler handler, void *user)
{
    ASSERT(interval > 0)
    
    // init arguments
    o->output = output;
    o->reactor = reactor;
    o->handler = handler;
    o->user = user;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // init input
    PacketPassInterface_Init(&o->input, PacketPassInterface_GetMTU(o->output), (PacketPassInterface_handler_send)input_handler_send, o);
    if (PacketPassInterface_HasCancel(o->output)) {
        PacketPassInterface_EnableCancel(&o->input, (PacketPassInterface_handler_cancel)input_handler_cancel);
    }
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    // init timer and start it
    BTimer_Init(&o->timer, interval, (BTimer_handler)timer_handler, o);
    BReactor_SetTimer(o->reactor, &o->timer);
    
    // set send not called
    o->send_called = 0;
    
    // init debug object
    DebugObject_Init(&o->d_obj);
}

void PacketPassInactivityMonitor_Free (PacketPassInactivityMonitor *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);

    // free timer
    BReactor_RemoveTimer(o->reactor, &o->timer);
    
    // free input
    PacketPassInterface_Free(&o->input);
    
    // free dead var
    DEAD_KILL(o->dead);
}

PacketPassInterface * PacketPassInactivityMonitor_GetInput (PacketPassInactivityMonitor *o)
{
    return &o->input;
}
