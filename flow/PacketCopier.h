/**
 * @file PacketCopier.h
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
 * Object which copies packets.
 */

#ifndef BADVPN_FLOW_PACKETCOPIER_H
#define BADVPN_FLOW_PACKETCOPIER_H

#include <stdint.h>

#include <misc/dead.h>
#include <flow/PacketPassInterface.h>
#include <flow/PacketRecvInterface.h>

/**
 * Object which copies packets.
 * Input is via {@link PacketPassInterface}.
 * Output is via {@link PacketRecvInterface}.
 */
typedef struct {
    DebugObject d_obj;
    dead_t dead;
    PacketPassInterface input;
    PacketRecvInterface output;
    int in_len;
    uint8_t *in;
    int out_have;
    uint8_t *out;
} PacketCopier;

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param mtu maximum packet size. Must be >=0.
 */
void PacketCopier_Init (PacketCopier *o, int mtu);

/**
 * Frees the object.
 * 
 * @param o the object
 */
void PacketCopier_Free (PacketCopier *o);

/**
 * Returns the input interface.
 * The MTU of the interface will as in {@link PacketCopier_Init}.
 * The interface will support cancel functionality.
 * 
 * @return input interface
 */
PacketPassInterface * PacketCopier_GetInput (PacketCopier *o);

/**
 * Returns the output interface.
 * The MTU of the interface will be as in {@link PacketCopier_Init}.
 * 
 * @return output interface
 */
PacketRecvInterface * PacketCopier_GetOutput (PacketCopier *o);

#endif
