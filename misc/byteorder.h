/**
 * @file byteorder.h
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
 * Byte order conversion functions.
 * 
 * hton* functions convert from host to big-endian (network) byte order.
 * htol* functions convert from host to little-endian byte order.
 * ntoh* functions convert from big-endian (network) to host byte order.
 * ltoh* functions convert from little-endian to host byte order.
 */

#ifndef BADVPN_MISC_BYTEORDER_H
#define BADVPN_MISC_BYTEORDER_H

#include <stdint.h>

static uint16_t badvpn_reverse16 (uint16_t x)
{
    uint16_t y;
    *((uint8_t *)&y+0) = *((uint8_t *)&x+1);
    *((uint8_t *)&y+1) = *((uint8_t *)&x+0);
    return y;
}

static uint32_t badvpn_reverse32 (uint32_t x)
{
    uint32_t y;
    *((uint8_t *)&y+0) = *((uint8_t *)&x+3);
    *((uint8_t *)&y+1) = *((uint8_t *)&x+2);
    *((uint8_t *)&y+2) = *((uint8_t *)&x+1);
    *((uint8_t *)&y+3) = *((uint8_t *)&x+0);
    return y;
}

static uint64_t badvpn_reverse64 (uint64_t x)
{
    uint64_t y;
    *((uint8_t *)&y+0) = *((uint8_t *)&x+7);
    *((uint8_t *)&y+1) = *((uint8_t *)&x+6);
    *((uint8_t *)&y+2) = *((uint8_t *)&x+5);
    *((uint8_t *)&y+3) = *((uint8_t *)&x+4);
    *((uint8_t *)&y+4) = *((uint8_t *)&x+3);
    *((uint8_t *)&y+5) = *((uint8_t *)&x+2);
    *((uint8_t *)&y+6) = *((uint8_t *)&x+1);
    *((uint8_t *)&y+7) = *((uint8_t *)&x+0);
    return y;
}

static uint8_t hton8 (uint8_t x)
{
    return x;
}

static uint8_t htol8 (uint8_t x)
{
    return x;
}

#if defined(BADVPN_LITTLE_ENDIAN)

static uint16_t hton16 (uint16_t x)
{
    return badvpn_reverse16(x);
}

static uint32_t hton32 (uint32_t x)
{
    return badvpn_reverse32(x);
}

static uint64_t hton64 (uint64_t x)
{
    return badvpn_reverse64(x);
}

static uint16_t htol16 (uint16_t x)
{
    return x;
}

static uint32_t htol32 (uint32_t x)
{
    return x;
}

static uint64_t htol64 (uint64_t x)
{
    return x;
}

#elif defined(BADVPN_BIG_ENDIAN)

static uint16_t hton16 (uint16_t x)
{
    return x;
}

static uint32_t hton32 (uint32_t x)
{
    return x;
}

static uint64_t hton64 (uint64_t x)
{
    return x;
}

static uint16_t htol16 (uint16_t x)
{
    return badvpn_reverse16(x);
}

static uint32_t htol32 (uint32_t x)
{
    return badvpn_reverse32(x);
}

static uint64_t htol64 (uint64_t x)
{
    return badvpn_reverse64(x);
}

#endif

static uint8_t ntoh8 (uint8_t x)
{
    return hton8(x);
}

static uint16_t ntoh16 (uint16_t x)
{
    return hton16(x);
}

static uint32_t ntoh32 (uint32_t x)
{
    return hton32(x);
}

static uint64_t ntoh64 (uint64_t x)
{
    return hton64(x);
}

static uint8_t ltoh8 (uint8_t x)
{
    return htol8(x);
}

static uint16_t ltoh16 (uint16_t x)
{
    return htol16(x);
}

static uint32_t ltoh32 (uint32_t x)
{
    return htol32(x);
}

static uint64_t ltoh64 (uint64_t x)
{
    return htol64(x);
}

#endif
