/**
 * @file StreamPacketSender.h
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

#ifndef BADVPN_STREAMPACKETSENDER_H
#define BADVPN_STREAMPACKETSENDER_H

#include <base/DebugObject.h>
#include <flow/StreamPassInterface.h>
#include <flow/PacketPassInterface.h>

/**
 * Object which breaks an input stream into output packets. The resulting
 * packets will have positive length, and, when concatenated, will form the
 * original stream.
 * 
 * Input is with {@link StreamPassInterface}.
 * Output is with {@link PacketPassInterface}.
 */
typedef struct {
    PacketPassInterface *output;
    int output_mtu;
    StreamPassInterface input;
    int sending_len;
    DebugObject d_obj;
} StreamPacketSender;

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param output output interface. Its MTU must be >0.
 * @param pg pending group we live in
 */
void StreamPacketSender_Init (StreamPacketSender *o, PacketPassInterface *output, BPendingGroup *pg);

/**
 * Frees the object.
 * 
 * @param o the object
 */
void StreamPacketSender_Free (StreamPacketSender *o);

/**
 * Returns the input interface.
 * 
 * @param o the object
 * @return input interface
 */
StreamPassInterface * StreamPacketSender_GetInput (StreamPacketSender *o);

#endif
