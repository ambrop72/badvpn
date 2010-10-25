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

#include <stdlib.h>

#include <misc/debug.h>

#include <flow/DatagramSocketSource.h>

static int report_error (DatagramSocketSource *s, int error)
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

static int output_handler_recv (DatagramSocketSource *s, uint8_t *data, int *data_len)
{
    ASSERT(!s->out_have)
    ASSERT(!s->in_error)
    
    int res;
    
    while (1) {
        res = BSocket_RecvFromTo(s->bsock, data, s->mtu, &s->last_addr, &s->last_local_addr);
        if (res < 0) {
            int error = BSocket_GetError(s->bsock);
            if (error == BSOCKET_ERROR_LATER) {
                s->out_have = 1;
                s->out = data;
                BSocket_EnableEvent(s->bsock, BSOCKET_READ);
                return 0;
            }
            if (report_error(s, DATAGRAMSOCKETSOURCE_ERROR_BSOCKET) < 0) {
                return -1;
            }
            continue;
        }
        break;
    }
    
    #ifndef NDEBUG
    s->have_last_addr = 1;
    #endif
    
    *data_len = res;
    return 1;
}

static void socket_handler (DatagramSocketSource *s, int event)
{
    ASSERT(s->out_have)
    ASSERT(event == BSOCKET_READ)
    ASSERT(!s->in_error)
    
    int res;
    
    while (1) {
        res = BSocket_RecvFromTo(s->bsock, s->out, s->mtu, &s->last_addr, &s->last_local_addr);
        if (res < 0) {
            int error = BSocket_GetError(s->bsock);
            if (error == BSOCKET_ERROR_LATER) {
                // nothing to receive, continue in socket_handler
                return;
            }
            if (report_error(s, DATAGRAMSOCKETSOURCE_ERROR_BSOCKET) < 0) {
                return;
            }
            continue;
        }
        break;
    }
    
    BSocket_DisableEvent(s->bsock, BSOCKET_READ);
    s->out_have = 0;
    
    #ifndef NDEBUG
    s->have_last_addr = 1;
    #endif
    
    PacketRecvInterface_Done(&s->output, res);
    return;
}

void DatagramSocketSource_Init (DatagramSocketSource *s, FlowErrorReporter rep, BSocket *bsock, int mtu)
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
    s->have_last_addr = 0;
    s->in_error = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&s->d_obj);
}

void DatagramSocketSource_Free (DatagramSocketSource *s)
{
    // free debug object
    DebugObject_Free(&s->d_obj);

    // free output
    PacketRecvInterface_Free(&s->output);
    
    // remove socket event handler
    BSocket_RemoveEventHandler(s->bsock, BSOCKET_READ);
    
    // free dead var
    DEAD_KILL(s->dead);
}

PacketRecvInterface * DatagramSocketSource_GetOutput (DatagramSocketSource *s)
{
    return &s->output;
}

void DatagramSocketSource_GetLastAddresses (DatagramSocketSource *s, BAddr *addr, BIPAddr *local_addr)
{
    ASSERT(s->have_last_addr)
    
    if (addr) {
        *addr = s->last_addr;
    }
    
    if (local_addr) {
        *local_addr = s->last_local_addr;
    }
}
