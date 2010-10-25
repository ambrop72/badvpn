/**
 * @file StreamSocketSource.c
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

#include <stdlib.h>

#include <misc/debug.h>

#include <flow/StreamSocketSource.h>

static void report_error (StreamSocketSource *s, int error)
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

static int output_handler_recv (StreamSocketSource *s, uint8_t *data, int data_avail)
{
    ASSERT(s->out_avail == -1)
    ASSERT(data_avail > 0)
    ASSERT(!s->in_error)
    
    int res = BSocket_Recv(s->bsock, data, data_avail);
    if (res < 0) {
        int error = BSocket_GetError(s->bsock);
        if (error == BSOCKET_ERROR_LATER) {
            s->out_avail = data_avail;
            s->out = data;
            BSocket_EnableEvent(s->bsock, BSOCKET_READ);
            return 0;
        }
        report_error(s, STREAMSOCKETSOURCE_ERROR_BSOCKET);
        return -1;
    }
    
    if (res == 0) {
        report_error(s, STREAMSOCKETSOURCE_ERROR_CLOSED);
        return -1;
    }
    
    return res;
}

static void socket_handler (StreamSocketSource *s, int event)
{
    ASSERT(s->out_avail > 0)
    ASSERT(event == BSOCKET_READ)
    ASSERT(!s->in_error)
    
    int res = BSocket_Recv(s->bsock, s->out, s->out_avail);
    if (res < 0) {
        int error = BSocket_GetError(s->bsock);
        if (error == BSOCKET_ERROR_LATER) {
            return;
        }
        report_error(s, STREAMSOCKETSOURCE_ERROR_BSOCKET);
        return;
    }
    
    if (res == 0) {
        report_error(s, STREAMSOCKETSOURCE_ERROR_CLOSED);
        return;
    }
    
    BSocket_DisableEvent(s->bsock, BSOCKET_READ);
    s->out_avail = -1;
    
    StreamRecvInterface_Done(&s->output, res);
    return;
}

void StreamSocketSource_Init (StreamSocketSource *s, FlowErrorReporter rep, BSocket *bsock)
{
    // init arguments
    s->rep = rep;
    s->bsock = bsock;
    
    // init dead var
    DEAD_INIT(s->dead);
    
    // add socket event handler
    BSocket_AddEventHandler(s->bsock, BSOCKET_READ, (BSocket_handler)socket_handler, s);
    
    // init output
    StreamRecvInterface_Init(&s->output, (StreamRecvInterface_handler_recv)output_handler_recv, s);
    
    // have no output packet
    s->out_avail = -1;
    
    // init debugging
    #ifndef NDEBUG
    s->in_error = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&s->d_obj);
}

void StreamSocketSource_Free (StreamSocketSource *s)
{
    // free debug object
    DebugObject_Free(&s->d_obj);
    
    // free output
    StreamRecvInterface_Free(&s->output);
    
    // remove socket event handler
    BSocket_RemoveEventHandler(s->bsock, BSOCKET_READ);
    
    // free dead var
    DEAD_KILL(s->dead);
}

StreamRecvInterface * StreamSocketSource_GetOutput (StreamSocketSource *s)
{
    return &s->output;
}
