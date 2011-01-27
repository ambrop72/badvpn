/**
 * @file DPRelay.h
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

#ifndef BADVPN_CLIENT_DPRELAY_H
#define BADVPN_CLIENT_DPRELAY_H

#include <stdint.h>
#include <limits.h>

#include <protocol/scproto.h>
#include <protocol/dataproto.h>
#include <misc/debug.h>
#include <structure/LinkedList1.h>
#include <system/DebugObject.h>
#include <flow/BufferWriter.h>
#include <client/DataProto.h>

struct DPRelay_flow;

typedef struct {
    int frame_mtu;
    BufferWriter writer;
    DataProtoDevice device;
    struct DPRelay_flow *current_flow;
    DebugObject d_obj;
    DebugCounter d_ctr;
} DPRelayRouter;

typedef struct {
    DPRelayRouter *router;
    peerid_t source_id;
    LinkedList1 flows_list;
    DebugObject d_obj;
} DPRelaySource;

typedef struct {
    peerid_t dest_id;
    DataProtoDest *dest;
    LinkedList1 flows_list;
    DebugObject d_obj;
} DPRelaySink;

struct DPRelay_flow {
    DPRelaySource *src;
    DPRelaySink *sink;
    DataProtoLocalSource dpls;
    LinkedList1Node src_list_node;
    LinkedList1Node sink_list_node;
};

int DPRelayRouter_Init (DPRelayRouter *o, int frame_mtu, BReactor *reactor) WARN_UNUSED;
void DPRelayRouter_Free (DPRelayRouter *o);
void DPRelayRouter_SubmitFrame (DPRelayRouter *o, DPRelaySource *src, DPRelaySink *sink, uint8_t *data, int data_len, int num_packets, int inactivity_time);

void DPRelaySource_Init (DPRelaySource *o, DPRelayRouter *router, peerid_t source_id, BReactor *reactor);
void DPRelaySource_Free (DPRelaySource *o);
void DPRelaySource_PrepareFreeDestinations (DPRelaySource *o);

void DPRelaySink_Init (DPRelaySink *o, peerid_t dest_id);
void DPRelaySink_Free (DPRelaySink *o);
void DPRelaySink_Attach (DPRelaySink *o, DataProtoDest *dest);
void DPRelaySink_Detach (DPRelaySink *o);

#endif
