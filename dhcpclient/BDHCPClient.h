/**
 * @file BDHCPClient.h
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
 * DHCP client.
 */

#ifndef BADVPN_DHCPCLIENT_BDHCPCLIENT_H
#define BADVPN_DHCPCLIENT_BDHCPCLIENT_H

#include <system/DebugObject.h>
#include <system/BSocket.h>
#include <flow/PacketCopier.h>
#include <flow/SinglePacketBuffer.h>
#include <flow/DatagramSocketSink.h>
#include <flow/DatagramSocketSource.h>
#include <dhcpclient/BDHCPClientCore.h>
#include <dhcpclient/DHCPIpUdpDecoder.h>
#include <dhcpclient/DHCPIpUdpEncoder.h>

typedef struct {
    BReactor *reactor;
    BSocket sock;
    FlowErrorDomain domain;
    
    PacketCopier send_copier;
    DHCPIpUdpEncoder send_encoder;
    SinglePacketBuffer send_buffer;
    DatagramSocketSink send_sink;
    
    DatagramSocketSource recv_source;
    SinglePacketBuffer recv_buffer;
    DHCPIpUdpDecoder recv_decoder;
    PacketCopier recv_copier;
    
    BDHCPClientCore dhcp;
    DebugObject d_obj;
} BDHCPClient;

int BDHCPClient_Init (BDHCPClient *o, const char *ifname, BReactor *reactor, BDHCPClientCore_handler handler, void *user);
void BDHCPClient_Free (BDHCPClient *o);
void BDHCPClient_GetClientIP (BDHCPClient *o, uint32_t *out_ip);
void BDHCPClient_GetClientMask (BDHCPClient *o, uint32_t *out_mask);
int BDHCPClient_GetRouter (BDHCPClient *o, uint32_t *out_router);
int BDHCPClient_GetDNS (BDHCPClient *o, uint32_t *out_dns_servers, size_t max_dns_servers);

#endif
