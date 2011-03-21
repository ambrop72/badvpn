/**
 * @file RandomPacketSink.h
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

#ifndef _RANDOMPACKETSINK_H
#define _RANDOMPACKETSINK_H

#include <stdio.h>

#include <security/BRandom.h>
#include <system/BReactor.h>
#include <system/DebugObject.h>
#include <flow/PacketPassInterface.h>

typedef struct {
    BReactor *reactor;
    PacketPassInterface input;
    BTimer timer;
    DebugObject d_obj;
} RandomPacketSink;

static void _RandomPacketSink_input_handler_send (RandomPacketSink *s, uint8_t *data, int data_len)
{
    DebugObject_Access(&s->d_obj);
    
    printf("sink: send '");
    fwrite(data, data_len, 1, stdout);
    
    uint8_t r;
    BRandom_randomize(&r, sizeof(r));
    if (r&(uint8_t)1) {
        printf("' accepting\n");
        PacketPassInterface_Done(&s->input);
    } else {
        printf("' delaying\n");
        BReactor_SetTimer(s->reactor, &s->timer);
    }
}

static void _RandomPacketSink_input_handler_requestcancel (RandomPacketSink *s)
{
    DebugObject_Access(&s->d_obj);
    
    printf("sink: cancelled\n");
    BReactor_RemoveTimer(s->reactor, &s->timer);
    PacketPassInterface_Done(&s->input);
}

static void _RandomPacketSink_timer_handler (RandomPacketSink *s)
{
    DebugObject_Access(&s->d_obj);
    
    PacketPassInterface_Done(&s->input);
}

static void RandomPacketSink_Init (RandomPacketSink *s, BReactor *reactor, int mtu, int ms)
{
    // init arguments
    s->reactor = reactor;
    
    // init input
    PacketPassInterface_Init(&s->input, mtu, (PacketPassInterface_handler_send)_RandomPacketSink_input_handler_send, s, BReactor_PendingGroup(reactor));
    PacketPassInterface_EnableCancel(&s->input, (PacketPassInterface_handler_requestcancel)_RandomPacketSink_input_handler_requestcancel);
    
    // init timer
    BTimer_Init(&s->timer, ms, (BTimer_handler)_RandomPacketSink_timer_handler, s);
    
    DebugObject_Init(&s->d_obj);
}

static void RandomPacketSink_Free (RandomPacketSink *s)
{
    DebugObject_Free(&s->d_obj);
    
    // free timer
    BReactor_RemoveTimer(s->reactor, &s->timer);
    
    // free input
    PacketPassInterface_Free(&s->input);
}

static PacketPassInterface * RandomPacketSink_GetInput (RandomPacketSink *s)
{
    DebugObject_Access(&s->d_obj);
    
    return &s->input;
}

#endif
