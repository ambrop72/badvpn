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
#include <base/BPending.h>
#include <flow/SinglePacketSender.h>
#include <flow/PacketProtoEncoder.h>
#include <flow/PacketCopier.h>
#include <client/SCOutmsgEncoder.h>

typedef void (*PeerChat_handler_error) (void *user);
typedef void (*PeerChat_handler_message) (void *user, uint8_t *data, int data_len);

typedef struct {
    void *user;
    PeerChat_handler_error handler_error;
    PeerChat_handler_message handler_message;
    PacketProtoEncoder pp_encoder;
    SCOutmsgEncoder sc_encoder;
    PacketCopier copier;
    BPending recv_job;
    uint8_t *recv_data;
    int recv_data_len;
    DebugObject d_obj;
} PeerChat;

int PeerChat_Init (PeerChat *o, peerid_t peer_id, BPendingGroup *pg, void *user, PeerChat_handler_error handler_error,
                                                                                 PeerChat_handler_message handler_message) WARN_UNUSED;
void PeerChat_Free (PeerChat *o);
PacketPassInterface * PeerChat_GetSendInput (PeerChat *o);
PacketRecvInterface * PeerChat_GetSendOutput (PeerChat *o);
void PeerChat_InputReceived (PeerChat *o, uint8_t *data, int data_len);

#endif
