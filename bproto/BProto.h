/**
 * @file BProto.h
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
 * Definitions for BProto serialization.
 */

#ifndef BADVPN_BPROTO_BPROTO_H
#define BADVPN_BPROTO_BPROTO_H

#include <stdint.h>

#define BPROTO_TYPE_UINT8 1
#define BPROTO_TYPE_UINT16 2
#define BPROTO_TYPE_UINT32 3
#define BPROTO_TYPE_UINT64 4
#define BPROTO_TYPE_DATA 5
#define BPROTO_TYPE_CONSTDATA 6

struct BProto_header_s {
    uint16_t id;
    uint16_t type;
} __attribute__((packed));

struct BProto_uint8_s {
    uint8_t v;
} __attribute__((packed));

struct BProto_uint16_s {
    uint16_t v;
} __attribute__((packed));

struct BProto_uint32_s {
    uint32_t v;
} __attribute__((packed));

struct BProto_uint64_s {
    uint64_t v;
} __attribute__((packed));

struct BProto_data_header_s {
    uint32_t len;
} __attribute__((packed));

#endif
