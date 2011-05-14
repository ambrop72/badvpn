/**
 * @file PacketBuffer.h
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
 * Packet buffer with {@link PacketRecvInterface} input and {@link PacketPassInterface} output.
 */

#ifndef BADVPN_FLOW_PACKETBUFFER_H
#define BADVPN_FLOW_PACKETBUFFER_H

#include <stdint.h>

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <structure/ChunkBuffer2.h>
#include <flow/PacketRecvInterface.h>
#include <flow/PacketPassInterface.h>

/**
 * Packet buffer with {@link PacketRecvInterface} input and {@link PacketPassInterface} output.
 */
typedef struct {
    DebugObject d_obj;
    PacketRecvInterface *input;
    int input_mtu;
    PacketPassInterface *output;
    struct ChunkBuffer2_block *buf_data;
    ChunkBuffer2 buf;
} PacketBuffer;

/**
 * Initializes the buffer.
 * Output MTU must be >= input MTU.
 *
 * @param buf the object
 * @param input input interface
 * @param output output interface
 * @param num_packets minimum number of packets the buffer must hold. Must be >0.
 * @param pg pending group
 * @return 1 on success, 0 on failure
 */
int PacketBuffer_Init (PacketBuffer *buf, PacketRecvInterface *input, PacketPassInterface *output, int num_packets, BPendingGroup *pg) WARN_UNUSED;

/**
 * Frees the buffer.
 *
 * @param buf the object
 */
void PacketBuffer_Free (PacketBuffer *buf);

#endif
