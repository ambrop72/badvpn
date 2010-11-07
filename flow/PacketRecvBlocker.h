/**
 * @file PacketRecvBlocker.h
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
 * {@link PacketRecvInterface} layer which blocks all output recv calls and only
 * passes a single blocked call on to input when the user wants so.
 */

#ifndef BADVPN_FLOW_PACKETRECVBLOCKER_H
#define BADVPN_FLOW_PACKETRECVBLOCKER_H

#include <stdint.h>

#include <system/DebugObject.h>
#include <flow/PacketRecvInterface.h>

/**
 * {@link PacketRecvInterface} layer which blocks all output recv calls and only
 * passes a single blocked call on to input when the user wants so.
 */
typedef struct {
    PacketRecvInterface output;
    int out_have;
    uint8_t *out;
    int out_input_blocking;
    PacketRecvInterface *input;
    DebugObject d_obj;
} PacketRecvBlocker;

/**
 * Initializes the object.
 *
 * @param o the object
 * @param input input interface
 */
void PacketRecvBlocker_Init (PacketRecvBlocker *o, PacketRecvInterface *input, BPendingGroup *pg);

/**
 * Frees the object.
 *
 * @param o the object
 */
void PacketRecvBlocker_Free (PacketRecvBlocker *o);

/**
 * Returns the output interface.
 * The MTU of the output interface will be the same as of the input interface.
 *
 * @param o the object
 * @return output interface
 */
PacketRecvInterface * PacketRecvBlocker_GetOutput (PacketRecvBlocker *o);

/**
 * Passes a blocked output recv call to input if there is one and it has not
 * been passed yet. Otherwise it does nothing.
 * Must not be called from input Recv calls.
 * This function may invoke I/O.
 *
 * @param o the object
 */
void PacketRecvBlocker_AllowBlockedPacket (PacketRecvBlocker *o);

#endif
