/**
 * @file DHCPIpUdpDecoder.c
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
 */

#include <limits.h>

#include <misc/ipv4_proto.h>
#include <misc/udp_proto.h>
#include <misc/byteorder.h>

#include <dhcpclient/DHCPIpUdpDecoder.h>

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

struct combined_header {
    struct ipv4_header ip;
    struct udp_header udp;
} __attribute__((packed));

static void input_handler_send (DHCPIpUdpDecoder *o, uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    DebugObject_Access(&o->d_obj);
    
    struct ipv4_header *iph;
    uint8_t *pl;
    int pl_len;
    
    if (!ipv4_check(data, data_len, &iph, &pl, &pl_len)) {
        goto fail;
    }
    
    if (ntoh8(iph->protocol) != IPV4_PROTOCOL_UDP) {
        goto fail;
    }
    
    if (pl_len < sizeof(struct udp_header)) {
        goto fail;
    }
    struct udp_header *udph = (void *)pl;
    
    if (ntoh16(udph->source_port) != DHCP_SERVER_PORT) {
        goto fail;
    }
    
    if (ntoh16(udph->dest_port) != DHCP_CLIENT_PORT) {
        goto fail;
    }
    
    int udph_length = ntoh16(udph->length);
    if (udph_length < sizeof(*udph)) {
        goto fail;
    }
    if (udph_length > data_len - (pl - data)) {
        goto fail;
    }
    
    // pass payload to output
    PacketPassInterface_Sender_Send(o->output, (uint8_t *)(udph + 1), udph_length - sizeof(*udph));
    
    return;
    
fail:
    PacketPassInterface_Done(&o->input);
}

static void output_handler_done (DHCPIpUdpDecoder *o)
{
    DebugObject_Access(&o->d_obj);
    
    PacketPassInterface_Done(&o->input);
}

void DHCPIpUdpDecoder_Init (DHCPIpUdpDecoder *o, PacketPassInterface *output, BPendingGroup *pg)
{
    ASSERT(PacketPassInterface_GetMTU(output) <= INT_MAX - sizeof(struct combined_header))
    
    // init arguments
    o->output = output;
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    // init input
    PacketPassInterface_Init(&o->input, sizeof(struct combined_header) + PacketPassInterface_GetMTU(o->output), (PacketPassInterface_handler_send)input_handler_send, o, pg);
    
    DebugObject_Init(&o->d_obj);
}

void DHCPIpUdpDecoder_Free (DHCPIpUdpDecoder *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free input
    PacketPassInterface_Free(&o->input);
}

PacketPassInterface * DHCPIpUdpDecoder_GetInput (DHCPIpUdpDecoder *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->input;
}
