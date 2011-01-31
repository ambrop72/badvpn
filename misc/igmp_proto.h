/**
 * @file igmp_proto.h
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
 * Definitions for the IGMP protocol.
 */

#ifndef BADVPN_MISC_IGMP_PROTO_H
#define BADVPN_MISC_IGMP_PROTO_H

#include <stdint.h>

#define IGMP_TYPE_MEMBERSHIP_QUERY 0x11
#define IGMP_TYPE_V1_MEMBERSHIP_REPORT 0x12
#define IGMP_TYPE_V2_MEMBERSHIP_REPORT 0x16
#define IGMP_TYPE_V3_MEMBERSHIP_REPORT 0x22
#define IGMP_TYPE_V2_LEAVE_GROUP 0x17

#define IGMP_RECORD_TYPE_MODE_IS_INCLUDE 1
#define IGMP_RECORD_TYPE_MODE_IS_EXCLUDE 2
#define IGMP_RECORD_TYPE_CHANGE_TO_INCLUDE_MODE 3
#define IGMP_RECORD_TYPE_CHANGE_TO_EXCLUDE_MODE 4

struct igmp_source {
    uint32_t addr;
} __attribute__((packed));

struct igmp_base {
    uint8_t type;
    uint8_t max_resp_code;
    uint16_t checksum;
} __attribute__((packed));

struct igmp_v3_query_extra {
    uint32_t group;
    uint8_t reserved4_suppress1_qrv3;
    uint8_t qqic;
    uint16_t number_of_sources;
} __attribute__((packed));

struct igmp_v3_report_extra {
    uint16_t reserved;
    uint16_t number_of_group_records;
} __attribute__((packed));

struct igmp_v3_report_record {
    uint8_t type;
    uint8_t aux_data_len;
    uint16_t number_of_sources;
    uint32_t group;
} __attribute__((packed));

struct igmp_v2_extra {
    uint32_t group;
} __attribute__((packed));

#endif
