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

static int report_error (DatagramSocketSink *s, int error)
{
    #ifndef NDEBUG
    s->in_error = 1;
    #endif
    
    DEAD_ENTER(s->dead)
    FlowErrorReporter_ReportError(&s->rep, &error);
    if (DEAD_LEAVE(s->dead)) {
        return -1;
    }
    
    #ifndef NDEBUG
    s->in_error = 0;
    #endif
    
    return 0;
}

static int input_handler_send (DatagramSocketSink *s, uint8_t *data, int data_len)
{
    ASSERT(s->in_len == -1)
    ASSERT(data_len >= 0)
    ASSERT(!s->in_error)
    
    int res = BSocket_SendToFrom(s->bsock, data, data_len, &s->addr, &s->local_addr);
    if (res < 0) {
        int error = BSocket_GetError(s->bsock);
        if (error == BSOCKET_ERROR_LATER) {
            s->in_len = data_len;
            s->in = data;
            BSocket_EnableEvent(s->bsock, BSOCKET_WRITE);
            return 0;
        }
        if (report_error(s, DATAGRAMSOCKETSINK_ERROR_BSOCKET) < 0) {
            return -1;
        }
    } else {
        if (res != data_len) {
            if (report_error(s, DATAGRAMSOCKETSINK_ERROR_WRONGSIZE) < 0) {
                return -1;
            }
        }
    }
    
    return 1;
}

static void socket_handler (DatagramSocketSink *s, int event)
{
    ASSERT(s->in_len >= 0)
    ASSERT(event == BSOCKET_WRITE)
    ASSERT(!s->in_error)
    
    int res = BSocket_SendToFrom(s->bsock, s->in, s->in_len, &s->addr, &s->local_addr);
    if (res < 0) {
        int error = BSocket_GetError(s->bsock);
        if (error == BSOCKET_ERROR_LATER) {
            return;
        }
        if (report_error(s, DATAGRAMSOCKETSINK_ERROR_BSOCKET) < 0) {
            return;
        }
    } else {
        if (res != s->in_len) {
            if (report_error(s, DATAGRAMSOCKETSINK_ERROR_WRONGSIZE) < 0) {
                return;
            }
        }
    }
    
    BSocket_DisableEvent(s->bsock, BSOCKET_WRITE);
    s->in_len = -1;
    
    PacketPassInterface_Done(&s->input);
    return;
}

void DatagramSocketSink_Init (DatagramSocketSink *s, FlowErrorReporter rep, BSocket *bsock, int mtu, BAddr addr, BIPAddr local_addr)
{
    ASSERT(mtu >= 0)
    ASSERT(BAddr_IsRecognized(&addr) && !BAddr_IsInvalid(&addr))
    ASSERT(BIPAddr_IsRecognized(&local_addr))
    
    // init arguments
    s->rep = rep;
    s->bsock = bsock;
    s->addr = addr;
    s->local_addr = local_addr;
    
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
    
    // init debug object
    DebugObject_Init(&s->d_obj);
}

void DatagramSocketSink_Free (DatagramSocketSink *s)
{
    // free debug object
    DebugObject_Free(&s->d_obj);

    // free input
    PacketPassInterface_Free(&s->input);
    
    // remove socket event handler
    BSocket_RemoveEventHandler(s->bsock, BSOCKET_WRITE);
    
    // free dead var
    DEAD_KILL(s->dead);
}

PacketPassInterface * DatagramSocketSink_GetInput (DatagramSocketSink *s)
{
    return &s->input;
}

void DatagramSocketSink_SetAddresses (DatagramSocketSink *s, BAddr addr, BIPAddr local_addr)
{
    ASSERT(BAddr_IsRecognized(&addr) && !BAddr_IsInvalid(&addr))
    ASSERT(BIPAddr_IsRecognized(&local_addr))
    
    s->addr = addr;
    s->local_addr = local_addr;
}
