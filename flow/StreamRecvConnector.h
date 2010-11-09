/**
 * @file StreamRecvConnector.h
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
 * A {@link StreamRecvInterface} layer which allows the input to be
 * connected and disconnected on the fly.
 */

#ifndef BADVPN_FLOW_STREAMRECVCONNECTOR_H
#define BADVPN_FLOW_STREAMRECVCONNECTOR_H

#include <stdint.h>

#include <system/DebugObject.h>
#include <flow/StreamRecvInterface.h>

/**
 * A {@link StreamRecvInterface} layer which allows the input to be
 * connected and disconnected on the fly.
 */
typedef struct {
    StreamRecvInterface output;
    int out_avail;
    uint8_t *out;
    StreamRecvInterface *input;
    int in_blocking;
    DebugObject d_obj;
} StreamRecvConnector;

/**
 * Initializes the object.
 * The object is initialized in not connected state.
 *
 * @param o the object
 * @param pg pending group
 */
void StreamRecvConnector_Init (StreamRecvConnector *o, BPendingGroup *pg);

/**
 * Frees the object.
 *
 * @param o the object
 */
void StreamRecvConnector_Free (StreamRecvConnector *o);

/**
 * Returns the output interface.
 *
 * @param o the object
 * @return output interface
 */
StreamRecvInterface * StreamRecvConnector_GetOutput (StreamRecvConnector *o);

/**
 * Connects input.
 * The object must be in not connected state.
 * The object enters connected state.
 *
 * @param o the object
 * @param output input to connect
 */
void StreamRecvConnector_ConnectInput (StreamRecvConnector *o, StreamRecvInterface *input);

/**
 * Disconnects input.
 * The object must be in connected state.
 * The object enters not connected state.
 *
 * @param o the object
 */
void StreamRecvConnector_DisconnectInput (StreamRecvConnector *o);

#endif
