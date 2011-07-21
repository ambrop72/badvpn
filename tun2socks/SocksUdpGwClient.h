/**
 * @file SocksUdpGwClient.h
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

#ifndef BADVPN_TUN2SOCKS_SOCKSUDPGWCLIENT_H
#define BADVPN_TUN2SOCKS_SOCKSUDPGWCLIENT_H

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <system/BReactor.h>
#include <udpgw_client/UdpGwClient.h>
#include <socksclient/BSocksClient.h>

typedef void (*SocksUdpGwClient_handler_received) (void *user, BAddr local_addr, BAddr remote_addr, const uint8_t *data, int data_len);

typedef struct {
    int udp_mtu;
    BAddr socks_server_addr;
    BAddr remote_udpgw_addr;
    BReactor *reactor;
    void *user;
    SocksUdpGwClient_handler_received handler_received;
    UdpGwClient udpgw_client;
    BTimer reconnect_timer;
    int have_socks;
    BSocksClient socks_client;
    int socks_up;
    DebugObject d_obj;
} SocksUdpGwClient;

int SocksUdpGwClient_Init (SocksUdpGwClient *o, int udp_mtu, int max_connections, int send_buffer_size, btime_t keepalive_time, BAddr socks_server_addr, BAddr remote_udpgw_addr, btime_t reconnect_time, BReactor *reactor, void *user,
                           SocksUdpGwClient_handler_received handler_received) WARN_UNUSED;
void SocksUdpGwClient_Free (SocksUdpGwClient *o);
void SocksUdpGwClient_SubmitPacket (SocksUdpGwClient *o, BAddr local_addr, BAddr remote_addr, const uint8_t *data, int data_len);

#endif
