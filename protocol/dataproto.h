/**
 * @file dataproto.h
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
 * Definitions for DataProto, the protocol for data transport between VPN peers.
 * 
 * All multi-byte integers in structs are little-endian, unless stated otherwise.
 * 
 * A DataProto packet consists of:
 *   - the header (struct {@link dataproto_header})
 *   - between zero and DATAPROTO_MAX_PEER_IDS destination peer IDs (struct {@link dataproto_peer_id})
 *   - the payload, e.g. Ethernet frame
 */

#ifndef BADVPN_PROTOCOL_DATAPROTO_H
#define BADVPN_PROTOCOL_DATAPROTO_H

#include <stdint.h>

#include <protocol/scproto.h>

#define DATAPROTO_MAX_PEER_IDS 1

#define DATAPROTO_FLAGS_RECEIVING_KEEPALIVES 1

/**
 * DataProto header.
 */
struct dataproto_header {
    /**
     * Bitwise OR of flags. Possible flags:
     *   - DATAPROTO_FLAGS_RECEIVING_KEEPALIVES
     *     Indicates that when the peer sent this packet, it has received at least
     *     one packet from the other peer in the last keep-alive tolerance time.
     */
    uint8_t flags;
    
    /**
     * ID of the peer this frame originates from.
     */
    peerid_t from_id;
    
    /**
     * Number of destination peer IDs that follow.
     * Must be <=DATAPROTO_MAX_PEER_IDS.
     */
    peerid_t num_peer_ids;
} __attribute__((packed));

/**
 * Structure for a destination peer ID in DataProto.
 * Wraps a single peerid_t in a packed struct for easy access.
 */
struct dataproto_peer_id {
    peerid_t id;
} __attribute__((packed));

#define DATAPROTO_MAX_OVERHEAD (sizeof(struct dataproto_header) + DATAPROTO_MAX_PEER_IDS * sizeof(struct dataproto_peer_id))

#endif
