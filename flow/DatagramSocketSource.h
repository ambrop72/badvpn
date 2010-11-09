/**
 * @file DatagramSocketSource.h
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
 * A {@link PacketRecvInterface} source which receives packets from a datagram socket.
 */

#ifndef BADVPN_FLOW_DATAGRAMSOCKETSOURCE_H
#define BADVPN_FLOW_DATAGRAMSOCKETSOURCE_H

#include <stdint.h>

#include <system/DebugObject.h>
#include <system/BSocket.h>
#include <system/BPending.h>
#include <flow/PacketRecvInterface.h>
#include <flow/error.h>

#define DATAGRAMSOCKETSOURCE_ERROR_BSOCKET 1

/**
 * A {@link PacketRecvInterface} source which receives packets from a datagram socket.
 */
typedef struct {
    FlowErrorReporter rep;
    BSocket *bsock;
    int mtu;
    PacketRecvInterface output;
    int out_have;
    uint8_t *out;
    BAddr last_addr;
    BIPAddr last_local_addr;
    BPending retry_job;
    DebugObject d_obj;
    #ifndef NDEBUG
    int have_last_addr;
    #endif
} DatagramSocketSource;

/**
 * Initializes the object.
 *
 * @param s the object
 * @param rep error reporting data. Error code is an int. Possible error codes:
 *              - DATAGRAMSOCKETSOURCE_ERROR_BSOCKET: {@link BSocket_RecvFromTo} failed
 *                with an unhandled error code
 *            On error, the object will continue to operate unless it is destroyed from
 *            the error handler.
 * @param bsock datagram socket to read data from. The BSOCKET_READ event must be disabled.
 * *            Takes over reading on the socket.
 * @param mtu maximum packet size. Must be >=0.
 * @param pg pending group
 */
void DatagramSocketSource_Init (DatagramSocketSource *s, FlowErrorReporter rep, BSocket *bsock, int mtu, BPendingGroup *pg);

/**
 * Frees the object.
 *
 * @param s the object
 */
void DatagramSocketSource_Free (DatagramSocketSource *s);

/**
 * Returns the output interface.
 *
 * @param s the object
 * @return output interface
 */
PacketRecvInterface * DatagramSocketSource_GetOutput (DatagramSocketSource *s);

/**
 * Returns the remote and local address of the last received packet.
 * At least one packet must have been received.
 *
 * @param s the object
 * @param addr where to put the remote address, if not NULL. The returned address
 *             will be valid.
 * @param local_addr where to put the local address, if not NULL. The returned
 *                   address may be an invalid address.
 */
void DatagramSocketSource_GetLastAddresses (DatagramSocketSource *s, BAddr *addr, BIPAddr *local_addr);

#endif
