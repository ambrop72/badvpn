/**
 * @file StreamSocketSink.h
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
 * A {@link StreamPassInterface} sink which sends data to a stream socket.
 */

#ifndef BADVPN_FLOW_STREAMSOCKETSINK_H
#define BADVPN_FLOW_STREAMSOCKETSINK_H

#include <stdint.h>

#include <misc/dead.h>
#include <system/DebugObject.h>
#include <system/BSocket.h>
#include <flow/StreamPassInterface.h>
#include <flow/FlowError.h>

#define STREAMSOCKETSINK_ERROR_BSOCKET 1

/**
 * A {@link StreamPassInterface} sink which sends data to a stream socket.
 */
typedef struct {
    FlowErrorReporter rep;
    BSocket *bsock;
    StreamPassInterface input;
    int in_len;
    uint8_t *in;
    DebugObject d_obj;
    #ifndef NDEBUG
    dead_t d_dead;
    #endif
} StreamSocketSink;

/**
 * Initializes the sink.
 *
 * @param s the object
 * @param rep error reporting data. Error code is an int. Possible error codes:
 *              - STREAMSOCKETSINK_ERROR_BSOCKET: {@link BSocket_Send} failed
 *                with an unhandled error code
 *            The object must be freed from the error handler.
 * @param bsock stream socket to write data to. Registers a BSOCKET_WRITE handler which
 *              must not be registered.
 * @param pg pending group
 */
void StreamSocketSink_Init (StreamSocketSink *s, FlowErrorReporter rep, BSocket *bsock, BPendingGroup *pg);

/**
 * Frees the sink.
 *
 * @param s the object
 */
void StreamSocketSink_Free (StreamSocketSink *s);

/**
 * Returns the input interface.
 *
 * @param s the object
 * @return input interface
 */
StreamPassInterface * StreamSocketSink_GetInput (StreamSocketSink *s);

#endif
