/**
 * @file parse_number.h
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * @section DESCRIPTION
 * 
 * Numeric string parsing.
 */

#ifndef BADVPN_MISC_PARSE_NUMBER_H
#define BADVPN_MISC_PARSE_NUMBER_H

#include <inttypes.h>
#include <string.h>
#include <stddef.h>

#include <misc/debug.h>

static int parse_unsigned_integer_bin (const char *str, size_t str_len, uintmax_t *out) WARN_UNUSED;
static int parse_unsigned_integer (const char *str, uintmax_t *out) WARN_UNUSED;
static int parse_unsigned_hex_integer_bin (const char *str, size_t str_len, uintmax_t *out) WARN_UNUSED;
static int parse_unsigned_hex_integer (const char *str, uintmax_t *out) WARN_UNUSED;

static int decode_hex_digit (char c)
{
    switch (c) {
        case '0': return 0;
        case '1': return 1;
        case '2': return 2;
        case '3': return 3;
        case '4': return 4;
        case '5': return 5;
        case '6': return 6;
        case '7': return 7;
        case '8': return 8;
        case '9': return 9;
        case 'A': case 'a': return 10;
        case 'B': case 'b': return 11;
        case 'C': case 'c': return 12;
        case 'D': case 'd': return 13;
        case 'E': case 'e': return 14;
        case 'F': case 'f': return 15;
    }
    
    return -1;
}

int parse_unsigned_integer_bin (const char *str, size_t str_len, uintmax_t *out)
{
    uintmax_t n = 0;
    
    if (str_len == 0) {
        return 0;
    }
    
    while (str_len > 0) {
        if (*str < '0' || *str > '9') {
            return 0;
        }
        int digit = *str - '0';
        
        if (n > UINTMAX_MAX / 10) {
            return 0;
        }
        n *= 10;
        
        if (digit > UINTMAX_MAX - n) {
            return 0;
        }
        n += digit;
        
        str++;
        str_len--;
    }
    
    *out = n;
    return 1;
}

int parse_unsigned_integer (const char *str, uintmax_t *out)
{
    return parse_unsigned_integer_bin(str, strlen(str), out);
}

int parse_unsigned_hex_integer_bin (const char *str, size_t str_len, uintmax_t *out)
{
    uintmax_t n = 0;
    
    if (str_len == 0) {
        return 0;
    }
    
    while (str_len > 0) {
        int digit = decode_hex_digit(*str);
        if (digit < 0) {
            return 0;
        }
        
        if (n > UINTMAX_MAX / 16) {
            return 0;
        }
        n *= 16;
        
        if (digit > UINTMAX_MAX - n) {
            return 0;
        }
        n += digit;
        
        str++;
        str_len--;
    }
    
    *out = n;
    return 1;
}

int parse_unsigned_hex_integer (const char *str, uintmax_t *out)
{
    return parse_unsigned_hex_integer_bin(str, strlen(str), out);
}

#endif
