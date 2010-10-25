/**
 * @file LexMemoryBufferInput.h
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
 * Object that can be used by a lexer to read input from a memory buffer.
 */

#ifndef BADVPN_PREDICATE_LEXMEMORYBUFFERINPUT_H
#define BADVPN_PREDICATE_LEXMEMORYBUFFERINPUT_H

#include <string.h>

#include <misc/debug.h>

typedef struct {
    char *buf;
    int len;
    int pos;
    int error;
} LexMemoryBufferInput;

static void LexMemoryBufferInput_Init (LexMemoryBufferInput *input, char *buf, int len)
{
    input->buf = buf;
    input->len = len;
    input->pos = 0;
    input->error = 0;
}

static int LexMemoryBufferInput_Read (LexMemoryBufferInput *input, char *dest, int len)
{
    ASSERT(dest)
    ASSERT(len > 0)
    
    if (input->pos >= input->len) {
        return 0;
    }
    
    int to_read = input->len - input->pos;
    if (to_read > len) {
        to_read = len;
    }
    
    memcpy(dest, input->buf + input->pos, to_read);
    input->pos += to_read;
    
    return to_read;
}

static void LexMemoryBufferInput_SetError (LexMemoryBufferInput *input)
{
    input->error = 1;
}

static int LexMemoryBufferInput_HasError (LexMemoryBufferInput *input)
{
    return input->error;
}

#endif
