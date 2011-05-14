/**
 * @file DHCPIpUdpDecoder.h
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

#ifndef BADVPN_DHCPCLIENT_DHCPIPUDPDECODER_H
#define BADVPN_DHCPCLIENT_DHCPIPUDPDECODER_H

#include <stdint.h>

#include <base/DebugObject.h>
#include <flow/PacketPassInterface.h>

typedef struct {
    PacketPassInterface *output;
    PacketPassInterface input;
    uint8_t *data;
    DebugObject d_obj;
} DHCPIpUdpDecoder;

void DHCPIpUdpDecoder_Init (DHCPIpUdpDecoder *o, PacketPassInterface *output, BPendingGroup *pg);
void DHCPIpUdpDecoder_Free (DHCPIpUdpDecoder *o);
PacketPassInterface * DHCPIpUdpDecoder_GetInput (DHCPIpUdpDecoder *o);

#endif
