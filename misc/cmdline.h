/**
 * @file cmdline.h
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
 * Command line construction functions.
 */

#ifndef BADVPN_MISC_CMDLINE_H
#define BADVPN_MISC_CMDLINE_H

#include <stddef.h>

#include <misc/debug.h>
#include <misc/exparray.h>

typedef struct {
    struct ExpArray arr;
    size_t n;
} CmdLine;

static int CmdLine_Init (CmdLine *c);
static void CmdLine_Free (CmdLine *c);
static int CmdLine_Append (CmdLine *c, char *str);
static int CmdLine_Finish (CmdLine *c);
static char ** CmdLine_Get (CmdLine *c);

static int _CmdLine_finished (CmdLine *c)
{
    return (c->n > 0 && ((char **)c->arr.v)[c->n - 1] == NULL);
}

int CmdLine_Init (CmdLine *c)
{
    if (!ExpArray_init(&c->arr, sizeof(char *), 16)) {
        return 0;
    }
    
    c->n = 0;
    
    return 1;
}

void CmdLine_Free (CmdLine *c)
{
    for (size_t i = 0; i < c->n; i++) {
        free(((char **)c->arr.v)[i]);
    }
    
    free(c->arr.v);
}

int CmdLine_Append (CmdLine *c, char *str)
{
    ASSERT(str)
    ASSERT(!_CmdLine_finished(c))
    
    if (!ExpArray_resize(&c->arr, c->n + 1)) {
        return 0;
    }
    
    if (!(((char **)c->arr.v)[c->n] = strdup(str))) {
        return 0;
    }
    
    c->n++;
    
    return 1;
}

int CmdLine_Finish (CmdLine *c)
{
    ASSERT(!_CmdLine_finished(c))
    
    if (!ExpArray_resize(&c->arr, c->n + 1)) {
        return 0;
    }
    
    ((char **)c->arr.v)[c->n] = NULL;
    
    c->n++;
    
    return 1;
}

char ** CmdLine_Get (CmdLine *c)
{
    ASSERT(_CmdLine_finished(c))
    
    return (char **)c->arr.v;
}

#endif
