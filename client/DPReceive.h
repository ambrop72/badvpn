/**
 * @file DPReceive.h
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
 * Receive processing for the VPN client.
 */

#ifndef BADVPN_CLIENT_DPRECEIVE_H
#define BADVPN_CLIENT_DPRECEIVE_H

#include <protocol/scproto.h>
#include <misc/debugcounter.h>
#include <misc/debug.h>
#include <structure/LinkedList2.h>
#include <base/DebugObject.h>
#include <client/DataProto.h>
#include <client/DPRelay.h>
#include <client/FrameDecider.h>

typedef void (*DPReceiveDevice_output_func) (void *output_user, uint8_t *data, int data_len);

struct DPReceiveReceiver_s;

typedef struct {
    int device_mtu;
    DPReceiveDevice_output_func output_func;
    void *output_func_user;
    BReactor *reactor;
    int relay_flow_buffer_size;
    int relay_flow_inactivity_time;
    int packet_mtu;
    DPRelayRouter relay_router;
    int have_peer_id;
    peerid_t peer_id;
    int freeing;
    LinkedList2 peers_list;
    DebugObject d_obj;
} DPReceiveDevice;

typedef struct {
    DPReceiveDevice *device;
    peerid_t peer_id;
    FrameDeciderPeer *decider_peer;
    int is_relay_client;
    DPRelaySource relay_source;
    DPRelaySink relay_sink;
    DataProtoSink *dp_sink;
    LinkedList2Node list_node;
    DebugObject d_obj;
    DebugCounter d_receivers_ctr;
} DPReceivePeer;

typedef struct DPReceiveReceiver_s {
    DPReceivePeer *peer;
    DPReceiveDevice *device;
    PacketPassInterface recv_if;
    DebugObject d_obj;
} DPReceiveReceiver;

int DPReceiveDevice_Init (DPReceiveDevice *o, int device_mtu, DPReceiveDevice_output_func output_func, void *output_func_user, BReactor *reactor, int relay_flow_buffer_size, int relay_flow_inactivity_time) WARN_UNUSED;
void DPReceiveDevice_Free (DPReceiveDevice *o);
void DPReceiveDevice_SetPeerID (DPReceiveDevice *o, peerid_t peer_id);

void DPReceivePeer_Init (DPReceivePeer *o, DPReceiveDevice *device, peerid_t peer_id, FrameDeciderPeer *decider_peer, int is_relay_client);
void DPReceivePeer_Free (DPReceivePeer *o);
void DPReceivePeer_AttachSink (DPReceivePeer *o, DataProtoSink *dp_sink);
void DPReceivePeer_DetachSink (DPReceivePeer *o);

void DPReceiveReceiver_Init (DPReceiveReceiver *o, DPReceivePeer *peer);
void DPReceiveReceiver_Free (DPReceiveReceiver *o);
PacketPassInterface * DPReceiveReceiver_GetInput (DPReceiveReceiver *o);

#endif
