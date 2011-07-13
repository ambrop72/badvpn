/**
 * @file SinglePacketSource.h
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
 */

#ifndef BADVPN_SINGLEPACKETSOURCE_H
#define BADVPN_SINGLEPACKETSOURCE_H

#include <base/DebugObject.h>
#include <flow/PacketRecvInterface.h>

/**
 * An object which provides a single packet through {@link PacketRecvInterface}.
 */
typedef struct {
    uint8_t *packet;
    int packet_len;
    int sent;
    PacketRecvInterface output;
    DebugObject d_obj;
} SinglePacketSource;

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param packet packet to provide to the output. Must stay available until the packet is provided.
 * @param packet_len length of packet. Must be >=0.
 * @param pg pending group we live in
 */
void SinglePacketSource_Init (SinglePacketSource *o, uint8_t *packet, int packet_len, BPendingGroup *pg);

/**
 * Frees the object.
 * 
 * @param o the object
 */
void SinglePacketSource_Free (SinglePacketSource *o);

/**
 * Returns the output interface.
 * The MTU of the interface will be packet_len.
 * 
 * @param o the object
 * @return output interface
 */
PacketRecvInterface * SinglePacketSource_GetOutput (SinglePacketSource *o);

#endif
