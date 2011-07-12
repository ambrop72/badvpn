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

#include <stdint.h>

#include <protocol/scproto.h>
#include <base/DebugObject.h>
#include <flow/PacketRecvInterface.h>

#define SCOUTMSG_OVERHEAD (sizeof(struct sc_header) + sizeof(struct sc_client_outmsg))

typedef struct {
    peerid_t peer_id;
    PacketRecvInterface *input;
    PacketRecvInterface output;
    uint8_t *output_packet;
    DebugObject d_obj;
} SCOutmsgEncoder;

void SCOutmsgEncoder_Init (SCOutmsgEncoder *enc, peerid_t peer_id, PacketRecvInterface *input, BPendingGroup *pg);
void SCOutmsgEncoder_Free (SCOutmsgEncoder *enc);
PacketRecvInterface * SCOutmsgEncoder_GetOutput (SCOutmsgEncoder *enc);

#endif
