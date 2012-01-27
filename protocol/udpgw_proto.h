/**
 * @file udpgw_proto.h
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
