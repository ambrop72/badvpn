/**
 * @file Utf16Encoder.h
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

#ifndef BADVPN_UTF16ENCODER_H
#define BADVPN_UTF16ENCODER_H

#include <stdint.h>

/**
 * Encodes a Unicode character into a sequence of 16-bit values according to UTF-16.
 * 
 * @param ch Unicode character to encode
 * @param out will receive the encoded 16-bit values. Must have space for 2 values.
 * @return number of 16-bit values written, 0-2, with 0 meaning the character cannot
 *         be encoded
 */
static int Utf16Encoder_EncodeCharacter (uint32_t ch, uint16_t *out);

int Utf16Encoder_EncodeCharacter (uint32_t ch, uint16_t *out)
{
    if (ch <= UINT32_C(0xFFFF)) {
        // surrogates
        if (ch >= UINT32_C(0xD800) && ch <= UINT32_C(0xDFFF)) {
            return 0;
        }
        
        out[0] = ch;
        return 1;
    }
    
    if (ch <= UINT32_C(0x10FFFF)) {
        uint32_t x = ch - UINT32_C(0x10000);
        out[0] = UINT32_C(0xD800) + (x >> 10);
        out[1] = UINT32_C(0xDC00) + (x & UINT32_C(0x3FF));
        return 2;
    }
    
    return 0;
}

#endif
