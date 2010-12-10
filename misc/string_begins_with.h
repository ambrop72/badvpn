/**
 * @file string_begins_with.h
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
 * Function for checking if a string begins with a given string.
 */

#ifndef BADVPN_MISC_STRING_BEGINS_WITH
#define BADVPN_MISC_STRING_BEGINS_WITH

#include <stddef.h>
#include <string.h>

#include <misc/debug.h>

static size_t data_begins_with (const char *str, size_t str_len, const char *needle)
{
    ASSERT(strlen(needle) > 0)
    
    size_t len = 0;
    
    while (str_len > 0 && *needle) {
        if (*str != *needle) {
            return 0;
        }
        str++;
        str_len--;
        needle++;
        len++;
    }
    
    if (*needle) {
        return 0;
    }
    
    return len;
}

static size_t string_begins_with (const char *str, const char *needle)
{
    ASSERT(strlen(needle) > 0)
    
    return data_begins_with(str, strlen(str), needle);
}

#endif
