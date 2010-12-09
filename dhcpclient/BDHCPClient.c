/**
 * @file BDHCPClient.c
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

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>

#include <misc/debug.h>
#include <misc/byteorder.h>
#include <misc/ethernet_proto.h>
#include <misc/ipv4_proto.h>
#include <misc/udp_proto.h>
#include <system/BLog.h>

#include <dhcpclient/BDHCPClient.h>

#include <generated/blog_channel_BDHCPClient.h>

#define COMPONENT_SOURCE 1
#define COMPONENT_SINK 2

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

#define IPUDP_OVERHEAD (sizeof(struct ipv4_header) + sizeof(struct udp_header))

static void error_handler (BDHCPClient *o, int component, const void *data)
{
    DebugObject_Access(&o->d_obj);
    
    switch (component) {
        case COMPONENT_SOURCE: {
            BLog(BLOG_ERROR, "source error");
        } break;
        
        case COMPONENT_SINK: {
            BLog(BLOG_ERROR, "sink error");
        } break;
        
        default:
            ASSERT(0);
    }
}

static void dhcp_handler (BDHCPClient *o, int event)
{
    DebugObject_Access(&o->d_obj);
    
    switch (event) {
        case BDHCPCLIENTCORE_EVENT_UP:
            ASSERT(!o->up)
            o->up = 1;
            o->handler(o->user, BDHCPCLIENT_EVENT_UP);
            return;
            
        case BDHCPCLIENTCORE_EVENT_DOWN:
            ASSERT(o->up)
            o->up = 0;
            o->handler(o->user, BDHCPCLIENT_EVENT_DOWN);
            return;
            
        default:
            ASSERT(0);
    }
}

static int get_iface_info (const char *ifname, uint8_t *out_mac, int *out_mtu, int *out_ifindex)
{
    struct ifreq ifr;
    
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (!s) {
        BLog(BLOG_ERROR, "socket failed");
        goto fail0;
    }
    
    // get MAC
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    if (ioctl(s, SIOCGIFHWADDR, &ifr)) {
        BLog(BLOG_ERROR, "ioctl(SIOCGIFHWADDR) failed");
        goto fail1;
    }
    if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
        BLog(BLOG_ERROR, "hardware address not ethernet");
        goto fail1;
    }
    memcpy(out_mac, ifr.ifr_hwaddr.sa_data, 6);
    
    // get MTU
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    if (ioctl(s, SIOCGIFMTU, &ifr)) {
        BLog(BLOG_ERROR, "ioctl(SIOCGIFMTU) failed");
        goto fail1;
    }
    *out_mtu = ifr.ifr_mtu;
    
    // get interface index
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    if (ioctl(s, SIOCGIFINDEX, &ifr)) {
        BLog(BLOG_ERROR, "ioctl(SIOCGIFINDEX) failed");
        goto fail1;
    }
    *out_ifindex = ifr.ifr_ifindex;
    
    close(s);
    
    return 1;
    
fail1:
    close(s);
fail0:
    return 0;
}

int BDHCPClient_Init (BDHCPClient *o, const char *ifname, BReactor *reactor, BDHCPClient_handler handler, void *user)
{
    // init arguments
    o->reactor = reactor;
    o->handler = handler;
    o->user = user;
    
    // get interface information
    uint8_t if_mac[6];
    int if_mtu;
    int if_index;
    if (!get_iface_info(ifname, if_mac, &if_mtu, &if_index)) {
        BLog(BLOG_ERROR, "failed to get interface information");
        goto fail0;
    }
    
    BLog(BLOG_INFO, "if_mac=%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8" if_mtu=%d if_index=%d",
          if_mac[0], if_mac[1], if_mac[2], if_mac[3], if_mac[4], if_mac[5], if_mtu, if_index);
    
    if (if_mtu < IPUDP_OVERHEAD) {
        BLog(BLOG_ERROR, "MTU is too small for UDP/IP !?!");
        goto fail0;
    }
    
    int dhcp_mtu = if_mtu - IPUDP_OVERHEAD;
    
    // init socket
    if (BSocket_Init(&o->sock, o->reactor, BADDR_TYPE_PACKET, BSOCKET_TYPE_DGRAM) < 0) {
        BLog(BLOG_ERROR, "BSocket_Init failed");
        goto fail0;
    }
    
    // bind socket
    BAddr bind_addr;
    BAddr_InitPacket(&bind_addr, hton16(ETHERTYPE_IPV4), if_index, BADDR_PACKET_HEADER_TYPE_ETHERNET, BADDR_PACKET_PACKET_TYPE_HOST, if_mac);
    if (BSocket_Bind(&o->sock, &bind_addr) < 0) {
        BLog(BLOG_ERROR, "BSocket_Bind failed");
        goto fail1;
    }
    
    // init error handler
    FlowErrorDomain_Init(&o->domain, (FlowErrorDomain_handler)error_handler, o);
    
    // init sending
    
    // init sink
    BAddr dest_addr;
    uint8_t broadcast_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    BAddr_InitPacket(&dest_addr, hton16(ETHERTYPE_IPV4), if_index, BADDR_PACKET_HEADER_TYPE_ETHERNET, BADDR_PACKET_PACKET_TYPE_BROADCAST, broadcast_mac);
    BIPAddr local_addr;
    BIPAddr_InitInvalid(&local_addr);
    DatagramSocketSink_Init(&o->send_sink, FlowErrorReporter_Create(&o->domain, COMPONENT_SINK), &o->sock, if_mtu, dest_addr, local_addr, BReactor_PendingGroup(o->reactor));
    
    // init copier
    PacketCopier_Init(&o->send_copier, dhcp_mtu, BReactor_PendingGroup(o->reactor));
    
    // init encoder
    DHCPIpUdpEncoder_Init(&o->send_encoder, PacketCopier_GetOutput(&o->send_copier), BReactor_PendingGroup(o->reactor));
    
    // init buffer
    if (!SinglePacketBuffer_Init(&o->send_buffer, DHCPIpUdpEncoder_GetOutput(&o->send_encoder), DatagramSocketSink_GetInput(&o->send_sink), BReactor_PendingGroup(o->reactor))) {
        BLog(BLOG_ERROR, "SinglePacketBuffer_Init failed");
        goto fail2;
    }
    
    // init receiving
    
    // init source
    DatagramSocketSource_Init(&o->recv_source, FlowErrorReporter_Create(&o->domain, COMPONENT_SOURCE), &o->sock, if_mtu, BReactor_PendingGroup(o->reactor));
    
    // init copier
    PacketCopier_Init(&o->recv_copier, dhcp_mtu, BReactor_PendingGroup(o->reactor));
    
    // init decoder
    DHCPIpUdpDecoder_Init(&o->recv_decoder, PacketCopier_GetInput(&o->recv_copier), BReactor_PendingGroup(o->reactor));
    
    // init buffer
    if (!SinglePacketBuffer_Init(&o->recv_buffer, DatagramSocketSource_GetOutput(&o->recv_source), DHCPIpUdpDecoder_GetInput(&o->recv_decoder), BReactor_PendingGroup(o->reactor))) {
        BLog(BLOG_ERROR, "SinglePacketBuffer_Init failed");
        goto fail3;
    }
    
    // init dhcp
    if (!BDHCPClientCore_Init(&o->dhcp, PacketCopier_GetInput(&o->send_copier), PacketCopier_GetOutput(&o->recv_copier), if_mac, o->reactor, (BDHCPClientCore_handler)dhcp_handler, o)) {
        BLog(BLOG_ERROR, "BDHCPClientCore_Init failed");
        goto fail4;
    }
    
    // set not up
    o->up = 0;
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail4:
    SinglePacketBuffer_Free(&o->recv_buffer);
fail3:
    DHCPIpUdpDecoder_Free(&o->recv_decoder);
    PacketCopier_Free(&o->recv_copier);
    DatagramSocketSource_Free(&o->recv_source);
    SinglePacketBuffer_Free(&o->send_buffer);
fail2:
    DHCPIpUdpEncoder_Free(&o->send_encoder);
    PacketCopier_Free(&o->send_copier);
    DatagramSocketSink_Free(&o->send_sink);
fail1:
    BSocket_Free(&o->sock);
fail0:
    return 0;
}

void BDHCPClient_Free (BDHCPClient *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free dhcp
    BDHCPClientCore_Free(&o->dhcp);
    
    // free receiving
    SinglePacketBuffer_Free(&o->recv_buffer);
    DHCPIpUdpDecoder_Free(&o->recv_decoder);
    PacketCopier_Free(&o->recv_copier);
    DatagramSocketSource_Free(&o->recv_source);
    
    // free sending
    SinglePacketBuffer_Free(&o->send_buffer);
    DHCPIpUdpEncoder_Free(&o->send_encoder);
    PacketCopier_Free(&o->send_copier);
    DatagramSocketSink_Free(&o->send_sink);
    
    // free socket
    BSocket_Free(&o->sock);
}

int BDHCPClient_IsUp (BDHCPClient *o)
{
    DebugObject_Access(&o->d_obj);
    
    return o->up;
}

void BDHCPClient_GetClientIP (BDHCPClient *o, uint32_t *out_ip)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->up)
    
    BDHCPClientCore_GetClientIP(&o->dhcp, out_ip);
}

void BDHCPClient_GetClientMask (BDHCPClient *o, uint32_t *out_mask)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->up)
    
    BDHCPClientCore_GetClientMask(&o->dhcp, out_mask);
}

int BDHCPClient_GetRouter (BDHCPClient *o, uint32_t *out_router)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->up)
    
    return BDHCPClientCore_GetRouter(&o->dhcp, out_router);
}

int BDHCPClient_GetDNS (BDHCPClient *o, uint32_t *out_dns_servers, size_t max_dns_servers)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->up)
    
    return BDHCPClientCore_GetDNS(&o->dhcp, out_dns_servers, max_dns_servers);
}
