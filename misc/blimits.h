/**
 * @file blimits.h
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

#ifndef BADVPN_BLIMITS_H
#define BADVPN_BLIMITS_H

#include <stdint.h>

#define BTYPE_IS_SIGNED(type) ((type)-1 < 0)

#define BSIGNED_TYPE_MIN(type) ( \
    sizeof(type) == 1 ? INT8_MIN : ( \
    sizeof(type) == 2 ? INT16_MIN : ( \
    sizeof(type) == 4 ? INT32_MIN : ( \
    sizeof(type) == 8 ? INT64_MIN : 0))))

#define BSIGNED_TYPE_MAX(type) ( \
    sizeof(type) == 1 ? INT8_MAX : ( \
    sizeof(type) == 2 ? INT16_MAX : ( \
    sizeof(type) == 4 ? INT32_MAX : ( \
    sizeof(type) == 8 ? INT64_MAX : 0))))

#define BUNSIGNED_TYPE_MIN(type) ((type)0)

#define BUNSIGNED_TYPE_MAX(type) ( \
    sizeof(type) == 1 ? UINT8_MAX : ( \
    sizeof(type) == 2 ? UINT16_MAX : ( \
    sizeof(type) == 4 ? UINT32_MAX : ( \
    sizeof(type) == 8 ? UINT64_MAX : 0))))

#define BTYPE_MIN(type) (BTYPE_IS_SIGNED(type) ? BSIGNED_TYPE_MIN(type) : BUNSIGNED_TYPE_MIN(type))
#define BTYPE_MAX(type) (BTYPE_IS_SIGNED(type) ? BSIGNED_TYPE_MAX(type) : BUNSIGNED_TYPE_MAX(type))

#endif
