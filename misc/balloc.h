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
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

#include <misc/debug.h>
#include <misc/bsize.h>
#include <misc/maxalign.h>

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
 * This may be slightly faster if 'bytes' is constant, because a division
 * with 'bytes' is performed.
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

/**
 * Allocates memory for two arrays as a continuous block.
 * Unused bytes are inserted to make the second array aligned for any kind
 * of object.
 * The 'count' and 'bytes' arguments are interchangeable; for performance
 * reasons, try to have the 'bytes' arguments constant.
 * 
 * @param count1 number of elements for first array
 * @param bytes1 size of element for first array
 * @param count2 number of elements for second array
 * @param bytes2 size of element for second array
 * @param out2 *out2 will receive the pointer to the second array, if successful.
 *             Must not be NULL.
 * @return on success, returns the pointer to the first array and sets *out2;
 *         the memory can be freed by calling {@link BFree} on the returned pointer.
 *         On failure, returns NULL.
 */
static void * BAllocTwoArrays (size_t count1, size_t bytes1, size_t count2, size_t bytes2, void **out2);

/**
 * Allocates memory for three arrays as a continuous block.
 * Unused bytes are inserted to make the second and third arrays aligned for any
 * kind of object.
 * The 'count' and 'bytes' arguments are interchangeable; for performance
 * reasons, try to have the 'bytes' arguments constant.
 * 
 * @param count1 number of elements for first array
 * @param bytes1 size of element for first array
 * @param count2 number of elements for second array
 * @param bytes2 size of element for second array
 * @param count3 number of elements for third array
 * @param bytes3 size of element for third array
 * @param out2 *out2 will receive the pointer to the second array, if successful.
 *             Must not be NULL.
 * @param out3 *out3 will receive the pointer to the third array, if successful.
 *             Must not be NULL.
 * @return on success, returns the pointer to the first array and sets *out2 and
 *         *out3; the memory can be freed by calling {@link BFree} on the returned
 *         pointer. On failure, returns NULL.
 */
static void * BAllocThreeArrays (size_t count1, size_t bytes1, size_t count2, size_t bytes2, size_t count3, size_t bytes3, void **out2, void **out3);

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


void * BAllocTwoArrays (size_t count1, size_t bytes1, size_t count2, size_t bytes2, void **out2)
{
    ASSERT(out2)
    
    // first array
    if (bytes1 > 0 && count1 > SIZE_MAX / bytes1) {
        return NULL;
    }
    size_t s = count1 * bytes1;
    
    // alignment for second array
    size_t mod = s % BMAX_ALIGN;
    if (mod > 0) {
        if (BMAX_ALIGN - mod > SIZE_MAX - s) {
            return NULL;
        }
        s += BMAX_ALIGN - mod;
    }
    
    // second array
    size_t pos2 = s;
    if (bytes2 > 0 && count2 > (SIZE_MAX - s) / bytes2) {
        return NULL;
    }
    s += count2 * bytes2;
    
    void *arr = BAlloc(s);
    
    if (arr) {
        *out2 = (char *)arr + pos2;
    }
    
    return arr;
}

void * BAllocThreeArrays (size_t count1, size_t bytes1, size_t count2, size_t bytes2, size_t count3, size_t bytes3, void **out2, void **out3)
{
    ASSERT(out2)
    ASSERT(out2)
    
    // first array
    if (bytes1 > 0 && count1 > SIZE_MAX / bytes1) {
        return NULL;
    }
    size_t s = count1 * bytes1;
    
    // alignment for second array
    size_t mod = s % BMAX_ALIGN;
    if (mod > 0) {
        if (BMAX_ALIGN - mod > SIZE_MAX - s) {
            return NULL;
        }
        s += BMAX_ALIGN - mod;
    }
    
    // second array
    size_t pos2 = s;
    if (bytes2 > 0 && count2 > (SIZE_MAX - s) / bytes2) {
        return NULL;
    }
    s += count2 * bytes2;
    
    // alignment for third array
    mod = s % BMAX_ALIGN;
    if (mod > 0) {
        if (BMAX_ALIGN - mod > SIZE_MAX - s) {
            return NULL;
        }
        s += BMAX_ALIGN - mod;
    }
    
    // third array
    size_t pos3 = s;
    if (bytes3 > 0 && count3 > (SIZE_MAX - s) / bytes3) {
        return NULL;
    }
    s += count3 * bytes3;
    
    void *arr = BAlloc(s);
    
    if (arr) {
        *out2 = (char *)arr + pos2;
        *out3 = (char *)arr + pos3;
    }
    
    return arr;
}

#endif
