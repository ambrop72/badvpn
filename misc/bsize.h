/**
 * @file bsize.h
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
 * Arithmetic with overflow detection.
 */

#ifndef BADVPN_MISC_BSIZE_H
#define BADVPN_MISC_BSIZE_H

#include <stddef.h>
#include <limits.h>

typedef struct {
    int is_overflow;
    size_t value;
} bsize_t;

static bsize_t bsize_fromsize (size_t v);
static bsize_t bsize_fromint (int v);
static int bsize_tosize (bsize_t s, size_t *out);
static int bsize_toint (bsize_t s, int *out);
static bsize_t bsize_add (bsize_t s1, bsize_t s2);
static bsize_t bsize_max (bsize_t s1, bsize_t s2);

bsize_t bsize_fromsize (size_t v)
{
    bsize_t s;
    s.is_overflow = 0;
    s.value = v;
    return s;
}

bsize_t bsize_fromint (int v)
{
    bsize_t s;
    
    if (v < 0 || v > SIZE_MAX) {
        s.is_overflow = 1;
    } else {
        s.is_overflow = 0;
        s.value = v;
    }
    
    return s;
}

int bsize_tosize (bsize_t s, size_t *out)
{
    if (s.is_overflow) {
        return 0;
    }
    
    if (out) {
        *out = s.value;
    }
    
    return 1;
}

int bsize_toint (bsize_t s, int *out)
{
    if (s.is_overflow || s.value > INT_MAX) {
        return 0;
    }
    
    if (out) {
        *out = s.value;
    }
    
    return 1;
}

bsize_t bsize_add (bsize_t s1, bsize_t s2)
{
    bsize_t s;
    
    if (s1.is_overflow || s2.is_overflow || s2.value > SIZE_MAX - s1.value) {
        s.is_overflow = 1;
    } else {
        s.is_overflow = 0;
        s.value = s1.value + s2.value;
    }
    
    return s;
}

bsize_t bsize_max (bsize_t s1, bsize_t s2)
{
    bsize_t s;
    
    if (s1.is_overflow || s2.is_overflow) {
        s.is_overflow = 1;
    } else {
        s.is_overflow = 0;
        s.value = s1.value;
        if (s.value < s2.value) {
            s.value = s2.value;
        }
    }
    
    return s;
}

#endif
