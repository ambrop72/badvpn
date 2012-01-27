/**
 * @file balloc.h
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
