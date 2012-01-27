/**
 * @file udp_proto.h
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
