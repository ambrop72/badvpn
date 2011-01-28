/**
 * @file minmax.h
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
 * Minimum and maximum macros.
 */

#ifndef BADVPN_MISC_MINMAX_H
#define BADVPN_MISC_MINMAX_H

#include <stddef.h>
#include <stdint.h>

#define DEFINE_BMINMAX(name, type) \
static type bmin ## name (type a, type b) { return (a < b ? a : b); } \
static type bmax ## name (type a, type b) { return (a > b ? a : b); }

DEFINE_BMINMAX(_size, size_t)
DEFINE_BMINMAX(_int, int)
DEFINE_BMINMAX(_int8, int8_t)
DEFINE_BMINMAX(_int16, int16_t)
DEFINE_BMINMAX(_int32, int32_t)
DEFINE_BMINMAX(_int64, int64_t)
DEFINE_BMINMAX(_uint, unsigned int)
DEFINE_BMINMAX(_uint8, uint8_t)
DEFINE_BMINMAX(_uint16, uint16_t)
DEFINE_BMINMAX(_uint32, uint32_t)
DEFINE_BMINMAX(_uint64, uint64_t)

#endif
