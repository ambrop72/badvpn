/**
 * @file DatagramSocketSource.c
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

#include <flow/DatagramSocketSource.h>

static void report_error (DatagramSocketSource *s, int error)
{
    DebugIn_GoIn(&s->d_in_error);
    DEAD_ENTER(s->dead)
    FlowErrorReporter_ReportError(&s->rep, &error);
    if (DEAD_LEAVE(s->dead)) {
        return;
    }
    DebugIn_GoOut(&s->d_in_error);
}

static void try_recv (DatagramSocketSource *s)
{
    ASSERT(s->out_have)
    
    int res = BSocket_RecvFromTo(s->bsock, s->out, s->mtu, &s->last_addr, &s->last_local_addr);
    if (res < 0 && BSocket_GetError(s->bsock) == BSOCKET_ERROR_LATER) {
        // wait for socket in socket_handler
        BSocket_EnableEvent(s->bsock, BSOCKET_READ);
        return;
    }
    
    if (res < 0) {
        // schedule retry
        BPending_Set(&s->retry_job);
        
        // report error
        report_error(s, DATAGRAMSOCKETSOURCE_ERROR_BSOCKET);
        return;
    }
    
    #ifndef NDEBUG
    s->have_last_addr = 1;
    #endif
    
    // finish packet
    s->out_have = 0;
    PacketRecvInterface_Done(&s->output, res);
}

static void output_handler_recv (DatagramSocketSource *s, uint8_t *data)
{
    ASSERT(!s->out_have)
    DebugIn_AmOut(&s->d_in_error);
    DebugObject_Access(&s->d_obj);
    
    // set packet
    s->out_have = 1;
    s->out = data;
    
    try_recv(s);
    return;
}

static void socket_handler (DatagramSocketSource *s, int event)
{
    ASSERT(s->out_have)
    ASSERT(event == BSOCKET_READ)
    DebugIn_AmOut(&s->d_in_error);
    DebugObject_Access(&s->d_obj);
    
    BSocket_DisableEvent(s->bsock, BSOCKET_READ);
    
    try_recv(s);
    return;
}

static void retry_job_handler (DatagramSocketSource *s)
{
    ASSERT(s->out_have)
    DebugIn_AmOut(&s->d_in_error);
    DebugObject_Access(&s->d_obj);
    
    try_recv(s);
    return;
}

void DatagramSocketSource_Init (DatagramSocketSource *s, FlowErrorReporter rep, BSocket *bsock, int mtu, BPendingGroup *pg)
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
    PacketRecvInterface_Init(&s->output, mtu, (PacketRecvInterface_handler_recv)output_handler_recv, s, pg);
    
    // have no output packet
    s->out_have = 0;
    
    // init retry job
    BPending_Init(&s->retry_job, pg, (BPending_handler)retry_job_handler, s);
    
    DebugIn_Init(&s->d_in_error);
    DebugObject_Init(&s->d_obj);
    #ifndef NDEBUG
    s->have_last_addr = 0;
    #endif
}

void DatagramSocketSource_Free (DatagramSocketSource *s)
{
    DebugObject_Free(&s->d_obj);
    
    // free retry job
    BPending_Free(&s->retry_job);
    
    // free output
    PacketRecvInterface_Free(&s->output);
    
    // remove socket event handler
    BSocket_RemoveEventHandler(s->bsock, BSOCKET_READ);
    
    // free dead var
    DEAD_KILL(s->dead);
}

PacketRecvInterface * DatagramSocketSource_GetOutput (DatagramSocketSource *s)
{
    DebugObject_Access(&s->d_obj);
    
    return &s->output;
}

void DatagramSocketSource_GetLastAddresses (DatagramSocketSource *s, BAddr *addr, BIPAddr *local_addr)
{
    ASSERT(s->have_last_addr)
    DebugObject_Access(&s->d_obj);
    
    if (addr) {
        *addr = s->last_addr;
    }
    
    if (local_addr) {
        *local_addr = s->last_local_addr;
    }
}
