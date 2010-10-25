/**
 * @file SCKeepaliveSource.h
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
 * A {@link PacketRecvInterface} source which provides SCProto keepalive packets.
 */

#ifndef BADVPN_FLOW_SCKEEPALIVESOURCE_H
#define BADVPN_FLOW_SCKEEPALIVESOURCE_H

#include <system/DebugObject.h>
#include <flow/PacketRecvInterface.h>

/**
 * A {@link PacketRecvInterface} source which provides SCProto keepalive packets.
 */
typedef struct {
    DebugObject d_obj;
    PacketRecvInterface output;
} SCKeepaliveSource;

/**
 * Initializes the object.
 *
 * @param o the object
 */
void SCKeepaliveSource_Init (SCKeepaliveSource *o);

/**
 * Frees the object.
 *
 * @param o the object
 */
void SCKeepaliveSource_Free (SCKeepaliveSource *o);

/**
 * Returns the output interface.
 * The MTU of the output interface will be sizeof(struct sc_header).
 *
 * @param o the object
 * @return output interface
 */
PacketRecvInterface * SCKeepaliveSource_GetOutput (SCKeepaliveSource *o);

#endif
