/**
 * @file Vector.h
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
 */

// Preprocessor inputs:
// VECTOR_NAME - prefix of functions and types to be defined
// VECTOR_ELEM_TYPE - type of vector elements

#include <limits.h>
#include <string.h>
#include <stddef.h>

#include <misc/debug.h>
#include <misc/balloc.h>
#include <misc/merge.h>

#define Vector VECTOR_NAME
#define VectorElem VECTOR_ELEM_TYPE
#define Vector_Init MERGE(VECTOR_NAME, _Init)
#define Vector_Free MERGE(VECTOR_NAME, _Free)
#define Vector_Get MERGE(VECTOR_NAME, _Get)
#define Vector_AllocAppend MERGE(VECTOR_NAME, _AllocAppend)
#define Vector_DoAppend MERGE(VECTOR_NAME, _DoAppend)
#define Vector_AppendValue MERGE(VECTOR_NAME, _AppendValue)

typedef struct {
    VectorElem *elems;
    size_t capacity;
    size_t count;
} Vector;

static int Vector_Init (Vector *o, size_t capacity) WARN_UNUSED;
static void Vector_Free (Vector *o);
static VectorElem * Vector_Get (Vector *o, size_t index);
static int Vector_AllocAppend (Vector *o, size_t count, VectorElem **out_ptr) WARN_UNUSED;
static void Vector_DoAppend (Vector *o, size_t count);
static int Vector_AppendValue (Vector *o, VectorElem value, size_t *out_index) WARN_UNUSED;

static int Vector_Init (Vector *o, size_t capacity)
{
    if (capacity == 0) {
        o->elems = NULL;
    } else {
        o->elems = BAllocArray(capacity, sizeof(VectorElem));
        if (!o->elems) {
            return 0;
        }
    }
    o->capacity = capacity;
    o->count = 0;
    return 1;
}

static void Vector_Free (Vector *o)
{
    BFree(o->elems);
}

static VectorElem * Vector_Get (Vector *o, size_t index)
{
    ASSERT(index < o->count)
    
    return &o->elems[index];
}

static int Vector_AllocAppend (Vector *o, size_t count, VectorElem **out_ptr)
{
    ASSERT(count > 0)
    
    if (count > o->capacity - o->count) {
        size_t new_capacity = o->capacity;
        do {
            if (new_capacity > SIZE_MAX / 2) {
                return 0;
            }
            new_capacity = (new_capacity == 0) ? 1 : (2 * new_capacity);
        } while (count > new_capacity - o->count);
        
        VectorElem *new_elems = BAllocArray(new_capacity, sizeof(VectorElem));
        if (!new_elems) {
            return 0;
        }
        
        if (o->count > 0) {
            memcpy(new_elems, o->elems, o->count * sizeof(VectorElem));
        }
        
        BFree(o->elems);
        
        o->elems = new_elems;
        o->capacity = new_capacity;
    }
    
    if (out_ptr) {
        *out_ptr = &o->elems[o->count];
    }
    return 1;
}

static void Vector_DoAppend (Vector *o, size_t count)
{
    ASSERT(count <= o->capacity - o->count)
    
    o->count += count;
}

static int Vector_AppendValue (Vector *o, VectorElem value, size_t *out_index)
{
    VectorElem *ptr;
    if (!Vector_AllocAppend(o, 1, &ptr)) {
        return 0;
    }
    *ptr = value;
    if (out_index) {
        *out_index = o->count;
    }
    Vector_DoAppend(o, 1);
    return 1;
}

#undef VECTOR_NAME
#undef VECTOR_ELEM_TYPE

#undef Vector
#undef VectorElem
#undef Vector_Init
#undef Vector_Free
#undef Vector_Get
#undef Vector_AllocAppend
#undef Vector_DoAppend
#undef Vector_AppendValue
