/**
 * @file SCOutmsgEncoder.h
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

#ifndef BADVPN_SCOUTMSGENCODER_H
#define BADVPN_SCOUTMSGENCODER_H

#include <protocol/scproto.h>
#include <base/DebugObject.h>
#include <flow/PacketRecvInterface.h>

#define SCOUTMSG_OVERHEAD (sizeof(struct sc_header) + sizeof(struct sc_client_outmsg))

/**
 * A {@link PacketRecvInterface} layer which encodes SCProto outgoing messages.
 */
typedef struct {
    peerid_t peer_id;
    PacketRecvInterface *input;
    PacketRecvInterface output;
    uint8_t *output_packet;
    DebugObject d_obj;
} SCOutmsgEncoder;

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param peer_id destination peer for messages
 * @param input input interface. Its MTU muse be <= (INT_MAX - SCOUTMSG_OVERHEAD).
 * @param pg pending group we live in
 */
void SCOutmsgEncoder_Init (SCOutmsgEncoder *o, peerid_t peer_id, PacketRecvInterface *input, BPendingGroup *pg);

/**
 * Frees the object.
 * 
 * @param o the object
 */
void SCOutmsgEncoder_Free (SCOutmsgEncoder *o);

/**
 * Returns the output interface.
 * The MTU of the interface will be (SCOUTMSG_OVERHEAD + input MTU).
 * 
 * @param o the object
 * @return output interface
 */
PacketRecvInterface * SCOutmsgEncoder_GetOutput (SCOutmsgEncoder *o);

#endif
