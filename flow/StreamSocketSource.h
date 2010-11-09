/**
 * @file StreamSocketSource.h
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
 * A {@link StreamRecvInterface} source which receives data from a stream socket.
 */

#ifndef BADVPN_FLOW_STREAMSOCKETSOURCE_H
#define BADVPN_FLOW_STREAMSOCKETSOURCE_H

#include <stdint.h>

#include <misc/dead.h>
#include <system/DebugObject.h>
#include <system/BSocket.h>
#include <flow/StreamRecvInterface.h>
#include <flow/error.h>

#define STREAMSOCKETSOURCE_ERROR_CLOSED 0
#define STREAMSOCKETSOURCE_ERROR_BSOCKET 1

/**
 * A {@link StreamRecvInterface} source which receives data from a stream socket.
 */
typedef struct {
    FlowErrorReporter rep;
    BSocket *bsock;
    StreamRecvInterface output;
    int out_avail;
    uint8_t *out;
    DebugObject d_obj;
    #ifndef NDEBUG
    dead_t d_dead;
    #endif
} StreamSocketSource;

/**
 * Initializes the source.
 *
 * @param s the object
 * @param rep error reporting data. Error code is an int. Possible error codes:
 *              - STREAMSOCKETSOURCE_ERROR_CLOSED: {@link BSocket_Recv} returned 0
 *              - STREAMSOCKETSOURCE_ERROR_BSOCKET: {@link BSocket_Recv} failed
 *                with an unhandled error code
 *            The object must be freed from the error handler.
 * @param bsock stream socket to read data from. Registers a BSOCKET_READ handler which
 *              must not be registered.
 * @param pg pending group
 */
void StreamSocketSource_Init (StreamSocketSource *s, FlowErrorReporter rep, BSocket *bsock, BPendingGroup *pg);

/**
 * Frees the source.
 *
 * @param s the object
 */
void StreamSocketSource_Free (StreamSocketSource *s);

/**
 * Returns the output interface.
 *
 * @param s the object
 * @return output interface
 */
StreamRecvInterface * StreamSocketSource_GetOutput (StreamSocketSource *s);

#endif
