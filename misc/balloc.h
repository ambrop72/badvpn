/**
 * @file balloc.h
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
 * Memory allocation functions.
 */

#ifndef BADVPN_MISC_BALLOC_H
#define BADVPN_MISC_BALLOC_H

#include <stddef.h>
#include <inttypes.h>
#include <stdlib.h>

#include <misc/debug.h>
#include <misc/bsize.h>

/**
 * Allocates memory.
 * 
 * @param bytes number of bytes to allocate.
 * @return a non-NULL pointer to the memory, or NULL on failure.
 *         The memory allocated can be freed using {@link BFree}.
 */
static void * BAlloc (size_t bytes);

/**
 * Frees memory.
 * 
 * @param m memory to free. Must have been obtained with {@link BAlloc},
 *          {@link BAllocArray}, or {@link BAllocArray2}. May be NULL;
 *          in this case, this function does nothing.
 */
static void BFree (void *m);

/**
 * Allocates memory, with size given as a {@link bsize_t}.
 * 
 * @param bytes number of bytes to allocate. If the size is overflow,
 *              this function will return NULL.
 * @return a non-NULL pointer to the memory, or NULL on failure.
 *         The memory allocated can be freed using {@link BFree}.
 */
static void * BAllocSize (bsize_t bytes);

/**
 * Allocates memory for an array.
 * A check is first done to make sure the multiplication doesn't overflow;
 * otherwise, this is equivalent to {@link BAlloc}(count * bytes).
 * 
 * @param count number of elements.
 * @param bytes size of one array element.
 * @return a non-NULL pointer to the memory, or NULL on failure.
 *         The memory allocated can be freed using {@link BFree}.
 */
static void * BAllocArray (size_t count, size_t bytes);

/**
 * Allocates memory for a two-dimensional array.
 * 
 * Checks are first done to make sure the multiplications don't overflow;
 * otherwise, this is equivalent to {@link BAlloc}((count2 * (count1 * bytes)).
 * 
 * @param count2 number of elements in one dimension.
 * @param count1 number of elements in the other dimension.
 * @param bytes size of one array element.
 * @return a non-NULL pointer to the memory, or NULL on failure.
 *         The memory allocated can be freed using {@link BFree}.
 */
static void * BAllocArray2 (size_t count2, size_t count1, size_t bytes);

void * BAlloc (size_t bytes)
{
    if (bytes == 0) {
        return malloc(1);
    }
    
    return malloc(bytes);
}

void BFree (void *m)
{
    free(m);
}

void * BAllocSize (bsize_t bytes)
{
    if (bytes.is_overflow) {
        return NULL;
    }
    
    return BAlloc(bytes.value);
}

void * BAllocArray (size_t count, size_t bytes)
{
    if (count == 0 || bytes == 0) {
        return malloc(1);
    }
    
    if (count > SIZE_MAX / bytes) {
        return NULL;
    }
    
    return BAlloc(count * bytes);
}

void * BAllocArray2 (size_t count2, size_t count1, size_t bytes)
{
    if (count2 == 0 || count1 == 0 || bytes == 0) {
        return malloc(1);
    }
    
    if (count1 > SIZE_MAX / bytes) {
        return NULL;
    }
    
    if (count2 > SIZE_MAX / (count1 * bytes)) {
        return NULL;
    }
    
    return BAlloc(count2 * (count1 * bytes));
}

#endif
