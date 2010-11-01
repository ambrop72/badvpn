/**
 * @file SeqPacketSocketSource.c
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

#include <flow/SeqPacketSocketSource.h>

static void report_error (SeqPacketSocketSource *s, int error)
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

static int output_handler_recv (SeqPacketSocketSource *s, uint8_t *data, int *data_len)
{
    ASSERT(!s->out_have)
    ASSERT(!s->in_error)
    DebugObject_Access(&s->d_obj);
    
    int res = BSocket_Recv(s->bsock, data, s->mtu);
    if (res < 0) {
        int error = BSocket_GetError(s->bsock);
        if (error == BSOCKET_ERROR_LATER) {
            s->out_have = 1;
            s->out = data;
            BSocket_EnableEvent(s->bsock, BSOCKET_READ);
            return 0;
        }
        report_error(s, SEQPACKETSOCKETSOURCE_ERROR_BSOCKET);
        return -1;
    }
    
    if (res == 0) {
        report_error(s, SEQPACKETSOCKETSOURCE_ERROR_CLOSED);
        return -1;
    }
    
    *data_len = res;
    return 1;
}

static void socket_handler (SeqPacketSocketSource *s, int event)
{
    ASSERT(s->out_have)
    ASSERT(event == BSOCKET_READ)
    ASSERT(!s->in_error)
    DebugObject_Access(&s->d_obj);
    
    int res = BSocket_Recv(s->bsock, s->out, s->mtu);
    if (res < 0) {
        int error = BSocket_GetError(s->bsock);
        if (error == BSOCKET_ERROR_LATER) {
            // nothing to receive, continue in socket_handler
            return;
        }
        report_error(s, SEQPACKETSOCKETSOURCE_ERROR_BSOCKET);
        return;
    }
    
    if (res == 0) {
        report_error(s, SEQPACKETSOCKETSOURCE_ERROR_CLOSED);
        return;
    }
    
    BSocket_DisableEvent(s->bsock, BSOCKET_READ);
    s->out_have = 0;
    
    PacketRecvInterface_Done(&s->output, res);
    return;
}

void SeqPacketSocketSource_Init (SeqPacketSocketSource *s, FlowErrorReporter rep, BSocket *bsock, int mtu)
{
    ASSERT(mtu >= 0)
    
    // init arguments
    s->rep = rep;
    s->bsock = bsock;
    s->mtu = mtu;
    
    // init dead var
    DEAD_INIT(s->dead);
    
    // add socket event handler
    BSocket_AddEventHandler(s->bsock, BSOCKET_READ, (BSocket_handler)socket_handler, s);
    
    // init output
    PacketRecvInterface_Init(&s->output, mtu, (PacketRecvInterface_handler_recv)output_handler_recv, s);
    
    // have no output packet
    s->out_have = 0;
    
    // init debugging
    #ifndef NDEBUG
    s->in_error = 0;
    #endif
    
    DebugObject_Init(&s->d_obj);
}

void SeqPacketSocketSource_Free (SeqPacketSocketSource *s)
{
    DebugObject_Free(&s->d_obj);

    // free output
    PacketRecvInterface_Free(&s->output);
    
    // remove socket event handler
    BSocket_RemoveEventHandler(s->bsock, BSOCKET_READ);
    
    // free dead var
    DEAD_KILL(s->dead);
}

PacketRecvInterface * SeqPacketSocketSource_GetOutput (SeqPacketSocketSource *s)
{
    DebugObject_Access(&s->d_obj);
    
    return &s->output;
}
