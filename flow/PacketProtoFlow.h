/**
 * @file PacketProtoFlow.h
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
 * Buffer which encodes packets with PacketProto, with {@link BestEffortPacketWriteInterface}
 * input and {@link PacketPassInterface} output.
 */

#ifndef BADVPN_FLOW_PACKETPROTOFLOW_H
#define BADVPN_FLOW_PACKETPROTOFLOW_H

#include <system/DebugObject.h>
#include <system/BPending.h>
#include <flow/PacketBufferAsyncInput.h>
#include <flow/PacketProtoEncoder.h>
#include <flow/PacketBuffer.h>

/**
 * Buffer which encodes packets with PacketProto, with {@link BestEffortPacketWriteInterface}
 * input and {@link PacketPassInterface} output.
 */
typedef struct {
    PacketBufferAsyncInput ainput;
    PacketProtoEncoder encoder;
    PacketBuffer buffer;
    DebugObject d_obj;
} PacketProtoFlow;

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param input_mtu maximum input packet size. Must be >=0 and <=PACKETPROTO_MAXPAYLOAD.
 * @param num_packets minimum number of packets the buffer should hold. Must be >0.
 * @param output output interface. Its MTU must be >=PACKETPROTO_ENCLEN(input_mtu).
 * @param pg pending group
 * @return 1 on success, 0 on failure
 */
int PacketProtoFlow_Init (PacketProtoFlow *o, int input_mtu, int num_packets, PacketPassInterface *output, BPendingGroup *pg) WARN_UNUSED;

/**
 * Frees the object.
 * 
 * @param o the object
 */
void PacketProtoFlow_Free (PacketProtoFlow *o);

/**
 * Returns the input interface.
 * Its MTU will be as in {@link PacketProtoFlow_Init}.
 * Be aware that it may not be possible to write packets immediately after initializing
 * the object; the object starts its internal I/O with a {@link BPending} job.
 * 
 * @param o the object
 * @return input interface
 */
BestEffortPacketWriteInterface * PacketProtoFlow_GetInput (PacketProtoFlow *o);

#endif
