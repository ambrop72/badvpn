/**
 * @file DatagramSocketSink.h
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
 * 
 * @section DESCRIPTION
 * 
 * A {@link PacketPassInterface} sink which sends packets to a datagram socket.
 */

#ifndef BADVPN_FLOW_DATAGRAMSOCKETSINK_H
#define BADVPN_FLOW_DATAGRAMSOCKETSINK_H

#include <stdint.h>

#include <misc/dead.h>
#include <misc/debugin.h>
#include <system/DebugObject.h>
#include <system/BSocket.h>
#include <flow/PacketPassInterface.h>
#include <flow/error.h>

#define DATAGRAMSOCKETSINK_ERROR_BSOCKET 1
#define DATAGRAMSOCKETSINK_ERROR_WRONGSIZE 2

/**
 * A {@link PacketPassInterface} sink which sends packets to a datagram socket.
 */
typedef struct {
    dead_t dead;
    FlowErrorReporter rep;
    BSocket *bsock;
    BAddr addr;
    BIPAddr local_addr;
    PacketPassInterface input;
    int in_len;
    uint8_t *in;
    DebugIn d_in_error;
    DebugObject d_obj;
} DatagramSocketSink;

/**
 * Initializes the sink.
 *
 * @param s the object
 * @param rep error reporting data. Error code is an int. Possible error codes:
 *              - DATAGRAMSOCKETSINK_ERROR_BSOCKET: {@link BSocket_SendToFrom} failed
 *                with an unhandled error code
 *              - DATAGRAMSOCKETSINK_ERROR_WRONGSIZE: {@link BSocket_SendToFrom} succeeded,
 *                but did not send all of the packet
 *            On error, the object will continue to operate unless it is destroyed from
 *            the error handler.
 * @param bsock datagram socket to write packets to. Registers a BSOCKET_WRITE handler which
 *              must not be registered.
 * @param mtu maximum packet size. Must be >=0.
 * @param addr remote address. Must be recognized and valid. Passed to {@link BSocket_SendToFrom}.
 * @param local_addr source address. Must be recognized.
 *                   Passed to {@link BSocket_SendToFrom}.
 * @param pg pending group
 */
void DatagramSocketSink_Init (DatagramSocketSink *s, FlowErrorReporter rep, BSocket *bsock, int mtu, BAddr addr, BIPAddr local_addr, BPendingGroup *pg);

/**
 * Frees the sink.
 *
 * @param s the object
 */
void DatagramSocketSink_Free (DatagramSocketSink *s);

/**
 * Returns the input interface.
 *
 * @param s the object
 * @return input interface
 */
PacketPassInterface * DatagramSocketSink_GetInput (DatagramSocketSink *s);

/**
 * Sets sending addresses.
 *
 * @param s the object
 * @param addr remote address. Must be recognized and valid. Passed to {@link BSocket_SendToFrom}.
 * @param local_addr source address. Must be recognized.
 *                   Passed to {@link BSocket_SendToFrom}.
 */
void DatagramSocketSink_SetAddresses (DatagramSocketSink *s, BAddr addr, BIPAddr local_addr);

#endif
