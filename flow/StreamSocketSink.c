/**
 * @file StreamSocketSink.c
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

#include <flow/StreamSocketSink.h>

static void report_error (StreamSocketSink *s, int error)
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

static int input_handler_send (StreamSocketSink *s, uint8_t *data, int data_len)
{
    ASSERT(s->in_len == -1)
    ASSERT(data_len > 0)
    ASSERT(!s->in_error)
    
    int res = BSocket_Send(s->bsock, data, data_len);
    if (res < 0) {
        int error = BSocket_GetError(s->bsock);
        if (error == BSOCKET_ERROR_LATER) {
            s->in_len = data_len;
            s->in = data;
            BSocket_EnableEvent(s->bsock, BSOCKET_WRITE);
            return 0;
        }
        report_error(s, STREAMSOCKETSINK_ERROR_BSOCKET);
        return -1;
    }
    
    ASSERT(res > 0)
    
    return res;
}

static void socket_handler (StreamSocketSink *s, int event)
{
    ASSERT(s->in_len > 0)
    ASSERT(event == BSOCKET_WRITE)
    ASSERT(!s->in_error)
    
    int res = BSocket_Send(s->bsock, s->in, s->in_len);
    if (res < 0) {
        int error = BSocket_GetError(s->bsock);
        if (error == BSOCKET_ERROR_LATER) {
            return;
        }
        report_error(s, STREAMSOCKETSINK_ERROR_BSOCKET);
        return;
    }
    
    ASSERT(res > 0)
    
    BSocket_DisableEvent(s->bsock, BSOCKET_WRITE);
    s->in_len = -1;
    
    StreamPassInterface_Done(&s->input, res);
    return;
}

void StreamSocketSink_Init (StreamSocketSink *s, FlowErrorReporter rep, BSocket *bsock)
{
    // init arguments
    s->rep = rep;
    s->bsock = bsock;
    
    // init dead var
    DEAD_INIT(s->dead);
    
    // add socket event handler
    BSocket_AddEventHandler(s->bsock, BSOCKET_WRITE, (BSocket_handler)socket_handler, s);
    
    // init input
    StreamPassInterface_Init(&s->input, (StreamPassInterface_handler_send)input_handler_send, s);
    
    // have no input packet
    s->in_len = -1;
    
    // init debugging
    #ifndef NDEBUG
    s->in_error = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&s->d_obj);
}

void StreamSocketSink_Free (StreamSocketSink *s)
{
    // free debug object
    DebugObject_Free(&s->d_obj);
    
    // free input
    StreamPassInterface_Free(&s->input);
    
    // remove socket event handler
    BSocket_RemoveEventHandler(s->bsock, BSOCKET_WRITE);
    
    // free dead var
    DEAD_KILL(s->dead);
}

StreamPassInterface * StreamSocketSink_GetInput (StreamSocketSink *s)
{
    return &s->input;
}
