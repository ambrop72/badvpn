/**
 * @file DatagramSocketSink.c
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

#include <flow/DatagramSocketSink.h>

static void report_error (DatagramSocketSink *s, int error)
{
    FlowErrorReporter_ReportError(&s->rep, &error);
    return;
}

static void try_send (DatagramSocketSink *s)
{
    ASSERT(s->in_len >= 0)
    
    int res = BSocket_SendToFrom(s->bsock, s->in, s->in_len, &s->addr, &s->local_addr);
    if (res < 0 && BSocket_GetError(s->bsock) == BSOCKET_ERROR_LATER) {
        // wait for socket in socket_handler
        BSocket_EnableEvent(s->bsock, BSOCKET_WRITE);
        return;
    }
    
    int old_len = s->in_len;
    
    // finish packet
    s->in_len = -1;
    PacketPassInterface_Done(&s->input);
    
    if (res < 0) {
        report_error(s, DATAGRAMSOCKETSINK_ERROR_BSOCKET);
        return;
    }
    else if (res != old_len) {
        report_error(s, DATAGRAMSOCKETSINK_ERROR_WRONGSIZE);
        return;
    }
}

static void input_handler_send (DatagramSocketSink *s, uint8_t *data, int data_len)
{
    ASSERT(s->in_len == -1)
    ASSERT(data_len >= 0)
    DebugObject_Access(&s->d_obj);
    
    // set packet
    s->in_len = data_len;
    s->in = data;
    
    try_send(s);
    return;
}

static void socket_handler (DatagramSocketSink *s, int event)
{
    ASSERT(s->in_len >= 0)
    ASSERT(event == BSOCKET_WRITE)
    DebugObject_Access(&s->d_obj);
    
    BSocket_DisableEvent(s->bsock, BSOCKET_WRITE);
    
    try_send(s);
    return;
}

void DatagramSocketSink_Init (DatagramSocketSink *s, FlowErrorReporter rep, BSocket *bsock, int mtu, BAddr addr, BIPAddr local_addr, BPendingGroup *pg)
{
    ASSERT(mtu >= 0)
    ASSERT(!BAddr_IsInvalid(&addr))
    BIPAddr_Assert(&local_addr);
    
    // init arguments
    s->rep = rep;
    s->bsock = bsock;
    s->addr = addr;
    s->local_addr = local_addr;
    
    // add socket event handler
    BSocket_AddEventHandler(s->bsock, BSOCKET_WRITE, (BSocket_handler)socket_handler, s);
    
    // init input
    PacketPassInterface_Init(&s->input, mtu, (PacketPassInterface_handler_send)input_handler_send, s, pg);
    
    // have no input packet
    s->in_len = -1;
    
    DebugObject_Init(&s->d_obj);
}

void DatagramSocketSink_Free (DatagramSocketSink *s)
{
    DebugObject_Free(&s->d_obj);

    // free input
    PacketPassInterface_Free(&s->input);
    
    // remove socket event handler
    BSocket_RemoveEventHandler(s->bsock, BSOCKET_WRITE);
}

PacketPassInterface * DatagramSocketSink_GetInput (DatagramSocketSink *s)
{
    DebugObject_Access(&s->d_obj);
    
    return &s->input;
}

void DatagramSocketSink_SetAddresses (DatagramSocketSink *s, BAddr addr, BIPAddr local_addr)
{
    ASSERT(!BAddr_IsInvalid(&addr))
    BIPAddr_Assert(&local_addr);
    DebugObject_Access(&s->d_obj);
    
    s->addr = addr;
    s->local_addr = local_addr;
}
