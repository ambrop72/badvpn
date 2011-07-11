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

static int parse_unsigned_integer_bin (const char *str, size_t str_len, uintmax_t *out);
static int parse_unsigned_integer (const char *str, uintmax_t *out);

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

#endif
