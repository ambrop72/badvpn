/**
 * @file TimerPacketSink.h
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

#ifndef _TIMERPACKETSINK_H
#define _TIMERPACKETSINK_H

#include <stdio.h>

#include <system/BReactor.h>
#include <flow/PacketPassInterface.h>

typedef struct {
    BReactor *reactor;
    PacketPassInterface input;
    BTimer timer;
} TimerPacketSink;

static void _TimerPacketSink_input_handler_send (TimerPacketSink *s, uint8_t *data, int data_len)
{
    printf("sink: send '");
    fwrite(data, data_len, 1, stdout);
    printf("'\n");
    
    BReactor_SetTimer(s->reactor, &s->timer);
}

static void _TimerPacketSink_input_handler_cancel (TimerPacketSink *s)
{
    printf("sink: cancelled\n");
    
    BReactor_RemoveTimer(s->reactor, &s->timer);
}

static void _TimerPacketSink_timer_handler (TimerPacketSink *s)
{
    printf("sink: done\n");
    
    PacketPassInterface_Done(&s->input);
}

static void TimerPacketSink_Init (TimerPacketSink *s, BReactor *reactor, int mtu, int ms)
{
    // init arguments
    s->reactor = reactor;
    
    // init input
    PacketPassInterface_Init(&s->input, mtu, (PacketPassInterface_handler_send)_TimerPacketSink_input_handler_send, s, BReactor_PendingGroup(s->reactor));
    PacketPassInterface_EnableCancel(&s->input, (PacketPassInterface_handler_cancel)_TimerPacketSink_input_handler_cancel);
    
    // init timer
    BTimer_Init(&s->timer, ms, (BTimer_handler)_TimerPacketSink_timer_handler, s);
}

static void TimerPacketSink_Free (TimerPacketSink *s)
{
    // free timer
    BReactor_RemoveTimer(s->reactor, &s->timer);
    
    // free input
    PacketPassInterface_Free(&s->input);
}

static PacketPassInterface * TimerPacketSink_GetInput (TimerPacketSink *s)
{
    return &s->input;
}

#endif
