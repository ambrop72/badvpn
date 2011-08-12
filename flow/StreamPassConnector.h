/**
 * @file StreamPassConnector.h
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
 * A {@link StreamPassInterface} layer which allows the output to be
 * connected and disconnected on the fly.
 */

#ifndef BADVPN_FLOW_STREAMPASSCONNECTOR_H
#define BADVPN_FLOW_STREAMPASSCONNECTOR_H

#include <stdint.h>

#include <base/DebugObject.h>
#include <flow/StreamPassInterface.h>

/**
 * A {@link StreamPassInterface} layer which allows the output to be
 * connected and disconnected on the fly.
 */
typedef struct {
    StreamPassInterface input;
    int in_len;
    uint8_t *in;
    StreamPassInterface *output;
    DebugObject d_obj;
} StreamPassConnector;

/**
 * Initializes the object.
 * The object is initialized in not connected state.
 *
 * @param o the object
 * @param pg pending group
 */
void StreamPassConnector_Init (StreamPassConnector *o, BPendingGroup *pg);

/**
 * Frees the object.
 *
 * @param o the object
 */
void StreamPassConnector_Free (StreamPassConnector *o);

/**
 * Returns the input interface.
 *
 * @param o the object
 * @return input interface
 */
StreamPassInterface * StreamPassConnector_GetInput (StreamPassConnector *o);

/**
 * Connects output.
 * The object must be in not connected state.
 * The object enters connected state.
 *
 * @param o the object
 * @param output output to connect
 */
void StreamPassConnector_ConnectOutput (StreamPassConnector *o, StreamPassInterface *output);

/**
 * Disconnects output.
 * The object must be in connected state.
 * The object enters not connected state.
 *
 * @param o the object
 */
void StreamPassConnector_DisconnectOutput (StreamPassConnector *o);

#endif
