/**
 * @file DHCPIpUdpEncoder.h
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

#ifndef BADVPN_DHCPCLIENT_DHCPIPUDPENCODER_H
#define BADVPN_DHCPCLIENT_DHCPIPUDPENCODER_H

#include <stdint.h>

#include <base/DebugObject.h>
#include <flow/PacketRecvInterface.h>

typedef struct {
    PacketRecvInterface *input;
    PacketRecvInterface output;
    uint8_t *data;
    DebugObject d_obj;
} DHCPIpUdpEncoder;

void DHCPIpUdpEncoder_Init (DHCPIpUdpEncoder *o, PacketRecvInterface *input, BPendingGroup *pg);
void DHCPIpUdpEncoder_Free (DHCPIpUdpEncoder *o);
PacketRecvInterface * DHCPIpUdpEncoder_GetOutput (DHCPIpUdpEncoder *o);

#endif
