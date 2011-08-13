/**
 * @file Utf8Encoder.h
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

#ifndef BADVPN_UTF8ENCODER_H
#define BADVPN_UTF8ENCODER_H

#include <stdint.h>

/**
 * Encodes a Unicode character into a sequence of bytes according to UTF-8.
 * 
 * @param ch Unicode character to encode
 * @param out will receive the encoded bytes. Must have space for 4 bytes.
 * @return number of bytes written, 0-4, with 0 meaning the character cannot
 *         be encoded
 */
static int Utf8Encoder_EncodeCharacter (uint32_t ch, uint8_t *out);

int Utf8Encoder_EncodeCharacter (uint32_t ch, uint8_t *out)
{
    if (ch <= UINT32_C(0x007F)) {
        out[0] = ch;
        return 1;
    }
    
    if (ch <= UINT32_C(0x07FF)) {
        out[0] = (0xC0 | (ch >> 6));
        out[1] = (0x80 | ((ch >> 0) & 0x3F));
        return 2;
    }
    
    if (ch <= UINT32_C(0xFFFF)) {
        // surrogates
        if (ch >= UINT32_C(0xD800) && ch <= UINT32_C(0xDFFF)) {
            return 0;
        }
        
        out[0] = (0xE0 | (ch >> 12));
        out[1] = (0x80 | ((ch >> 6) & 0x3F));
        out[2] = (0x80 | ((ch >> 0) & 0x3F));
        return 3;
    }
    
    if (ch < UINT32_C(0x10FFFF)) {
        out[0] = (0xF0 | (ch >> 18));
        out[1] = (0x80 | ((ch >> 12) & 0x3F));
        out[2] = (0x80 | ((ch >> 6) & 0x3F));
        out[3] = (0x80 | ((ch >> 0) & 0x3F));
        return 4;
    }
    
    return 0;
}

#endif
