/**
 * @file SeqPacketSocketSink.c
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

#include <misc/debug.h>

#include <flow/SeqPacketSocketSink.h>

static void report_error (SeqPacketSocketSink *s, int error)
{
    #ifndef NDEBUG
    s->in_error = 1;
    DEAD_ENTER(s->dead)
    #endif
    
    FlowErrorReporter_ReportError(&s->rep, &error);
    
    #ifndef NDEBUG
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(s->dead);
    #endif
}

static int input_handler_send (SeqPacketSocketSink *s, uint8_t *data, int data_len)
{
    ASSERT(s->in_len == -1)
    ASSERT(data_len >= 0)
    ASSERT(!s->in_error)
    DebugObject_Access(&s->d_obj);
    
    int res = BSocket_Send(s->bsock, data, data_len);
    if (res < 0) {
        int error = BSocket_GetError(s->bsock);
        if (error == BSOCKET_ERROR_LATER) {
            s->in_len = data_len;
            s->in = data;
            BSocket_EnableEvent(s->bsock, BSOCKET_WRITE);
            return 0;
        }
        report_error(s, SEQPACKETSOCKETSINK_ERROR_BSOCKET);
        return -1;
    } else {
        if (res != data_len) {
            report_error(s, SEQPACKETSOCKETSINK_ERROR_WRONGSIZE);
            return -1;
        }
    }
    
    return 1;
}

static void socket_handler (SeqPacketSocketSink *s, int event)
{
    ASSERT(s->in_len >= 0)
    ASSERT(event == BSOCKET_WRITE)
    ASSERT(!s->in_error)
    DebugObject_Access(&s->d_obj);
    
    int res = BSocket_Send(s->bsock, s->in, s->in_len);
    if (res < 0) {
        int error = BSocket_GetError(s->bsock);
        if (error == BSOCKET_ERROR_LATER) {
            return;
        }
        report_error(s, SEQPACKETSOCKETSINK_ERROR_BSOCKET);
        return;
    } else {
        if (res != s->in_len) {
            report_error(s, SEQPACKETSOCKETSINK_ERROR_WRONGSIZE);
            return;
        }
    }
    
    BSocket_DisableEvent(s->bsock, BSOCKET_WRITE);
    s->in_len = -1;
    
    PacketPassInterface_Done(&s->input);
    return;
}

void SeqPacketSocketSink_Init (SeqPacketSocketSink *s, FlowErrorReporter rep, BSocket *bsock, int mtu)
{
    ASSERT(mtu >= 0)
    
    // init arguments
    s->rep = rep;
    s->bsock = bsock;
    
    // init dead var
    DEAD_INIT(s->dead);
    
    // add socket event handler
    BSocket_AddEventHandler(s->bsock, BSOCKET_WRITE, (BSocket_handler)socket_handler, s);
    
    // init input
    PacketPassInterface_Init(&s->input, mtu, (PacketPassInterface_handler_send)input_handler_send, s);
    
    // have no input packet
    s->in_len = -1;
    
    // init debugging
    #ifndef NDEBUG
    s->in_error = 0;
    #endif
    
    DebugObject_Init(&s->d_obj);
}

void SeqPacketSocketSink_Free (SeqPacketSocketSink *s)
{
    DebugObject_Free(&s->d_obj);

    // free input
    PacketPassInterface_Free(&s->input);
    
    // remove socket event handler
    BSocket_RemoveEventHandler(s->bsock, BSOCKET_WRITE);
    
    // free dead var
    DEAD_KILL(s->dead);
}

PacketPassInterface * SeqPacketSocketSink_GetInput (SeqPacketSocketSink *s)
{
    DebugObject_Access(&s->d_obj);
    
    return &s->input;
}
