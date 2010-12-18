/**
 * @file exparray.h
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
 * Dynamic array which grows exponentionally on demand.
 */

#ifndef BADVPN_MISC_EXPARRAY_H
#define BADVPN_MISC_EXPARRAY_H

#include <stddef.h>
#include <stdlib.h>
#include <limits.h>

#include <misc/debug.h>

struct ExpArray {
    size_t esize;
    size_t size;
    void *v;
};

static int ExpArray_init (struct ExpArray *o, size_t esize, size_t size)
{
    ASSERT(esize > 0)
    ASSERT(size > 0)
    
    o->esize = esize;
    o->size = size;
    
    if (o->size > SIZE_MAX / o->esize) {
        return 0;
    }
    
    if (!(o->v = malloc(o->size * o->esize))) {
        return 0;
    }
    
    return 1;
}

static int ExpArray_resize (struct ExpArray *o, size_t size)
{
    ASSERT(size > 0)
    
    if (size <= o->size) {
        return 1;
    }
    
    size_t newsize = o->size;
    
    while (newsize < size) {
        if (2 > SIZE_MAX / newsize) {
            return 0;
        }
        
        newsize = 2 * newsize;
    }
    
    void *newarr = realloc(o->v, newsize * o->esize);
    if (!newarr) {
        return 0;
    }
    
    o->size = newsize;
    o->v = newarr;
    
    return 1;
}

#endif
