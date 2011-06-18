/**
 * @file SocksUdpGwClient.c
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

#include <misc/debug.h>
#include <base/BLog.h>

#include <tun2socks/SocksUdpGwClient.h>

#include <generated/blog_channel_SocksUdpGwClient.h>

static void free_socks (SocksUdpGwClient *o);
static void try_connect (SocksUdpGwClient *o);
static void reconnect_timer_handler (SocksUdpGwClient *o);
static void socks_client_handler (SocksUdpGwClient *o, int event);
static void udpgw_handler_servererror (SocksUdpGwClient *o);
static void udpgw_handler_received (SocksUdpGwClient *o, BAddr local_addr, BAddr remote_addr, const uint8_t *data, int data_len);

static void free_socks (SocksUdpGwClient *o)
{
    ASSERT(o->have_socks)
    
    // disconnect udpgw client from SOCKS
    if (o->socks_up) {
        UdpGwClient_DisconnectServer(&o->udpgw_client);
    }
    
    // free SOCKS client
    BSocksClient_Free(&o->socks_client);
    
    // set have no SOCKS
    o->have_socks = 0;
}

static void try_connect (SocksUdpGwClient *o)
{
    ASSERT(!o->have_socks)
    ASSERT(!BTimer_IsRunning(&o->reconnect_timer))
    
    // init SOCKS client
    if (!BSocksClient_Init(&o->socks_client, o->socks_server_addr, o->remote_udpgw_addr, (BSocksClient_handler)socks_client_handler, o, o->reactor)) {
        BLog(BLOG_ERROR, "BSocksClient_Init failed");
        goto fail0;
    }
    
    // set have SOCKS
    o->have_socks = 1;
    
    // set SOCKS not up
    o->socks_up = 0;
    
    return;
    
fail0:
    // set reconnect timer
    BReactor_SetTimer(o->reactor, &o->reconnect_timer);
}

static void reconnect_timer_handler (SocksUdpGwClient *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(!o->have_socks)
    
    // try connecting
    try_connect(o);
}

static void socks_client_handler (SocksUdpGwClient *o, int event)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->have_socks)
    
    switch (event) {
        case BSOCKSCLIENT_EVENT_UP: {
            ASSERT(!o->socks_up)
            
            BLog(BLOG_INFO, "SOCKS up");
            
            // connect udpgw client to SOCKS
            if (!UdpGwClient_ConnectServer(&o->udpgw_client, BSocksClient_GetSendInterface(&o->socks_client), BSocksClient_GetRecvInterface(&o->socks_client))) {
                BLog(BLOG_ERROR, "UdpGwClient_ConnectServer failed");
                goto fail0;
            }
            
            // set SOCKS up
            o->socks_up = 1;
            
            return;
            
        fail0:
            // free SOCKS
            free_socks(o);
            
            // set reconnect timer
            BReactor_SetTimer(o->reactor, &o->reconnect_timer);
        } break;
        
        case BSOCKSCLIENT_EVENT_ERROR:
        case BSOCKSCLIENT_EVENT_ERROR_CLOSED: {
            BLog(BLOG_INFO, "SOCKS error");
            
            // free SOCKS
            free_socks(o);
            
            // set reconnect timer
            BReactor_SetTimer(o->reactor, &o->reconnect_timer);
        } break;
        
        default: ASSERT(0);
    }
}

static void udpgw_handler_servererror (SocksUdpGwClient *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->have_socks)
    ASSERT(o->socks_up)
    
    BLog(BLOG_ERROR, "client reports server error");
    
    // free SOCKS
    free_socks(o);
    
    // set reconnect timer
    BReactor_SetTimer(o->reactor, &o->reconnect_timer);
}

static void udpgw_handler_received (SocksUdpGwClient *o, BAddr local_addr, BAddr remote_addr, const uint8_t *data, int data_len)
{
    DebugObject_Access(&o->d_obj);
    
    // submit to user
    o->handler_received(o->user, local_addr, remote_addr, data, data_len);
    return;
}

void SocksUdpGwClient_Init (SocksUdpGwClient *o, int udp_mtu, int max_connections, int send_buffer_size, btime_t keepalive_time, BAddr socks_server_addr, BAddr remote_udpgw_addr, btime_t reconnect_time, BReactor *reactor, void *user,
                            SocksUdpGwClient_handler_received handler_received)
{
    ASSERT(udp_mtu >= 0)
    ASSERT(udpgw_compute_mtu(udp_mtu) >= 0)
    ASSERT(udpgw_compute_mtu(udp_mtu) <= PACKETPROTO_MAXPAYLOAD)
    ASSERT(max_connections > 0)
    ASSERT(send_buffer_size > 0)
    ASSERT(!BAddr_IsInvalid(&socks_server_addr))
    ASSERT(remote_udpgw_addr.type == BADDR_TYPE_IPV4 || remote_udpgw_addr.type == BADDR_TYPE_IPV6)
    
    // init arguments
    o->udp_mtu = udp_mtu;
    o->socks_server_addr = socks_server_addr;
    o->remote_udpgw_addr = remote_udpgw_addr;
    o->reactor = reactor;
    o->user = user;
    o->handler_received = handler_received;
    
    // init udpgw client
    UdpGwClient_Init(&o->udpgw_client, udp_mtu, max_connections, send_buffer_size, keepalive_time, o->reactor, o,
                     (UdpGwClient_handler_servererror)udpgw_handler_servererror,
                     (UdpGwClient_handler_received)udpgw_handler_received);
    
    // init reconnect timer
    BTimer_Init(&o->reconnect_timer, reconnect_time, (BTimer_handler)reconnect_timer_handler, o);
    
    // set have no SOCKS
    o->have_socks = 0;
    
    // try connecting
    try_connect(o);
    
    DebugObject_Init(&o->d_obj);
}

void SocksUdpGwClient_Free (SocksUdpGwClient *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free SOCKS
    if (o->have_socks) {
        free_socks(o);
    }
    
    // free reconnect timer
    BReactor_RemoveTimer(o->reactor, &o->reconnect_timer);
    
    // free udpgw client
    UdpGwClient_Free(&o->udpgw_client);
}

void SocksUdpGwClient_SubmitPacket (SocksUdpGwClient *o, BAddr local_addr, BAddr remote_addr, const uint8_t *data, int data_len)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(local_addr.type == BADDR_TYPE_IPV4)
    ASSERT(remote_addr.type == BADDR_TYPE_IPV4)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= o->udp_mtu)
    
    // submit to udpgw client
    UdpGwClient_SubmitPacket(&o->udpgw_client, local_addr, remote_addr, data, data_len);
}
