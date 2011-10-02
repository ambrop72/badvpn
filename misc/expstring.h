/**
 * @file expstring.h
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

#ifndef BADVPN_MISC_EXPSTRING_H
#define BADVPN_MISC_EXPSTRING_H

#include <stddef.h>

#include <misc/debug.h>
#include <misc/exparray.h>
#include <misc/bsize.h>

typedef struct {
    struct ExpArray arr;
    size_t n;
} ExpString;

static int ExpString_Init (ExpString *c);
static void ExpString_Free (ExpString *c);
static int ExpString_Append (ExpString *c, const char *str);
static char * ExpString_Get (ExpString *c);

int ExpString_Init (ExpString *c)
{
    if (!ExpArray_init(&c->arr, 1, 16)) {
        return 0;
    }
    
    c->n = 0;
    ((char *)c->arr.v)[c->n] = '\0';
    
    return 1;
}

void ExpString_Free (ExpString *c)
{
    free(c->arr.v);
}

int ExpString_Append (ExpString *c, const char *str)
{
    ASSERT(str)
    
    size_t l = strlen(str);
    bsize_t newsize = bsize_add(bsize_fromsize(c->n), bsize_add(bsize_fromsize(l), bsize_fromint(1)));
    
    if (newsize.is_overflow || !ExpArray_resize(&c->arr, newsize.value)) {
        return 0;
    }
    
    memcpy((char *)c->arr.v + c->n, str, l);
    c->n += l;
    ((char *)c->arr.v)[c->n] = '\0';
    
    return 1;
}

char * ExpString_Get (ExpString *c)
{
    return (char *)c->arr.v;
}

#endif
