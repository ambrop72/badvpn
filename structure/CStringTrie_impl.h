/**
 * @file CStringTrie_impl.h
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

#include "CStringTrie_header.h"

static int CStringTrie__new_node (CStringTrie *o, int *out_nodeidx)
{
    ASSERT(o->count <= o->capacity)
    ASSERT(o->capacity > 0)
    ASSERT(o->count >= 0)
    ASSERT(out_nodeidx)
    
    if (o->count == o->capacity) {
        if (o->capacity > INT_MAX / 2 || o->capacity > SIZE_MAX / 2) {
            return 0;
        }
        int newcap = 2 * o->capacity;
        
        struct CStringTrie__node *newarr = BAllocArray(newcap, sizeof(newarr[0]));
        if (!newarr) {
            return 0;
        }
        
        memcpy(newarr, o->arr, o->count * sizeof(newarr[0]));
        BFree(o->arr);
        
        o->arr = newarr;
        o->capacity = newcap;
    }
    
    struct CStringTrie__node *node = &o->arr[o->count];
    
    node->value = CSTRINGTRIE_PARAM_DEFAULT;
    
    for (int i = 0; i < CStringTrie__DEGREE; i++) {
        node->links[i] = -1;
    }
    
    *out_nodeidx = o->count;
    o->count++;
    
    return 1;
}

static int CStringTrie_Init (CStringTrie *o)
{
    o->count = 0;
    o->capacity = 1;
    
    if (!(o->arr = BAllocArray(o->capacity, sizeof(o->arr[0])))) {
        return 0;
    }
    
    int idx0;
    CStringTrie__new_node(o, &idx0);
    
    return 1;
}

static void CStringTrie_Free (CStringTrie *o)
{
    BFree(o->arr);
}

static int CStringTrie_Set (CStringTrie *o, const char *key, CStringTrieValue value)
{
    ASSERT(key)
    
    const unsigned char *ukey = (const unsigned char *)key;
    int nodeidx = 0;
    
    while (*ukey) {
        unsigned char mod = *ukey % CStringTrie__DEGREE;
        ASSERT(o->arr[nodeidx].links[mod] >= -1)
        
        int new_nodeidx;
        
        if (o->arr[nodeidx].links[mod] >= 0) {
            new_nodeidx = o->arr[nodeidx].links[mod];
        } else {
            if (!CStringTrie__new_node(o, &new_nodeidx)) {
                return 0;
            }
            o->arr[nodeidx].links[mod] = new_nodeidx;
        }
        
        ASSERT(new_nodeidx >= 0)
        ASSERT(new_nodeidx < o->count)
        
        nodeidx = new_nodeidx;
        ukey++;
    }
    
    o->arr[nodeidx].value = value;
    
    return 1;
}

static CStringTrieValue CStringTrie_Get (const CStringTrie *o, const char *key)
{
    ASSERT(key)
    
    const unsigned char *ukey = (const unsigned char *)key;
    int nodeidx = 0;
    
    while (*ukey) {
        unsigned char mod = *ukey % CStringTrie__DEGREE;
        nodeidx = o->arr[nodeidx].links[mod];
        ASSERT(nodeidx >= -1)
        if (nodeidx < 0) {
            return CSTRINGTRIE_PARAM_DEFAULT;
        }
        ASSERT(nodeidx < o->count)
        ukey++;
    }
    
    return o->arr[nodeidx].value;
}

#include "CStringTrie_footer.h"
