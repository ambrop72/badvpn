/**
 * @file PacketPassConnector.h
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
 * A {@link PacketPassInterface} layer which allows the output to be
 * connected and disconnected on the fly.
 */

#ifndef BADVPN_FLOW_PACKETPASSCONNECTOR_H
#define BADVPN_FLOW_PACKETPASSCONNECTOR_H

#include <stdint.h>

#include <system/DebugObject.h>
#include <flow/PacketPassInterface.h>

/**
 * A {@link PacketPassInterface} layer which allows the output to be
 * connected and disconnected on the fly.
 */
typedef struct {
    PacketPassInterface input;
    int input_mtu;
    int in_len;
    uint8_t *in;
    PacketPassInterface *output;
    DebugObject d_obj;
} PacketPassConnector;

/**
 * Initializes the object.
 * The object is initialized in not connected state.
 *
 * @param o the object
 * @param mtu maximum input packet size. Must be >=0.
 * @param pg pending group
 */
void PacketPassConnector_Init (PacketPassConnector *o, int mtu, BPendingGroup *pg);

/**
 * Frees the object.
 *
 * @param o the object
 */
void PacketPassConnector_Free (PacketPassConnector *o);

/**
 * Returns the input interface.
 * The MTU of the interface will be as in {@link PacketPassConnector_Init}.
 *
 * @param o the object
 * @return input interface
 */
PacketPassInterface * PacketPassConnector_GetInput (PacketPassConnector *o);

/**
 * Connects output.
 * The object must be in not connected state.
 * The object enters connected state.
 *
 * @param o the object
 * @param output output to connect. Its MTU must be >= MTU specified in
 *               {@link PacketPassConnector_Init}.
 */
void PacketPassConnector_ConnectOutput (PacketPassConnector *o, PacketPassInterface *output);

/**
 * Disconnects output.
 * The object must be in connected state.
 * The object enters not connected state.
 *
 * @param o the object
 */
void PacketPassConnector_DisconnectOutput (PacketPassConnector *o);

#endif
