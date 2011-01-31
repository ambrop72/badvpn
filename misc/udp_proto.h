/**
 * @file udp_proto.h
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
 * Definitions for the UDP protocol.
 */

#ifndef BADVPN_MISC_UDP_PROTO_H
#define BADVPN_MISC_UDP_PROTO_H

#include <stdint.h>

#include <misc/debug.h>
#include <misc/byteorder.h>
#include <misc/ipv4_proto.h>

struct udp_header {
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

static uint32_t udp_checksum_summer (uint8_t *data, uint16_t len)
{
    ASSERT(len % 2 == 0)
    
    struct ipv4_short *s = (void *)data;
    
    uint32_t t = 0;
    
    for (uint16_t i = 0; i < len / 2; i++) {
        t += ntoh16(s[i].v);
    }
    
    return t;
}

static uint16_t udp_checksum (uint8_t *udp, uint16_t len, uint32_t source_addr, uint32_t dest_addr)
{
    uint32_t t = 0;
    
    t += udp_checksum_summer((uint8_t *)&source_addr, sizeof(source_addr));
    t += udp_checksum_summer((uint8_t *)&dest_addr, sizeof(dest_addr));
    
    uint16_t x;
    x = hton16(IPV4_PROTOCOL_UDP);
    t += udp_checksum_summer((uint8_t *)&x, sizeof(x));
    x = hton16(len);
    t += udp_checksum_summer((uint8_t *)&x, sizeof(x));
    
    if (len % 2 == 0) {
        t += udp_checksum_summer(udp, len);
    } else {
        t += udp_checksum_summer(udp, len - 1);
        
        x = hton16(((uint16_t)udp[len - 1]) << 8);
        t += udp_checksum_summer((uint8_t *)&x, sizeof(x));
    }
    
    while (t >> 16) {
        t = (t & 0xFFFF) + (t >> 16);
    }
    
    if (t == 0) {
        t = UINT16_MAX;
    }
    
    return hton16(~t);
}

#endif
