/**
 * @file Utf8Decoder.h
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

#ifndef BADVPN_UTF8DECODER_H
#define BADVPN_UTF8DECODER_H

#include <stdint.h>

#include <misc/debug.h>

/**
 * Decodes UTF-8 data into Unicode characters.
 */
typedef struct {
    int bytes;
    int pos;
    uint32_t ch;
} Utf8Decoder;

/**
 * Initializes the UTF-8 decoder.
 * 
 * @param o the object
 */
static void Utf8Decoder_Init (Utf8Decoder *o);

/**
 * Inputs a byte to the decoder.
 * 
 * @param o the object
 * @param b byte to input
 * @param out_ch will receive a Unicode character if this function returns 1.
 *               If written, the character will be in the range 0 - 0x10FFFF,
 *               excluding the surrogate range 0xD800 - 0xDFFF.
 * @return 1 if a Unicode character has been written to *out_ch, 0 if not
 */
static int Utf8Decoder_Input (Utf8Decoder *o, uint8_t b, uint32_t *out_ch);

void Utf8Decoder_Init (Utf8Decoder *o)
{
    o->bytes = 0;
}

int Utf8Decoder_Input (Utf8Decoder *o, uint8_t b, uint32_t *out_ch)
{
    // one-byte character
    if ((b & 128) == 0) {
        o->bytes = 0;
        *out_ch = b;
        return 1;
    }
    
    // start of two-byte character
    if ((b & 224) == 192) {
        o->bytes = 2;
        o->pos = 1;
        o->ch = (uint32_t)(b & 31) << 6;
        return 0;
    }
    
    // start of three-byte character
    if ((b & 240) == 224) {
        o->bytes = 3;
        o->pos = 1;
        o->ch = (uint32_t)(b & 15) << 12;
        return 0;
    }
    
    // start of four-byte character
    if ((b & 248) == 240) {
        o->bytes = 4;
        o->pos = 1;
        o->ch = (uint32_t)(b & 7) << 18;
        return 0;
    }
    
    // continuation of multi-byte character
    if ((b & 192) == 128 && o->bytes > 0) {
        ASSERT(o->bytes <= 4)
        ASSERT(o->pos > 0)
        ASSERT(o->pos < o->bytes)
        
        // add bits from this byte
        o->ch |= (uint32_t)(b & 63) << (6 * (o->bytes - o->pos - 1));
        
        // end of multi-byte character?
        if (o->pos == o->bytes - 1) {
            // reset state
            o->bytes = 0;
            
            // don't report out-of-range characters
            if (o->ch > UINT32_C(0x10FFFF)) {
                return 0;
            }
            
            // don't report surrogates
            if (o->ch >= UINT32_C(0xD800) && o->ch <= UINT32_C(0xDFFF)) {
                return 0;
            }
            
            *out_ch = o->ch;
            return 1;
        }
        
        // increment byte index
        o->pos++;
        
        return 0;
    }
    
    // error, reset state
    o->bytes = 0;
    
    return 0;
}

#endif
