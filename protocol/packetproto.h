/**
 * @file packetproto.h
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
 * Definitions for PacketProto, a protocol that allows sending of packets
 * over a reliable stream connection.
 * 
 * All multi-byte integers in structs are little-endian, unless stated otherwise.
 * 
 * Packets are encoded into a stream by representing each packet with:
 *   - a 16-bit little-endian unsigned integer representing the length
 *     of the payload
 *   - that many bytes of payload
 */

#ifndef BADVPN_PROTOCOL_PACKETPROTO_H
#define BADVPN_PROTOCOL_PACKETPROTO_H

#include <stdint.h>
#include <limits.h>

#include <misc/balign.h>

/**
 * PacketProto packet header.
 * Wraps a single uint16_t in a packed struct for easy access.
 */
struct packetproto_header
{
    /**
     * Length of the packet payload that follows.
     */
    uint16_t len;
} __attribute__((packed));

#define PACKETPROTO_ENCLEN(_len) (sizeof(struct packetproto_header) + (_len))

#define PACKETPROTO_MAXPAYLOAD UINT16_MAX

#endif
