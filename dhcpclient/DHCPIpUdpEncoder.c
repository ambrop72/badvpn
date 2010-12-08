/**
 * @file DHCPIpUdpEncoder.c
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

#include <dhcpclient/DHCPIpUdpEncoder.h>

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

struct combined_header {
    struct ipv4_header ip;
    struct udp_header udp;
} __attribute__((packed));

static void output_handler_recv (DHCPIpUdpEncoder *o, uint8_t *data)
{
    DebugObject_Access(&o->d_obj);
    
    // remember output packet
    o->data = data;
    
    // receive payload
    PacketRecvInterface_Receiver_Recv(o->input, o->data + sizeof(struct combined_header));
}

static void input_handler_done (DHCPIpUdpEncoder *o, int data_len)
{
    DebugObject_Access(&o->d_obj);
    
    struct combined_header *header = (void *)o->data;
    
    // write IP header
    struct ipv4_header *iph = &header->ip;
    iph->version4_ihl4 = IPV4_MAKE_VERSION_IHL(sizeof(*iph));
    iph->ds = hton8(0);
    iph->total_length = hton16(sizeof(struct combined_header) + data_len);
    iph->identification = hton16(0);
    iph->flags3_fragmentoffset13 = hton16(0);
    iph->ttl = hton8(64);
    iph->protocol = hton8(IPV4_PROTOCOL_UDP);
    iph->checksum = hton16(0);
    iph->source_address = hton32(0x00000000);
    iph->destination_address = hton32(0xFFFFFFFF);
    
    // compute and write IP header checksum
    uint32_t checksum = ipv4_checksum((uint8_t *)iph, sizeof(*iph));
    iph->checksum = checksum;
    
    // write UDP header
    struct udp_header *udph = &header->udp;
    udph->source_port = hton16(DHCP_CLIENT_PORT);
    udph->dest_port = hton16(DHCP_SERVER_PORT);
    udph->length = hton16(sizeof(*udph) + data_len);
    udph->checksum = hton16(0);
    
    // finish packet
    PacketRecvInterface_Done(&o->output, sizeof(struct combined_header) + data_len);
}

void DHCPIpUdpEncoder_Init (DHCPIpUdpEncoder *o, PacketRecvInterface *input, BPendingGroup *pg)
{
    ASSERT(PacketRecvInterface_GetMTU(input) <= INT_MAX - sizeof(struct combined_header))
    
    // init arguments
    o->input = input;
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    // init output
    PacketRecvInterface_Init(&o->output, sizeof(struct combined_header) + PacketRecvInterface_GetMTU(o->input), (PacketRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    DebugObject_Init(&o->d_obj);
}

void DHCPIpUdpEncoder_Free (DHCPIpUdpEncoder *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free output
    PacketRecvInterface_Free(&o->output);
}

PacketRecvInterface * DHCPIpUdpEncoder_GetOutput (DHCPIpUdpEncoder *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}
