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

#include <misc/debug.h>
#include <misc/byteorder.h>

#define IPV4_PROTOCOL_IGMP 2
#define IPV4_PROTOCOL_UDP 17

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

#define IPV4_MAKE_VERSION_IHL(size) (((size)/4) + (4 << 4))

struct ipv4_short {
    uint16_t v;
} __attribute__((packed));

static uint16_t ipv4_checksum (uint8_t *ip_hdr, uint16_t len)
{
    ASSERT(len % 2 == 0)
    
    struct ipv4_short *s = (void *)ip_hdr;
    
    uint32_t t = 0;
    
    for (uint16_t i = 0; i < len / 2; i++) {
        t += ntoh16(s[i].v);
    }
    
    t = (t&0xFFFF) + (t >> 16);
    
    return hton16(~t);
}

static int ipv4_check (uint8_t *data, int data_len, struct ipv4_header **out_header, uint8_t **out_payload, int *out_payload_len)
{
    ASSERT(data_len >= 0)
    
    // check base header
    if (data_len < sizeof(struct ipv4_header)) {
        return 0;
    }
    struct ipv4_header *header = (struct ipv4_header *)data;
    
    // check version
    if (IPV4_GET_VERSION(*header) != 4) {
        return 0;
    }
    
    // check options
    uint16_t header_len = IPV4_GET_IHL(*header) * 4;
    if (header_len < sizeof(struct ipv4_header)) {
        return 0;
    }
    if (header_len > data_len) {
        return 0;
    }
    
    // check total length
    uint16_t total_length = ntoh16(header->total_length);
    if (total_length < header_len) {
        return 0;
    }
    if (total_length > data_len) {
        return 0;
    }
    
    // check checksum
    uint16_t checksum_in_packet = header->checksum;
    header->checksum = hton16(0);
    uint16_t checksum_computed = ipv4_checksum(data, header_len);
    header->checksum = checksum_in_packet;
    if (checksum_in_packet != checksum_computed) {
        return 0;
    }
    
    *out_header = header;
    *out_payload = data + header_len;
    *out_payload_len = total_length - header_len;
    
    return 1;
}

#endif
