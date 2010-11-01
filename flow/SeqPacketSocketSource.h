/**
 * @file SeqPacketSocketSource.h
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
 * A {@link PacketRecvInterface} source which receives packets from a seqpacket socket.
 */

#ifndef BADVPN_FLOW_SEQPACKETSOCKETSOURCE_H
#define BADVPN_FLOW_SEQPACKETSOCKETSOURCE_H

#include <misc/dead.h>
#include <system/DebugObject.h>
#include <system/BSocket.h>
#include <flow/error.h>
#include <flow/PacketRecvInterface.h>

#define SEQPACKETSOCKETSOURCE_ERROR_CLOSED 0
#define SEQPACKETSOCKETSOURCE_ERROR_BSOCKET 1

/**
 * A {@link PacketRecvInterface} source which receives packets from a seqpacket socket.
 */
typedef struct {
    DebugObject d_obj;
    dead_t dead;
    FlowErrorReporter rep;
    BSocket *bsock;
    int mtu;
    PacketRecvInterface output;
    int out_have;
    uint8_t *out;
    #ifndef NDEBUG
    int in_error;
    #endif
} SeqPacketSocketSource;

/**
 * Initializes the object.
 *
 * @param s the object
 * @param rep error reporting data. Error code is an int. Possible error codes:
 *              - SEQPACKETSOCKETSOURCE_ERROR_CLOSED: {@link BSocket_Recv} returned 0
 *              - SEQPACKETSOCKETSOURCE_ERROR_BSOCKET: {@link BSocket_Recv} failed
 *                with an unhandled error code
 *            The object must be freed from the error handler.
 * @param bsock socket to read data from. The BSOCKET_READ event must be disabled.
 * *            Takes over reading on the socket.
 * @param mtu maximum packet size. Must be >=0.
 */
void SeqPacketSocketSource_Init (SeqPacketSocketSource *s, FlowErrorReporter rep, BSocket *bsock, int mtu);

/**
 * Frees the object.
 *
 * @param s the object
 */
void SeqPacketSocketSource_Free (SeqPacketSocketSource *s);

/**
 * Returns the output interface.
 * The MTU of the interface will be as in {@link SeqPacketSocketSource_Init}.
 *
 * @param s the object
 * @return output interface
 */
PacketRecvInterface * SeqPacketSocketSource_GetOutput (SeqPacketSocketSource *s);

#endif
