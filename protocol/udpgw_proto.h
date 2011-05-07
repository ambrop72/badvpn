/**
 * @file udpgw_proto.h
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
 * Protocol for forwarding UDP over TCP. Messages should be carried with PacketProto.
 */

#ifndef BADVPN_PROTOCOL_UDPGW_PROTO_H
#define BADVPN_PROTOCOL_UDPGW_PROTO_H

#include <stdint.h>

#include <misc/bsize.h>

#define UDPGW_CLIENT_FLAG_KEEPALIVE (1 << 0)
#define UDPGW_CLIENT_FLAG_REBIND (1 << 1)

struct udpgw_header {
    uint8_t flags;
    uint16_t conid;
    uint32_t addr_ip;
    uint16_t addr_port;
} __attribute__((packed));

static int udpgw_compute_mtu (int dgram_mtu)
{
    bsize_t bs = bsize_add(
        bsize_fromsize(sizeof(struct udpgw_header)),
        bsize_fromint(dgram_mtu)
    );
    
    int s;
    if (!bsize_toint(bs, &s)) {
        return -1;
    }
    
    return s;
}

#endif
