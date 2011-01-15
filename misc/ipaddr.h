/**
 * @file ipaddr.h
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
 * IP address parsing functions.
 */

#ifndef BADVPN_MISC_IPADDR_H
#define BADVPN_MISC_IPADDR_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <misc/debug.h>
#include <misc/byteorder.h>

struct ipv4_ifaddr {
    uint32_t addr;
    int prefix;
};

static int ipaddr_parse_ipv4_addr_bin (char *name, size_t name_len, uint32_t *out_addr);
static int ipaddr_parse_ipv4_addr (char *name, uint32_t *out_addr);
static int ipaddr_parse_ipv4_prefix (char *str, int *num);
static int ipaddr_parse_ipv4_ifaddr (char *str, struct ipv4_ifaddr *out);
static int ipaddr_ipv4_ifaddr_from_addr_mask (uint32_t addr, uint32_t mask, struct ipv4_ifaddr *out);
static int ipaddr_ipv4_mask_from_prefix (int prefix);
static int ipaddr_ipv4_addrs_in_network (uint32_t addr1, uint32_t addr2, int netprefix);

static int ipaddr_char_is_digit (char c)
{
    return (c >= '0' && c <= '9');
}

static int ipaddr_parse_number (char *data, size_t data_len, int *out)
{
    if (data_len == 0) {
        return 0;
    }
    
    for (size_t i = 0; i < data_len; i++) {
        if (!ipaddr_char_is_digit(data[i])) {
            return 0;
        }
    }
    
    char data2[data_len + 1];
    memcpy(data2, data, data_len);
    data2[data_len] = '\0';
    
    *out = atoi(data2);
    
    return 1;
}

int ipaddr_parse_ipv4_addr_bin (char *name, size_t name_len, uint32_t *out_addr)
{
    for (size_t i = 0; ; i++) {
        size_t j;
        for (j = 0; j < name_len && name[j] != '.'; j++) {
            if (!ipaddr_char_is_digit(name[j])) {
                return 0;
            }
        }
        
        if ((j == name_len && i < 3) || (j < name_len && i == 3)) {
            return 0;
        }
        
        if (j < 1 || j > 3) {
            return 0;
        }
        
        int d;
        if (!ipaddr_parse_number(name, j, &d)) {
            return 0;
        }
        
        if (d > 255) {
            return 0;
        }
        
        ((uint8_t *)out_addr)[i] = d;
        
        if (i == 3) {
            return 1;
        }
        
        name += j + 1;
        name_len -= j + 1;
    }
}

int ipaddr_parse_ipv4_addr (char *name, uint32_t *out_addr)
{
    return ipaddr_parse_ipv4_addr_bin(name, strlen(name), out_addr);
}

int ipaddr_parse_ipv4_prefix (char *str, int *num)
{
    if (strlen(str) > 2 || !ipaddr_parse_number(str, strlen(str), num)) {
        return 0;
    }
    if (*num > 32) {
        return 0;
    }
    return 1;
}

int ipaddr_parse_ipv4_ifaddr (char *str, struct ipv4_ifaddr *out)
{
    char *slash = strstr(str, "/");
    if (!slash) {
        return 0;
    }
    
    if (!ipaddr_parse_ipv4_addr_bin(str, (slash - str), &out->addr)) {
        return 0;
    }
    
    char *prefix = slash + 1;
    size_t prefix_len = strlen(prefix);
    
    if (prefix_len > 2) {
        return 0;
    }
    
    if (!ipaddr_parse_number(prefix, prefix_len, &out->prefix)) {
        return 0;
    }
    
    if (out->prefix > 32) {
        return 0;
    }
    
    return 1;
}

int ipaddr_ipv4_ifaddr_from_addr_mask (uint32_t addr, uint32_t mask, struct ipv4_ifaddr *out)
{
    // check mask
    uint32_t t = 0;
    int i;
    for (i = 0; i <= 32; i++) {
        if (ntoh32(mask) == t) {
            break;
        }
        if (i < 32) {
            t |= (1 << (32 - i - 1));
        }
    }
    if (!(i <= 32)) {
        return 0;
    }
    
    out->addr = addr;
    out->prefix = i;
    return 1;
}

int ipaddr_ipv4_mask_from_prefix (int prefix)
{
    ASSERT(prefix >= 0)
    ASSERT(prefix <= 32)
    
    uint32_t t = 0;
    for (int i = 0; i < prefix; i++) {
        t |= 1 << (32 - i - 1);
    }
    
    return hton32(t);
}

int ipaddr_ipv4_addrs_in_network (uint32_t addr1, uint32_t addr2, int netprefix)
{
    ASSERT(netprefix >= 0)
    ASSERT(netprefix <= 32)
    
    uint32_t mask = ipaddr_ipv4_mask_from_prefix(netprefix);
    
    return !!((addr1 & mask) == (addr2 & mask));
}

#endif
