/**
 * @file cc.h
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

#ifndef LWIP_CUSTOM_CC_H
#define LWIP_CUSTOM_CC_H

#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>

#include <misc/debug.h>
#include <misc/byteorder.h>

#define u8_t uint8_t
#define s8_t int8_t
#define u16_t uint16_t
#define s16_t int16_t
#define u32_t uint32_t
#define s32_t int32_t
#define mem_ptr_t uintptr_t

#define PACK_STRUCT_STRUCT __attribute__((packed))

#define LWIP_PLATFORM_DIAG(x) DEBUG x
#define LWIP_PLATFORM_ASSERT(x) ({ DEBUG("lwip assertion failure: %s", (x)); abort(); })

#define U16_F PRIu16
#define S16_F PRId16
#define X16_F PRIx16
#define U32_F PRIu32
#define S32_F PRId32
#define X32_F PRIx32
#define SZT_F "zu"

#define LWIP_PLATFORM_BYTESWAP 1
#define LWIP_PLATFORM_HTONS(x) hton16(x)
#define LWIP_PLATFORM_HTONL(x) hton32(x)

// for BYTE_ORDER
#ifdef BADVPN_USE_WINAPI
    #include <sys/param.h>
#endif
#ifdef BADVPN_LINUX
    #include <endian.h>
#endif
#ifdef BADVPN_FREEBSD
    #include <machine/endian.h>
#endif

#endif
