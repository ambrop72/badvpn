/**
 * @file NCDIfConfig.h
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

#ifndef BADVPN_NCD_NCDIFCONFIG_H
#define BADVPN_NCD_NCDIFCONFIG_H

#include <stddef.h>

#include <misc/ipaddr.h>

#define NCDIFCONFIG_FLAG_EXISTS (1 << 0)
#define NCDIFCONFIG_FLAG_UP (1 << 1)
#define NCDIFCONFIG_FLAG_RUNNING (1 << 2)

int NCDIfConfig_query (const char *ifname);

int NCDIfConfig_set_up (const char *ifname);
int NCDIfConfig_set_down (const char *ifname);

int NCDIfConfig_add_ipv4_addr (const char *ifname, struct ipv4_ifaddr ifaddr);
int NCDIfConfig_remove_ipv4_addr (const char *ifname, struct ipv4_ifaddr ifaddr);

int NCDIfConfig_add_ipv4_route (struct ipv4_ifaddr dest, const uint32_t *gateway, int metric, const char *device);
int NCDIfConfig_remove_ipv4_route (struct ipv4_ifaddr dest, const uint32_t *gateway, int metric, const char *device);

int NCDIfConfig_set_dns_servers (uint32_t *servers, size_t num_servers);

int NCDIfConfig_make_tuntap (const char *ifname, const char *owner, int tun);
int NCDIfConfig_remove_tuntap (const char *ifname, int tun);

#endif
