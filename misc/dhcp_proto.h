/**
 * @file dhcp_proto.h
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
 * Definitions for the DHCP protocol.
 */

#ifndef BADVPN_MISC_DHCP_PROTO_H
#define BADVPN_MISC_DHCP_PROTO_H

#include <stdint.h>

#define DHCP_OP_BOOTREQUEST 1
#define DHCP_OP_BOOTREPLY 2

#define DHCP_HARDWARE_ADDRESS_TYPE_ETHERNET 1

#define DHCP_MAGIC 0x63825363

#define DHCP_OPTION_PAD 0
#define DHCP_OPTION_END 255

#define DHCP_OPTION_SUBNET_MASK 1
#define DHCP_OPTION_ROUTER 3
#define DHCP_OPTION_DOMAIN_NAME_SERVER 6
#define DHCP_OPTION_HOST_NAME 12
#define DHCP_OPTION_REQUESTED_IP_ADDRESS 50
#define DHCP_OPTION_IP_ADDRESS_LEASE_TIME 51
#define DHCP_OPTION_DHCP_MESSAGE_TYPE 53
#define DHCP_OPTION_DHCP_SERVER_IDENTIFIER 54
#define DHCP_OPTION_PARAMETER_REQUEST_LIST 55
#define DHCP_OPTION_MAXIMUM_MESSAGE_SIZE 57
#define DHCP_OPTION_RENEWAL_TIME_VALUE 58
#define DHCP_OPTION_REBINDING_TIME_VALUE 59
#define DHCP_OPTION_VENDOR_CLASS_IDENTIFIER 60

#define DHCP_MESSAGE_TYPE_DISCOVER 1
#define DHCP_MESSAGE_TYPE_OFFER 2
#define DHCP_MESSAGE_TYPE_REQUEST 3
#define DHCP_MESSAGE_TYPE_DECLINE 4
#define DHCP_MESSAGE_TYPE_ACK 5
#define DHCP_MESSAGE_TYPE_NAK 6
#define DHCP_MESSAGE_TYPE_RELEASE 7

struct dhcp_header {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t magic;
} __attribute__((packed));

struct dhcp_option_header {
    uint8_t type;
    uint8_t len;
} __attribute__((packed));

struct dhcp_option_dhcp_message_type {
    uint8_t type;
} __attribute__((packed));

struct dhcp_option_maximum_message_size {
    uint16_t size;
} __attribute__((packed));

struct dhcp_option_dhcp_server_identifier {
    uint32_t id;
} __attribute__((packed));

struct dhcp_option_time {
    uint32_t time;
} __attribute__((packed));

struct dhcp_option_addr {
    uint32_t addr;
} __attribute__((packed));

#endif
