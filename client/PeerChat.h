/**
 * @file PeerChat.h
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

#ifndef BADVPN_PEERCHAT_H
#define BADVPN_PEERCHAT_H

#include <protocol/packetproto.h>
#include <protocol/scproto.h>
#include <misc/debug.h>
#include <base/DebugObject.h>
#include <flow/SinglePacketSender.h>
#include <flow/SinglePacketBuffer.h>
#include <flow/PacketProtoEncoder.h>
#include <flow/PacketCopier.h>
#include <client/SCOutmsgEncoder.h>

typedef void (*PeerChat_handler_error) (void *user);

typedef struct {
    void *user;
    PeerChat_handler_error handler_error;
    SinglePacketBuffer buffer;
    PacketProtoEncoder pp_encoder;
    SCOutmsgEncoder sc_encoder;
    PacketCopier copier;
    DebugObject d_obj;
} PeerChat;

int PeerChat_Init (PeerChat *o, peerid_t peer_id, PacketPassInterface *output, BPendingGroup *pg, void *user, PeerChat_handler_error handler_error) WARN_UNUSED;
void PeerChat_Free (PeerChat *o);
PacketPassInterface * PeerChat_GetInput (PeerChat *o);

#endif
