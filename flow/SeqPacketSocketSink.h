/**
 * @file SeqPacketSocketSink.h
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
 * A {@link PacketPassInterface} sink which sends packets to a seqpacket socket.
 */

#ifndef BADVPN_FLOW_SEQPACKETSOCKETSINK_H
#define BADVPN_FLOW_SEQPACKETSOCKETSINK_H

#include <stdint.h>

#include <misc/dead.h>
#include <system/DebugObject.h>
#include <system/BSocket.h>
#include <flow/PacketPassInterface.h>
#include <flow/error.h>

#define SEQPACKETSOCKETSINK_ERROR_BSOCKET 1
#define SEQPACKETSOCKETSINK_ERROR_WRONGSIZE 2

/**
 * A {@link PacketPassInterface} sink which sends packets to a seqpacket socket.
 */
typedef struct {
    DebugObject d_obj;
    dead_t dead;
    FlowErrorReporter rep;
    BSocket *bsock;
    PacketPassInterface input;
    int in_len;
    uint8_t *in;
    #ifndef NDEBUG
    int in_error;
    #endif
} SeqPacketSocketSink;

/**
 * Initializes the sink.
 *
 * @param s the object
 * @param rep error reporting data. Error code is an int. Possible error codes:
 *              - SEQPACKETSOCKETSINK_ERROR_BSOCKET: {@link BSocket_Send} failed
 *                with an unhandled error code
 *              - SEQPACKETSOCKETSINK_ERROR_WRONGSIZE: {@link BSocket_Send} succeeded,
 *                but did not send all of the packet
 *            The object must be freed from the error handler.
 * @param bsock socket to write packets to. Registers a BSOCKET_WRITE handler which
 *              must not be registered.
 * @param mtu maximum packet size. Must be >=0.
 */
void SeqPacketSocketSink_Init (SeqPacketSocketSink *s, FlowErrorReporter rep, BSocket *bsock, int mtu);

/**
 * Frees the sink.
 *
 * @param s the object
 */
void SeqPacketSocketSink_Free (SeqPacketSocketSink *s);

/**
 * Returns the input interface.
 * The MTU of the interface will be as in {@link SeqPacketSocketSink_Init}.
 *
 * @param s the object
 * @return input interface
 */
PacketPassInterface * SeqPacketSocketSink_GetInput (SeqPacketSocketSink *s);

#endif
