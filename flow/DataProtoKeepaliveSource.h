/**
 * @file DataProtoKeepaliveSource.h
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
 * A {@link PacketRecvInterface} source which provides DataProto keepalive packets.
 */

#ifndef BADVPN_FLOW_DATAPROTOKEEPALIVESOURCE_H
#define BADVPN_FLOW_DATAPROTOKEEPALIVESOURCE_H

#include <system/DebugObject.h>
#include <flow/PacketRecvInterface.h>

/**
 * A {@link PacketRecvInterface} source which provides DataProto keepalive packets.
 * These packets have no payload, no destination peers and flags zero.
 */
typedef struct {
    DebugObject d_obj;
    PacketRecvInterface output;
} DataProtoKeepaliveSource;

/**
 * Initializes the object.
 *
 * @param o the object
 * @param pg pending group
 */
void DataProtoKeepaliveSource_Init (DataProtoKeepaliveSource *o, BPendingGroup *pg);

/**
 * Frees the object.
 *
 * @param o the object
 */
void DataProtoKeepaliveSource_Free (DataProtoKeepaliveSource *o);

/**
 * Returns the output interface.
 * The MTU of the output interface will be sizeof(struct dataproto_header).
 *
 * @param o the object
 * @return output interface
 */
PacketRecvInterface * DataProtoKeepaliveSource_GetOutput (DataProtoKeepaliveSource *o);

#endif
