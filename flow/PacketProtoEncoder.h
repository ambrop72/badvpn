/**
 * @file PacketProtoEncoder.h
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
 * Object which encodes packets according to PacketProto.
 */

#ifndef BADVPN_FLOW_PACKETPROTOENCODER_H
#define BADVPN_FLOW_PACKETPROTOENCODER_H

#include <stdint.h>

#include <base/DebugObject.h>
#include <flow/PacketRecvInterface.h>

/**
 * Object which encodes packets according to PacketProto.
 *
 * Input is with {@link PacketRecvInterface}.
 * Output is with {@link PacketRecvInterface}.
 */
typedef struct {
    PacketRecvInterface *input;
    PacketRecvInterface output;
    uint8_t *output_packet;
    DebugObject d_obj;
} PacketProtoEncoder;

/**
 * Initializes the object.
 *
 * @param enc the object
 * @param input input interface. Its MTU must be <=PACKETPROTO_MAXPAYLOAD.
 * @param pg pending group
 */
void PacketProtoEncoder_Init (PacketProtoEncoder *enc, PacketRecvInterface *input, BPendingGroup *pg);

/**
 * Frees the object.
 *
 * @param enc the object
 */
void PacketProtoEncoder_Free (PacketProtoEncoder *enc);

/**
 * Returns the output interface.
 * The MTU of the output interface is PACKETPROTO_ENCLEN(MTU of input interface).
 *
 * @param enc the object
 * @return output interface
 */
PacketRecvInterface * PacketProtoEncoder_GetOutput (PacketProtoEncoder *enc);

#endif
