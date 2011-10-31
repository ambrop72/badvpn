/**
 * @file parse_number.h
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
