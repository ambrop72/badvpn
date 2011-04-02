/**
 * @file concat_strings.h
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
 * Function for concatenating strings.
 */

#ifndef BADVPN_MISC_CONCAT_STRINGS_H
#define BADVPN_MISC_CONCAT_STRINGS_H

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include <misc/debug.h>

static char * concat_strings (int num, ...)
{
    ASSERT(num >= 0)
    
    // calculate sum of lengths
    size_t sum = 0;
    va_list ap;
    va_start(ap, num);
    for (int i = 0; i < num; i++) {
        const char *str = va_arg(ap, const char *);
        size_t str_len = strlen(str);
        if (str_len > SIZE_MAX - 1 - sum) {
            return NULL;
        }
        sum += str_len;
    }
    va_end(ap);
    
    // allocate memory
    char *res_str = malloc(sum + 1);
    if (!res_str) {
        return NULL;
    }
    
    // copy strings
    sum = 0;
    va_start(ap, num);
    for (int i = 0; i < num; i++) {
        const char *str = va_arg(ap, const char *);
        size_t str_len = strlen(str);
        memcpy(res_str + sum, str, str_len);
        sum += str_len;
    }
    va_end(ap);
    
    // terminate
    res_str[sum] = '\0';
    
    return res_str;
}

#endif
