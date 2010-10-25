#ifndef _TESTPACKETSINK_H
#define _TESTPACKETSINK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <misc/debug.h>
#include <system/DebugObject.h>
#include <flow/PacketPassInterface.h>

typedef struct {
    DebugObject d_obj;
    PacketPassInterface input;
    int accpeting;
    int have_packet;
    const char *expect;
} TestPacketSink;

static int _TestPacketSink_input_handler_send (TestPacketSink *s, uint8_t *data, int data_len)
{
    ASSERT(!s->have_packet)
    ASSERT(s->expect)
    ASSERT(strlen(s->expect) == data_len)
    ASSERT(!memcmp(s->expect, data, data_len))
    
    s->expect = NULL;
    
    if (s->accpeting) {
        return 1;
    }
    
    s->have_packet = 1;
    
    return 0;
}

static void _TestPacketSink_input_handler_cancel (TestPacketSink *s)
{
    ASSERT(s->have_packet)
    
    s->have_packet = 0;
}

static void TestPacketSink_Init (TestPacketSink *s, int mtu)
{
    PacketPassInterface_Init(&s->input, mtu, (PacketPassInterface_handler_send)_TestPacketSink_input_handler_send, s);
    PacketPassInterface_EnableCancel(&s->input, (PacketPassInterface_handler_cancel)_TestPacketSink_input_handler_cancel);
    s->accpeting = 1;
    s->have_packet = 0;
    s->expect = NULL;
    DebugObject_Init(&s->d_obj);
}

static void TestPacketSink_Free (TestPacketSink *s)
{
    DebugObject_Free(&s->d_obj);
    PacketPassInterface_Free(&s->input);
}

static PacketPassInterface * TestPacketSink_GetInput (TestPacketSink *s)
{
    return &s->input;
}

static void TestPacketSink_SetAccepting (TestPacketSink *s, int accepting)
{
    s->accpeting = !!accepting;
}

static void TestPacketSink_Done (TestPacketSink *s)
{
    ASSERT(s->have_packet)
    
    s->have_packet = 0;
    
    PacketPassInterface_Done(&s->input);
    return;
}

static void TestPacketSink_Expect (TestPacketSink *s, const char *str)
{
    s->expect = str;
}

#endif
