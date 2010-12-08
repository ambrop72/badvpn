/**
 * @file BDHCPClientCore.h
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
 * DHCP client, without I/O details.
 */

#ifndef BADVPN_DHCPCLIENT_BDHCPCLIENTCORE_H
#define BADVPN_DHCPCLIENT_BDHCPCLIENTCORE_H

#include <stdint.h>
#include <stddef.h>

#include <misc/dhcp_proto.h>
#include <system/BReactor.h>
#include <system/DebugObject.h>
#include <flow/PacketPassInterface.h>
#include <flow/PacketRecvInterface.h>

#define BDHCPCLIENTCORE_EVENT_UP 1
#define BDHCPCLIENTCORE_EVENT_DOWN 2

#define BDHCPCLIENTCORE_MAX_DOMAIN_NAME_SERVERS 16

typedef void (*BDHCPClientCore_handler) (void *user, int event);

typedef struct {
    PacketPassInterface *send_if;
    PacketRecvInterface *recv_if;
    uint8_t client_mac_addr[6];
    BReactor *reactor;
    BDHCPClientCore_handler handler;
    void *user;
    struct dhcp_header *send_buf;
    struct dhcp_header *recv_buf;
    int sending;
    BTimer reset_timer;
    BTimer request_timer;
    BTimer renew_timer;
    BTimer renew_request_timer;
    BTimer lease_timer;
    int state;
    int request_count;
    uint32_t xid;
    struct {
        uint32_t yiaddr;
        uint32_t dhcp_server_identifier;
    } offered;
    struct {
        uint32_t ip_address_lease_time;
        uint32_t subnet_mask;
        int have_router;
        uint32_t router;
        int domain_name_servers_count;
        uint32_t domain_name_servers[BDHCPCLIENTCORE_MAX_DOMAIN_NAME_SERVERS];
    } acked;
    DebugObject d_obj;
} BDHCPClientCore;

int BDHCPClientCore_Init (BDHCPClientCore *o, PacketPassInterface *send_if, PacketRecvInterface *recv_if, uint8_t *client_mac_addr, BReactor *reactor, BDHCPClientCore_handler handler, void *user);
void BDHCPClientCore_Free (BDHCPClientCore *o);
void BDHCPClientCore_GetClientIP (BDHCPClientCore *o, uint32_t *out_ip);
void BDHCPClientCore_GetClientMask (BDHCPClientCore *o, uint32_t *out_mask);
int BDHCPClientCore_GetRouter (BDHCPClientCore *o, uint32_t *out_router);
int BDHCPClientCore_GetDNS (BDHCPClientCore *o, uint32_t *out_dns_servers, size_t max_dns_servers);

#endif
