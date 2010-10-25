/**
 * @file PacketRecvConnector.h
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
 * A {@link PacketRecvInterface} layer which allows the input to be
 * connected and disconnected on the fly.
 */

#ifndef BADVPN_FLOW_PACKETRECVCONNECTOR_H
#define BADVPN_FLOW_PACKETRECVCONNECTOR_H

#include <stdint.h>

#include <misc/dead.h>
#include <system/DebugObject.h>
#include <system/BPending.h>
#include <flow/PacketRecvInterface.h>

/**
 * A {@link PacketRecvInterface} layer which allows the input to be
 * connected and disconnected on the fly.
 */
typedef struct {
    DebugObject d_obj;
    dead_t dead;
    PacketRecvInterface output;
    int output_mtu;
    int out_have;
    uint8_t *out;
    PacketRecvInterface *input;
    dead_t input_dead;
    int in_blocking;
    BPending continue_job;
} PacketRecvConnector;

/**
 * Initializes the object.
 * The object is initialized in not connected state.
 *
 * @param o the object
 * @param mtu maximum output packet size. Must be >=0.
 * @param pg pending group
 */
void PacketRecvConnector_Init (PacketRecvConnector *o, int mtu, BPendingGroup *pg);

/**
 * Frees the object.
 *
 * @param o the object
 */
void PacketRecvConnector_Free (PacketRecvConnector *o);

/**
 * Returns the output interface.
 * The MTU of the interface will be as in {@link PacketRecvConnector_Init}.
 *
 * @param o the object
 * @return output interface
 */
PacketRecvInterface * PacketRecvConnector_GetOutput (PacketRecvConnector *o);

/**
 * Connects input.
 * The object must be in not connected state.
 * The object enters connected state.
 *
 * @param o the object
 * @param output input to connect. Its MTU must be <= MTU specified in
 *               {@link PacketRecvConnector_Init}.
 */
void PacketRecvConnector_ConnectInput (PacketRecvConnector *o, PacketRecvInterface *input);

/**
 * Disconnects input.
 * The object must be in connected state.
 * The object enters not connected state.
 *
 * @param o the object
 */
void PacketRecvConnector_DisconnectInput (PacketRecvConnector *o);

#endif
