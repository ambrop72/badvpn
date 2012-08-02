/**
 * @file CHash_impl.h
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

#include "CHash_header.h"

static CHashLink CHashNullLink ()
{
    return CHASH_PARAM_NULL;
}

static CHashRef CHashNullRef ()
{
    CHashRef entry = {NULL, CHASH_PARAM_NULL};
    return entry;
}

static int CHash_Init (CHash *o, size_t numBuckets)
{
    if (numBuckets == 0) {
        numBuckets = 1;
    }
    
    o->numBuckets = numBuckets;
    o->numEntries = 0;
    
    o->buckets = (CHashLink *)BAllocArray(o->numBuckets, sizeof(o->buckets[0])); 
    if (!o->buckets) {
        return 0;
    }
    
    for (size_t i = 0; i < o->numBuckets; i++) {
        o->buckets[i] = CHashNullLink();
    }
    
    return 1;
}

static void CHash_Free (CHash *o)
{
    BFree(o->buckets);
}

static CHashRef CHash_Deref (CHashArg arg, CHashLink link)
{
    if (link == CHashNullLink()) {
        return CHashNullRef();
    }
    
    CHashRef entry;
    entry.ptr = CHASH_PARAM_DEREF(arg, link);
    entry.link = link;
    
    ASSERT(entry.ptr)
    
    return entry;
}

static int CHash_Insert (CHash *o, CHashArg arg, CHashRef entry, CHashRef *out_existing)
{
    CHashKey key = CHASH_PARAM_GETKEY(arg, entry);
    size_t index = CHASH_PARAM_HASHFUN(arg, key) % o->numBuckets;
    
    CHashRef e = CHash_Deref(arg, o->buckets[index]);
    while (e.link != CHashNullLink()) {
        if (CHASH_PARAM_KEYSEQUAL(arg, key, CHASH_PARAM_GETKEY(arg, e))) {
            if (out_existing) {
                *out_existing = e;
            }
            return 0;
        }
        e = CHash_Deref(arg, e.ptr->CHASH_PARAM_ENTRY_NEXT);
    }
    
    entry.ptr->CHASH_PARAM_ENTRY_NEXT = o->buckets[index];
    o->buckets[index] = entry.link;
    
    o->numEntries++;
    
    if (out_existing) {
        *out_existing = entry;
    }
    return 1;
}

static void CHash_InsertMulti (CHash *o, CHashArg arg, CHashRef entry)
{
    CHashKey key = CHASH_PARAM_GETKEY(arg, entry);
    size_t index = CHASH_PARAM_HASHFUN(arg, key) % o->numBuckets;
    
    CHashRef prev = CHashNullRef();
    CHashRef cur = CHash_Deref(arg, o->buckets[index]);
    while (cur.link != CHashNullLink()) {
        if (CHASH_PARAM_KEYSEQUAL(arg, CHASH_PARAM_GETKEY(arg, cur), key)) {
            break;
        }
        prev = cur;
        cur = CHash_Deref(arg, cur.ptr->CHASH_PARAM_ENTRY_NEXT);
    }
    
    if (cur.link == CHashNullLink() || prev.link == CHashNullLink()) {
        entry.ptr->CHASH_PARAM_ENTRY_NEXT = o->buckets[index];
        o->buckets[index] = entry.link;
    } else {
        entry.ptr->CHASH_PARAM_ENTRY_NEXT = cur.link;
        prev.ptr->CHASH_PARAM_ENTRY_NEXT = entry.link;
    }
    
    o->numEntries++;
}

static void CHash_Remove (CHash *o, CHashArg arg, CHashRef entry)
{
    CHashKey key = CHASH_PARAM_GETKEY(arg, entry);
    size_t index = CHASH_PARAM_HASHFUN(arg, key) % o->numBuckets;
    
    CHashRef prev = CHashNullRef();
    CHashRef cur = CHash_Deref(arg, o->buckets[index]);
    while (cur.link != entry.link) {
        prev = cur;
        cur = CHash_Deref(arg, cur.ptr->CHASH_PARAM_ENTRY_NEXT);
        ASSERT(cur.link != CHashNullLink());
    }
    
    if (prev.link == CHashNullLink()) {
        o->buckets[index] = entry.ptr->CHASH_PARAM_ENTRY_NEXT;
    } else {
        prev.ptr->CHASH_PARAM_ENTRY_NEXT = entry.ptr->CHASH_PARAM_ENTRY_NEXT;
    }
    
    o->numEntries--;
}

static CHashRef CHash_Lookup (const CHash *o, CHashArg arg, CHashKey key) 
{
    size_t index = CHASH_PARAM_HASHFUN(arg, key) % o->numBuckets;
    
    CHashLink link = o->buckets[index];
    while (link != CHashNullLink()) {
        CHashRef e = CHash_Deref(arg, link);
        if (CHASH_PARAM_KEYSEQUAL(arg, CHASH_PARAM_GETKEY(arg, e), key)) {
            return e;
        }
        link = e.ptr->CHASH_PARAM_ENTRY_NEXT;
    }
    
    return CHashNullRef();
}

static CHashRef CHash_GetFirst (const CHash *o, CHashArg arg)
{
    size_t i = 0;
    while (i < o->numBuckets && o->buckets[i] == CHashNullLink()) {
        i++;
    }
    
    if (i == o->numBuckets) {
        return CHashNullRef();
    }
    
    return CHash_Deref(arg, o->buckets[i]);
}

static CHashRef CHash_GetNext (const CHash *o, CHashArg arg, CHashRef entry)
{
    CHashLink next = entry.ptr->CHASH_PARAM_ENTRY_NEXT;
    if (next != CHashNullLink()) {
        return CHash_Deref(arg, next);
    }
    
    CHashKey key = CHASH_PARAM_GETKEY(arg, entry);
    size_t i = CHASH_PARAM_HASHFUN(arg, key) % o->numBuckets;
    i++;
    
    while (i < o->numBuckets && o->buckets[i] == CHashNullLink()) {
        i++;
    }
    
    if (i == o->numBuckets) {
        return CHashNullRef();
    }
    
    return CHash_Deref(arg, o->buckets[i]);
}

static CHashRef CHash_GetNextEqual (const CHash *o, CHashArg arg, CHashRef entry)
{
    CHashLink next = entry.ptr->CHASH_PARAM_ENTRY_NEXT;
    
    if (next == CHashNullLink()) {
        return CHashNullRef();
    }
    
    CHashRef next_ref = CHash_Deref(arg, next);
    if (!CHASH_PARAM_KEYSEQUAL(arg, CHASH_PARAM_GETKEY(arg, next_ref), CHASH_PARAM_GETKEY(arg, entry))) {
        return CHashNullRef();
    }
    
    return next_ref;
}

static size_t CHash_NumEntries (const CHash *o)
{
    return o->numEntries;
}

static int CHash_IsEmpty (const CHash *o)
{
    return o->numEntries == 0;
}

#include "CHash_footer.h"
