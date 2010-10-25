/**
 * @file overflow.h
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
 * Functions for checking for overflow of integer addition.
 */

#ifndef BADVPN_MISC_OVERFLOW_H
#define BADVPN_MISC_OVERFLOW_H

#include <limits.h>
#include <stdint.h>

#define __DEFINE_UNSIGNED_OVERFLOW(_name, _type, _max) \
static int add_ ## _name ## _overflows (_type a, _type b) \
{\
    return (b > _max - a); \
}

#define __DEFINE_SIGNED_OVERFLOW(_name, _type, _min, _max) \
static int add_ ## _name ## _overflows (_type a, _type b) \
{\
    if ((a < 0) ^ (b < 0)) return 0; \
    if (a < 0) return -(a < _min - b); \
    return (a > _max - b); \
}

__DEFINE_UNSIGNED_OVERFLOW(uint, unsigned int, UINT_MAX)
__DEFINE_UNSIGNED_OVERFLOW(uint8, uint8_t, UINT8_MAX)
__DEFINE_UNSIGNED_OVERFLOW(uint16, uint16_t, UINT16_MAX)
__DEFINE_UNSIGNED_OVERFLOW(uint32, uint32_t, UINT32_MAX)
__DEFINE_UNSIGNED_OVERFLOW(uint64, uint64_t, UINT64_MAX)

__DEFINE_SIGNED_OVERFLOW(int, int, INT_MIN, INT_MAX)
__DEFINE_SIGNED_OVERFLOW(int8, int8_t, INT8_MIN, INT8_MAX)
__DEFINE_SIGNED_OVERFLOW(int16, int16_t, INT16_MIN, INT16_MAX)
__DEFINE_SIGNED_OVERFLOW(int32, int32_t, INT32_MIN, INT32_MAX)
__DEFINE_SIGNED_OVERFLOW(int64, int64_t, INT64_MIN, INT64_MAX)

#endif
