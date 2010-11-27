/**
 * @file socks_proto.h
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
 * Definitions for the SOCKS protocol.
 */

#ifndef BADVPN_MISC_SOCKS_PROTO_H
#define BADVPN_MISC_SOCKS_PROTO_H

#include <stdint.h>

#define SOCKS_VERSION 0x05

#define SOCKS_METHOD_NO_AUTHENTICATION_REQUIRED 0x00
#define SOCKS_METHOD_GSSAPI 0x01
#define SOCKS_METHOD_USERNAME_PASSWORD 0x02
#define SOCKS_METHOD_NO_ACCEPTABLE_METHODS 0xFF

#define SOCKS_CMD_CONNECT 0x01
#define SOCKS_CMD_BIND 0x02
#define SOCKS_CMD_UDP_ASSOCIATE 0x03

#define SOCKS_ATYP_IPV4 0x01
#define SOCKS_ATYP_DOMAINNAME 0x03
#define SOCKS_ATYP_IPV6 0x04

#define SOCKS_REP_SUCCEEDED 0x00
#define SOCKS_REP_GENERAL_FAILURE 0x01
#define SOCKS_REP_CONNECTION_NOT_ALLOWED 0x02
#define SOCKS_REP_NETWORK_UNREACHABLE 0x03
#define SOCKS_REP_HOST_UNREACHABLE 0x04
#define SOCKS_REP_CONNECTION_REFUSED 0x05
#define SOCKS_REP_TTL_EXPIRED 0x06
#define SOCKS_REP_COMMAND_NOT_SUPPORTED 0x07
#define SOCKS_REP_ADDRESS_TYPE_NOT_SUPPORTED 0x08

struct socks_client_hello_header {
    uint8_t ver;
    uint8_t nmethods;
} __attribute__((packed));

struct socks_client_hello_method {
    uint8_t method;
} __attribute__((packed));

struct socks_server_hello {
    uint8_t ver;
    uint8_t method;
} __attribute__((packed));

struct socks_request_header {
    uint8_t ver;
    uint8_t cmd;
    uint8_t rsv;
    uint8_t atyp;
} __attribute__((packed));

struct socks_reply_header {
    uint8_t ver;
    uint8_t rep;
    uint8_t rsv;
    uint8_t atyp;
} __attribute__((packed));

struct socks_addr_ipv4 {
    uint32_t addr;
    uint16_t port;
} __attribute__((packed));

struct socks_addr_ipv6 {
    uint8_t addr[16];
    uint16_t port;
} __attribute__((packed));    

#endif
