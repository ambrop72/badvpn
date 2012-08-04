/**
 * @file BStringTrie.h
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

#ifndef BADVPN_BSTRINGTRIE_H
#define BADVPN_BSTRINGTRIE_H

#include <stddef.h>
#include <limits.h>
#include <string.h>

#include <misc/debug.h>
#include <misc/balloc.h>

#define BSTRINGTRIE_DEFAULT_VALUE ((int)-1)

#define BSTRINGTRIE_DEGREE ((((size_t)1) << CHAR_BIT) - 1)

struct BStringTrie__node {
    int value_links[1 + BSTRINGTRIE_DEGREE];
};

typedef struct {
    struct BStringTrie__node *arr;
    size_t count;
    size_t capacity;
} BStringTrie;

static int BStringTrie_Init (BStringTrie *o) WARN_UNUSED;
static void BStringTrie_Free (BStringTrie *o);
static int BStringTrie_Set (BStringTrie *o, const char *key, int value) WARN_UNUSED;
static int BStringTrie_Lookup (const BStringTrie *o, const char *key);

// implementation follows

static int BStringTrie__new_node (BStringTrie *o, int *out_nodeidx)
{
    ASSERT(o->count <= o->capacity)
    ASSERT(o->capacity > 0)
    ASSERT(out_nodeidx)
    
    if (o->count == o->capacity) {
        if (o->capacity > SIZE_MAX / 2) {
            return 0;
        }
        size_t newcap = 2 * o->capacity;
        
        struct BStringTrie__node *newarr = BAllocArray(newcap, sizeof(newarr[0]));
        if (!newarr) {
            return 0;
        }
        
        memcpy(newarr, o->arr, o->count * sizeof(newarr[0]));
        BFree(o->arr);
        
        o->arr = newarr;
        o->capacity = newcap;
    }
    
    struct BStringTrie__node *node = &o->arr[o->count];
    
    node->value_links[0] = BSTRINGTRIE_DEFAULT_VALUE;
    
    for (size_t i = 0; i < BSTRINGTRIE_DEGREE; i++) {
        node->value_links[1 + i] = -1;
    }
    
    *out_nodeidx = o->count;
    o->count++;
    
    return 1;
}

static int BStringTrie_Init (BStringTrie *o)
{
    o->count = 0;
    o->capacity = 1;
    
    if (!(o->arr = BAllocArray(o->capacity, sizeof(o->arr[0])))) {
        return 0;
    }
    
    int idx0;
    BStringTrie__new_node(o, &idx0);
    
    return 1;
}

static void BStringTrie_Free (BStringTrie *o)
{
    BFree(o->arr);
}

static int BStringTrie_Set (BStringTrie *o, const char *key, int value)
{
    ASSERT(key)
    
    const unsigned char *ukey = (const unsigned char *)key;
    int nodeidx = 0;
    
    while (*ukey) {
        ASSERT(o->arr[nodeidx].value_links[*ukey] >= -1)
        
        int new_nodeidx;
        
        if (o->arr[nodeidx].value_links[*ukey] >= 0) {
            new_nodeidx = o->arr[nodeidx].value_links[*ukey];
        } else {
            if (!BStringTrie__new_node(o, &new_nodeidx)) {
                return 0;
            }
            o->arr[nodeidx].value_links[*ukey] = new_nodeidx;
        }
        
        ASSERT(new_nodeidx >= 0)
        ASSERT(new_nodeidx < o->count)
        
        nodeidx = new_nodeidx;
        ukey++;
    }
    
    o->arr[nodeidx].value_links[0] = value;
    
    return 1;
}

static int BStringTrie_Lookup (const BStringTrie *o, const char *key)
{
    ASSERT(key)
    
    const unsigned char *ukey = (const unsigned char *)key;
    int nodeidx = 0;
    
    while (*ukey) {
        nodeidx = o->arr[nodeidx].value_links[*ukey];
        ASSERT(nodeidx >= -1)
        if (nodeidx < 0) {
            return -1;
        }
        ASSERT(nodeidx < o->count)
        ukey++;
    }
    
    return o->arr[nodeidx].value_links[0];
}

#endif
