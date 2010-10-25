/**
 * @file ipv4_proto.h
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
 * Definitions for the IPv4 protocol.
 */

#ifndef BADVPN_MISC_IPV4_PROTO_H
#define BADVPN_MISC_IPV4_PROTO_H

#include <stdint.h>

#define IPV4_PROTOCOL_IGMP 2

struct ipv4_header {
    uint8_t version4_ihl4;
    uint8_t ds;
    uint16_t total_length;
    //
    uint16_t identification;
    uint16_t flags3_fragmentoffset13;
    //
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    //
    uint32_t source_address;
    //
    uint32_t destination_address;
} __attribute__((packed));

#define IPV4_GET_VERSION(_header) (((_header).version4_ihl4&0xF0)>>4)
#define IPV4_GET_IHL(_header) (((_header).version4_ihl4&0x0F)>>0)

#endif
