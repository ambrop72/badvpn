/**
 * @file NCDVal.c
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

#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>

#include <misc/bsize.h>
#include <misc/balloc.h>
#include <misc/strdup.h>
#include <misc/offset.h>
#include <base/BLog.h>

#include "NCDVal.h"

#include <generated/blog_channel_NCDVal.h>

//#define NCDVAL_TEST_EXTERNAL_STRINGS

#define EXTERNAL_TYPE_MASK ((1 << 3) - 1)
#define IDSTRING_TYPE (NCDVAL_STRING | (1 << 3))
#define EXTERNALSTRING_TYPE (NCDVAL_STRING | (2 << 3))

static void * NCDValMem__BufAt (NCDValMem *o, NCDVal__idx idx)
{
    ASSERT(idx >= 0)
    ASSERT(idx < o->used)
    
    return (o->buf ? o->buf : o->fastbuf) + idx;
}

static NCDVal__idx NCDValMem__Alloc (NCDValMem *o, bsize_t alloc_size, NCDVal__idx align)
{
    if (alloc_size.is_overflow) {
        return -1;
    }
    
    NCDVal__idx mod = o->used % align;
    NCDVal__idx align_extra = mod ? (align - mod) : 0;
    
    if (alloc_size.value > NCDVAL_MAXIDX - align_extra) {
        return -1;
    }
    NCDVal__idx aligned_alloc_size = align_extra + alloc_size.value;
    
    if (aligned_alloc_size > o->size - o->used) {
        NCDVal__idx newsize = (o->buf ? o->size : NCDVAL_FIRST_SIZE);
        while (aligned_alloc_size > newsize - o->used) {
            if (newsize > NCDVAL_MAXIDX / 2) {
                return -1;
            }
            newsize *= 2;
        }
        
        char *newbuf;
        
        if (!o->buf) {
            newbuf = malloc(newsize);
            if (!newbuf) {
                return -1;
            }
            memcpy(newbuf, o->fastbuf, o->used);
        } else {
            newbuf = realloc(o->buf, newsize);
            if (!newbuf) {
                return -1;
            }
        }
        
        o->buf = newbuf;
        o->size = newsize;
    }
    
    NCDVal__idx idx = o->used + align_extra;
    o->used += aligned_alloc_size;
    
    return idx;
}

static NCDValRef NCDVal__Ref (NCDValMem *mem, NCDVal__idx idx)
{
    ASSERT(idx == -1 || mem)
    
    NCDValRef ref = {mem, idx};
    return ref;
}

static void NCDVal__AssertMem (NCDValMem *mem)
{
    ASSERT(mem)
    ASSERT(mem->size >= 0)
    ASSERT(mem->used >= 0)
    ASSERT(mem->used <= mem->size)
    ASSERT(mem->buf || mem->size == NCDVAL_FASTBUF_SIZE)
    ASSERT(!mem->buf || mem->size >= NCDVAL_FIRST_SIZE)
}

static void NCDVal_AssertExternal (NCDValMem *mem, const void *e_buf, size_t e_len)
{
#ifndef NDEBUG
    const char *e_cbuf = e_buf;
    char *buf = (mem->buf ? mem->buf : mem->fastbuf);
    ASSERT(e_cbuf >= buf + mem->size || e_cbuf + e_len <= buf)
#endif
}

static void NCDVal__AssertValOnly (NCDValMem *mem, NCDVal__idx idx)
{
    // placeholders
    if (idx < -1) {
        return;
    }
    
    ASSERT(idx >= 0)
    ASSERT(idx + sizeof(int) <= mem->used)
    
#ifndef NDEBUG
    int *type_ptr = NCDValMem__BufAt(mem, idx);
    
    switch (*type_ptr) {
        case NCDVAL_STRING: {
            ASSERT(idx + sizeof(struct NCDVal__string) <= mem->used)
            struct NCDVal__string *str_e = NCDValMem__BufAt(mem, idx);
            ASSERT(str_e->length >= 0)
            ASSERT(idx + sizeof(struct NCDVal__string) + str_e->length + 1 <= mem->used)
        } break;
        case NCDVAL_LIST: {
            ASSERT(idx + sizeof(struct NCDVal__list) <= mem->used)
            struct NCDVal__list *list_e = NCDValMem__BufAt(mem, idx);
            ASSERT(list_e->maxcount >= 0)
            ASSERT(list_e->count >= 0)
            ASSERT(list_e->count <= list_e->maxcount)
            ASSERT(idx + sizeof(struct NCDVal__list) + list_e->maxcount * sizeof(NCDVal__idx) <= mem->used)
        } break;
        case NCDVAL_MAP: {
            ASSERT(idx + sizeof(struct NCDVal__map) <= mem->used)
            struct NCDVal__map *map_e = NCDValMem__BufAt(mem, idx);
            ASSERT(map_e->maxcount >= 0)
            ASSERT(map_e->count >= 0)
            ASSERT(map_e->count <= map_e->maxcount)
            ASSERT(idx + sizeof(struct NCDVal__map) + map_e->maxcount * sizeof(struct NCDVal__mapelem) <= mem->used)
        } break;
        case IDSTRING_TYPE: {
            ASSERT(idx + sizeof(struct NCDVal__idstring) <= mem->used)
            struct NCDVal__idstring *ids_e = NCDValMem__BufAt(mem, idx);
            ASSERT(ids_e->string_id >= 0)
            ASSERT(ids_e->string_index)
        } break;
        case EXTERNALSTRING_TYPE: {
            ASSERT(idx + sizeof(struct NCDVal__externalstring) <= mem->used)
            struct NCDVal__externalstring *exs_e = NCDValMem__BufAt(mem, idx);
            ASSERT(exs_e->data)
            ASSERT(!exs_e->ref.target || exs_e->ref.next >= -1)
            ASSERT(!exs_e->ref.target || exs_e->ref.next < mem->used)
        } break;
        default: ASSERT(0);
    }
#endif
}

static void NCDVal__AssertVal (NCDValRef val)
{
    NCDVal__AssertMem(val.mem);
    NCDVal__AssertValOnly(val.mem, val.idx);
}

static NCDValMapElem NCDVal__MapElem (NCDVal__idx elemidx)
{
    ASSERT(elemidx >= 0 || elemidx == -1)
    
    NCDValMapElem me = {elemidx};
    return me;
}

static void NCDVal__MapAssertElemOnly (NCDValRef map, NCDVal__idx elemidx)
{
#ifndef NDEBUG
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    ASSERT(elemidx >= map.idx + offsetof(struct NCDVal__map, elems))
    ASSERT(elemidx < map.idx + offsetof(struct NCDVal__map, elems) + map_e->count * sizeof(struct NCDVal__mapelem))

    struct NCDVal__mapelem *me_e = NCDValMem__BufAt(map.mem, elemidx);
    NCDVal__AssertValOnly(map.mem, me_e->key_idx);
    NCDVal__AssertValOnly(map.mem, me_e->val_idx);
#endif
}

static void NCDVal__MapAssertElem (NCDValRef map, NCDValMapElem me)
{
    ASSERT(NCDVal_IsMap(map))
    NCDVal__MapAssertElemOnly(map, me.elemidx);
}

static NCDVal__idx NCDVal__MapElemIdx (NCDVal__idx mapidx, NCDVal__idx pos)
{
    return mapidx + offsetof(struct NCDVal__map, elems) + pos * sizeof(struct NCDVal__mapelem);
}

#include "NCDVal_maptree.h"
#include <structure/CAvl_impl.h>

void NCDValMem_Init (NCDValMem *o)
{
    o->buf = NULL;
    o->size = NCDVAL_FASTBUF_SIZE;
    o->used = 0;
    o->first_ref = -1;
}

void NCDValMem_Free (NCDValMem *o)
{
    NCDVal__AssertMem(o);
    
    NCDVal__idx refidx = o->first_ref;
    while (refidx != -1) {
        struct NCDVal__ref *ref = NCDValMem__BufAt(o, refidx);
        ASSERT(ref->target)
        NCDRefTarget_Deref(ref->target);
        refidx = ref->next;
    }
    
    if (o->buf) {
        BFree(o->buf);
    }
}

int NCDValMem_InitCopy (NCDValMem *o, NCDValMem *other)
{
    NCDVal__AssertMem(other);
    
    o->size = other->size;
    o->used = other->used;
    o->first_ref = other->first_ref;
    
    if (!other->buf) {
        o->buf = NULL;
        memcpy(o->fastbuf, other->fastbuf, other->used);
    } else {
        o->buf = BAlloc(other->size);
        if (!o->buf) {
            goto fail0;
        }
        memcpy(o->buf, other->buf, other->used);
    }
    
    NCDVal__idx refidx = o->first_ref;
    while (refidx != -1) {
        struct NCDVal__ref *ref = NCDValMem__BufAt(o, refidx);
        ASSERT(ref->target)
        if (!NCDRefTarget_Ref(ref->target)) {
            goto fail1;
        }
        refidx = ref->next;
    }
    
    return 1;
    
fail1:;
    NCDVal__idx undo_refidx = o->first_ref;
    while (undo_refidx != refidx) {
        struct NCDVal__ref *ref = NCDValMem__BufAt(o, undo_refidx);
        NCDRefTarget_Deref(ref->target);
        undo_refidx = ref->next;
    }
    if (o->buf) {
        BFree(o->buf);
    }
fail0:
    return 0;
}

void NCDVal_Assert (NCDValRef val)
{
    ASSERT(val.idx == -1 || (NCDVal__AssertVal(val), 1))
}

int NCDVal_IsInvalid (NCDValRef val)
{
    NCDVal_Assert(val);
    
    return (val.idx == -1);
}

int NCDVal_IsPlaceholder (NCDValRef val)
{
    NCDVal_Assert(val);
    
    return (val.idx < -1);
}

int NCDVal_Type (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    if (val.idx < -1) {
        return NCDVAL_PLACEHOLDER;
    }
    
    int *type_ptr = NCDValMem__BufAt(val.mem, val.idx);
    
    return (*type_ptr & EXTERNAL_TYPE_MASK);
}

NCDValRef NCDVal_NewInvalid (void)
{
    NCDValRef ref = {NULL, -1};
    return ref;
}

NCDValRef NCDVal_NewPlaceholder (NCDValMem *mem, int plid)
{
    NCDVal__AssertMem(mem);
    ASSERT(plid >= 0)
    ASSERT(NCDVAL_MINIDX + plid < -1)
    
    NCDValRef ref = {mem, NCDVAL_MINIDX + plid};
    return ref;
}

int NCDVal_PlaceholderId (NCDValRef val)
{
    ASSERT(NCDVal_IsPlaceholder(val))
    
    return (val.idx - NCDVAL_MINIDX);
}

NCDValRef NCDVal_NewCopy (NCDValMem *mem, NCDValRef val)
{
    NCDVal__AssertMem(mem);
    NCDVal__AssertVal(val);
    
    if (val.idx < -1) {
        return NCDVal_NewPlaceholder(mem, NCDVal_PlaceholderId(val));
    }
    
    void *ptr = NCDValMem__BufAt(val.mem, val.idx);
    
    switch (*(int *)ptr) {
        case NCDVAL_STRING: {
            size_t len = NCDVal_StringLength(val);
            
            NCDValRef copy = NCDVal_NewStringUninitialized(mem, len);
            if (NCDVal_IsInvalid(copy)) {
                goto fail;
            }
            
            memcpy((char *)NCDVal_StringData(copy), NCDVal_StringData(val), len);
            
            return copy;
        } break;
        
        case NCDVAL_LIST: {
            size_t count = NCDVal_ListCount(val);
            
            NCDValRef copy = NCDVal_NewList(mem, count);
            if (NCDVal_IsInvalid(copy)) {
                goto fail;
            }
            
            for (size_t i = 0; i < count; i++) {
                NCDValRef elem_copy = NCDVal_NewCopy(mem, NCDVal_ListGet(val, i));
                if (NCDVal_IsInvalid(elem_copy)) {
                    goto fail;
                }
                
                NCDVal_ListAppend(copy, elem_copy);
            }
            
            return copy;
        } break;
        
        case NCDVAL_MAP: {
            size_t count = NCDVal_MapCount(val);
            
            NCDValRef copy = NCDVal_NewMap(mem, count);
            if (NCDVal_IsInvalid(copy)) {
                goto fail;
            }
            
            for (NCDValMapElem e = NCDVal_MapFirst(val); !NCDVal_MapElemInvalid(e); e = NCDVal_MapNext(val, e)) {
                NCDValRef key_copy = NCDVal_NewCopy(mem, NCDVal_MapElemKey(val, e));
                NCDValRef val_copy = NCDVal_NewCopy(mem, NCDVal_MapElemVal(val, e));
                if (NCDVal_IsInvalid(key_copy) || NCDVal_IsInvalid(val_copy)) {
                    goto fail;
                }
                
                int res = NCDVal_MapInsert(copy, key_copy, val_copy);
                ASSERT_EXECUTE(res)
            }
            
            return copy;
        } break;
        
        case IDSTRING_TYPE: {
            struct NCDVal__idstring *ids_e = ptr;
            
            return NCDVal_NewIdString(mem, ids_e->string_id, ids_e->string_index);
        } break;
        
        case EXTERNALSTRING_TYPE: {
            struct NCDVal__externalstring *exs_e = ptr;
            
            return NCDVal_NewExternalString(mem, exs_e->data, exs_e->length, exs_e->ref.target);
        } break;
        
        default: ASSERT(0);
    }
    
    ASSERT(0);
    
fail:
    return NCDVal_NewInvalid();
}

int NCDVal_Compare (NCDValRef val1, NCDValRef val2)
{
    NCDVal__AssertVal(val1);
    NCDVal__AssertVal(val2);
    
    int type1 = NCDVal_Type(val1);
    int type2 = NCDVal_Type(val2);
    
    if (type1 != type2) {
        return (type1 > type2) - (type1 < type2);
    }
    
    switch (type1) {
        case NCDVAL_STRING: {
            size_t len1 = NCDVal_StringLength(val1);
            size_t len2 = NCDVal_StringLength(val2);
            size_t min_len = len1 < len2 ? len1 : len2;
            
            int cmp = memcmp(NCDVal_StringData(val1), NCDVal_StringData(val2), min_len);
            if (cmp) {
                return (cmp > 0) - (cmp < 0);
            }
            
            return (len1 > len2) - (len1 < len2);
        } break;
        
        case NCDVAL_LIST: {
            size_t count1 = NCDVal_ListCount(val1);
            size_t count2 = NCDVal_ListCount(val2);
            size_t min_count = count1 < count2 ? count1 : count2;
            
            for (size_t i = 0; i < min_count; i++) {
                NCDValRef ev1 = NCDVal_ListGet(val1, i);
                NCDValRef ev2 = NCDVal_ListGet(val2, i);
                
                int cmp = NCDVal_Compare(ev1, ev2);
                if (cmp) {
                    return cmp;
                }
            }
            
            return (count1 > count2) - (count1 < count2);
        } break;
        
        case NCDVAL_MAP: {
            NCDValMapElem e1 = NCDVal_MapOrderedFirst(val1);
            NCDValMapElem e2 = NCDVal_MapOrderedFirst(val2);
            
            while (1) {
                int inv1 = NCDVal_MapElemInvalid(e1);
                int inv2 = NCDVal_MapElemInvalid(e2);
                if (inv1 || inv2) {
                    return inv2 - inv1;
                }
                
                NCDValRef key1 = NCDVal_MapElemKey(val1, e1);
                NCDValRef key2 = NCDVal_MapElemKey(val2, e2);
                
                int cmp = NCDVal_Compare(key1, key2);
                if (cmp) {
                    return cmp;
                }
                
                NCDValRef value1 = NCDVal_MapElemVal(val1, e1);
                NCDValRef value2 = NCDVal_MapElemVal(val2, e2);
                
                cmp = NCDVal_Compare(value1, value2);
                if (cmp) {
                    return cmp;
                }
                
                e1 = NCDVal_MapOrderedNext(val1, e1);
                e2 = NCDVal_MapOrderedNext(val2, e2);
            }
        } break;
        
        case NCDVAL_PLACEHOLDER: {
            int plid1 = NCDVal_PlaceholderId(val1);
            int plid2 = NCDVal_PlaceholderId(val2);
            
            return (plid1 > plid2) - (plid1 < plid2);
        } break;
        
        default:
            ASSERT(0);
            return 0;
    }
}

NCDValSafeRef NCDVal_ToSafe (NCDValRef val)
{
    NCDVal_Assert(val);
    
    NCDValSafeRef sval = {val.idx};
    return sval;
}

NCDValRef NCDVal_FromSafe (NCDValMem *mem, NCDValSafeRef sval)
{
    NCDVal__AssertMem(mem);
    ASSERT(sval.idx == -1 || (NCDVal__AssertValOnly(mem, sval.idx), 1))
    
    NCDValRef val = {mem, sval.idx};
    return val;
}

NCDValRef NCDVal_Moved (NCDValMem *mem, NCDValRef val)
{
    NCDVal__AssertMem(mem);
    ASSERT(val.idx == -1 || (NCDVal__AssertValOnly(mem, val.idx), 1))
    
    NCDValRef val2 = {mem, val.idx};
    return val2;
}

int NCDVal_IsString (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return NCDVal_Type(val) == NCDVAL_STRING;
}

int NCDVal_IsIdString (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return !(val.idx < -1) && *(int *)NCDValMem__BufAt(val.mem, val.idx) == IDSTRING_TYPE;
}

int NCDVal_IsExternalString (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return !(val.idx < -1) && *(int *)NCDValMem__BufAt(val.mem, val.idx) == EXTERNALSTRING_TYPE;
}

int NCDVal_IsStringNoNulls (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return NCDVal_Type(val) == NCDVAL_STRING && !NCDVal_StringHasNulls(val);
}

NCDValRef NCDVal_NewString (NCDValMem *mem, const char *data)
{
    NCDVal__AssertMem(mem);
    ASSERT(data)
    NCDVal_AssertExternal(mem, data, strlen(data));
    
    return NCDVal_NewStringBin(mem, (const uint8_t *)data, strlen(data));
}

#ifdef NCDVAL_TEST_EXTERNAL_STRINGS

struct test_ext_str {
    NCDRefTarget ref_target;
    char *data;
};

static void test_ext_str_ref_target_func_dealloc (NCDRefTarget *ref_target)
{
    struct test_ext_str *tes = UPPER_OBJECT(ref_target, struct test_ext_str, ref_target);
    BFree(tes->data);
    BFree(tes);
}

NCDValRef NCDVal_NewStringBin (NCDValMem *mem, const uint8_t *data, size_t len)
{
    NCDVal__AssertMem(mem);
    ASSERT(len == 0 || data)
    NCDVal_AssertExternal(mem, data, len);
    
    struct test_ext_str *tes = BAlloc(sizeof(*tes));
    if (!tes) {
        goto fail0;
    }
    
    tes->data = BAlloc(len);
    if (!tes->data) {
        goto fail1;
    }
    
    if (len > 0) {
        memcpy(tes->data, data, len);
    }
    
    NCDRefTarget_Init(&tes->ref_target, test_ext_str_ref_target_func_dealloc);
    
    NCDValRef res = NCDVal_NewExternalString(mem, tes->data, len, &tes->ref_target);
    NCDRefTarget_Deref(&tes->ref_target);
    return res;
    
fail1:
    BFree(tes);
fail0:
    return NCDVal_NewInvalid();
}

#endif

#ifndef NCDVAL_TEST_EXTERNAL_STRINGS

NCDValRef NCDVal_NewStringBin (NCDValMem *mem, const uint8_t *data, size_t len)
{
    NCDVal__AssertMem(mem);
    ASSERT(len == 0 || data)
    NCDVal_AssertExternal(mem, data, len);
    
    if (len == SIZE_MAX) {
        goto fail;
    }
    
    bsize_t size = bsize_add(bsize_fromsize(sizeof(struct NCDVal__string)), bsize_fromsize(len + 1));
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__string));
    if (idx < 0) {
        goto fail;
    }
    
    struct NCDVal__string *str_e = NCDValMem__BufAt(mem, idx);
    str_e->type = NCDVAL_STRING;
    str_e->length = len;
    if (len > 0) {
        memcpy(str_e->data, data, len);
    }
    str_e->data[len] = '\0';
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

#endif

NCDValRef NCDVal_NewStringUninitialized (NCDValMem *mem, size_t len)
{
    NCDVal__AssertMem(mem);
    
    if (len == SIZE_MAX) {
        goto fail;
    }
    
    bsize_t size = bsize_add(bsize_fromsize(sizeof(struct NCDVal__string)), bsize_fromsize(len + 1));
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__string));
    if (idx < 0) {
        goto fail;
    }
    
    struct NCDVal__string *str_e = NCDValMem__BufAt(mem, idx);
    str_e->type = NCDVAL_STRING;
    str_e->length = len;
    str_e->data[len] = '\0';
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

NCDValRef NCDVal_NewIdString (NCDValMem *mem, NCD_string_id_t string_id, NCDStringIndex *string_index)
{
    NCDVal__AssertMem(mem);
    ASSERT(string_id >= 0)
    ASSERT(string_index)
    
    bsize_t size = bsize_fromsize(sizeof(struct NCDVal__idstring));
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__idstring));
    if (idx < 0) {
        goto fail;
    }
    
    struct NCDVal__idstring *ids_e = NCDValMem__BufAt(mem, idx);
    ids_e->type = IDSTRING_TYPE;
    ids_e->string_id = string_id;
    ids_e->string_index = string_index;
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

NCDValRef NCDVal_NewExternalString (NCDValMem *mem, const char *data, size_t len,
                                    NCDRefTarget *ref_target)
{
    NCDVal__AssertMem(mem);
    ASSERT(data)
    NCDVal_AssertExternal(mem, data, len);
    
    bsize_t size = bsize_fromsize(sizeof(struct NCDVal__externalstring));
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__externalstring));
    if (idx < 0) {
        goto fail;
    }
    
    if (ref_target) {
        if (!NCDRefTarget_Ref(ref_target)) {
            goto fail;
        }
    }
    
    struct NCDVal__externalstring *exs_e = NCDValMem__BufAt(mem, idx);
    exs_e->type = EXTERNALSTRING_TYPE;
    exs_e->data = data;
    exs_e->length = len;
    exs_e->ref.target = ref_target;
    
    if (ref_target) {
        exs_e->ref.next = mem->first_ref;
        mem->first_ref = idx + offsetof(struct NCDVal__externalstring, ref);
    }
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

const char * NCDVal_StringData (NCDValRef string)
{
    ASSERT(NCDVal_IsString(string))
    
    void *ptr = NCDValMem__BufAt(string.mem, string.idx);
    
    switch (*(int *)ptr) {
        case IDSTRING_TYPE: {
            struct NCDVal__idstring *ids_e = ptr;
            const char *value = NCDStringIndex_Value(ids_e->string_index, ids_e->string_id);
            return value;
        } break;
        
        case EXTERNALSTRING_TYPE: {
            struct NCDVal__externalstring *exs_e = ptr;
            return exs_e->data;
        } break;
    }
    
    struct NCDVal__string *str_e = ptr;
    return str_e->data;
}

size_t NCDVal_StringLength (NCDValRef string)
{
    ASSERT(NCDVal_IsString(string))
    
    void *ptr = NCDValMem__BufAt(string.mem, string.idx);
    
    switch (*(int *)ptr) {
        case IDSTRING_TYPE: {
            struct NCDVal__idstring *ids_e = ptr;
            return NCDStringIndex_Length(ids_e->string_index, ids_e->string_id);
        } break;
        
        case EXTERNALSTRING_TYPE: {
            struct NCDVal__externalstring *exs_e = ptr;
            return exs_e->length;
        } break;
    }
    
    struct NCDVal__string *str_e = ptr;;
    return str_e->length;
}

int NCDVal_StringNullTerminate (NCDValRef string, NCDValNullTermString *out)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(out)
    
    void *ptr = NCDValMem__BufAt(string.mem, string.idx);
    
    switch (*(int *)ptr) {
        case IDSTRING_TYPE: {
            struct NCDVal__idstring *ids_e = ptr;
            out->data = (char *)NCDStringIndex_Value(ids_e->string_index, ids_e->string_id);
            out->is_allocated = 0;
            return 1;
        } break;
        
        case EXTERNALSTRING_TYPE: {
            struct NCDVal__externalstring *exs_e = ptr;
            
            char *copy = b_strdup_bin(exs_e->data, exs_e->length);
            if (!copy) {
                return 0;
            }
            
            out->data = copy;
            out->is_allocated = 1;
            return 1;
        } break;
    }
    
    struct NCDVal__string *str_e = ptr;
    out->data = str_e->data;
    out->is_allocated = 0;
    return 1;
}

NCDValNullTermString NCDValNullTermString_NewDummy (void)
{
    NCDValNullTermString nts;
    nts.data = NULL;
    nts.is_allocated = 0;
    return nts;
}

void NCDValNullTermString_Free (NCDValNullTermString *o)
{
    if (o->is_allocated) {
        BFree(o->data);
    }
}

void NCDVal_IdStringGet (NCDValRef idstring, NCD_string_id_t *out_string_id,
                         NCDStringIndex **out_string_index)
{
    ASSERT(NCDVal_IsIdString(idstring))
    ASSERT(out_string_id)
    ASSERT(out_string_index)
    
    struct NCDVal__idstring *ids_e = NCDValMem__BufAt(idstring.mem, idstring.idx);
    *out_string_id = ids_e->string_id;
    *out_string_index = ids_e->string_index;
}

NCD_string_id_t NCDVal_IdStringId (NCDValRef idstring)
{
    ASSERT(NCDVal_IsIdString(idstring))
    
    struct NCDVal__idstring *ids_e = NCDValMem__BufAt(idstring.mem, idstring.idx);
    return ids_e->string_id;
}

NCDStringIndex * NCDVal_IdStringStringIndex (NCDValRef idstring)
{
    ASSERT(NCDVal_IsIdString(idstring))
    
    struct NCDVal__idstring *ids_e = NCDValMem__BufAt(idstring.mem, idstring.idx);
    return ids_e->string_index;
}

NCDRefTarget * NCDVal_ExternalStringTarget (NCDValRef externalstring)
{
    ASSERT(NCDVal_IsExternalString(externalstring))
    
    struct NCDVal__externalstring *exs_e = NCDValMem__BufAt(externalstring.mem, externalstring.idx);
    return exs_e->ref.target;
}

int NCDVal_StringHasNulls (NCDValRef string)
{
    ASSERT(NCDVal_IsString(string))
    
    void *ptr = NCDValMem__BufAt(string.mem, string.idx);
    
    switch (*(int *)ptr) {
        case IDSTRING_TYPE: {
            struct NCDVal__idstring *ids_e = ptr;
            return NCDStringIndex_HasNulls(ids_e->string_index, ids_e->string_id);
        } break;
        
        default: {
            const char *data = NCDVal_StringData(string);
            size_t length = NCDVal_StringLength(string);
            return !!memchr(data, '\0', length);
        } break;
    }
}

int NCDVal_StringEquals (NCDValRef string, const char *data)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(data)
    
    size_t len = strlen(data);
    
    return NCDVal_StringLength(string) == len && !memcmp(NCDVal_StringData(string), data, len);
}

int NCDVal_StringEqualsId (NCDValRef string, NCD_string_id_t string_id,
                           NCDStringIndex *string_index)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(string_id >= 0)
    ASSERT(string_index)
    
    void *ptr = NCDValMem__BufAt(string.mem, string.idx);
    
    switch (*(int *)ptr) {
        case IDSTRING_TYPE: {
            struct NCDVal__idstring *ids_e = ptr;
            ASSERT(ids_e->string_index == string_index)
            return ids_e->string_id == string_id;
        } break;
        
        case EXTERNALSTRING_TYPE: {
            struct NCDVal__externalstring *exs_e = ptr;
            const char *string_data = NCDStringIndex_Value(string_index, string_id);
            size_t string_length = NCDStringIndex_Length(string_index, string_id);
            return (string_length == exs_e->length) && !memcmp(string_data, exs_e->data, string_length);
        } break;
    }
    
    struct NCDVal__string *str_e = ptr;
    const char *string_data = NCDStringIndex_Value(string_index, string_id);
    size_t string_length = NCDStringIndex_Length(string_index, string_id);
    return (string_length == str_e->length) && !memcmp(string_data, str_e->data, string_length);
}

int NCDVal_IsList (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return NCDVal_Type(val) == NCDVAL_LIST;
}

NCDValRef NCDVal_NewList (NCDValMem *mem, size_t maxcount)
{
    NCDVal__AssertMem(mem);
    
    bsize_t size = bsize_add(bsize_fromsize(sizeof(struct NCDVal__list)), bsize_mul(bsize_fromsize(maxcount), bsize_fromsize(sizeof(NCDVal__idx))));
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__list));
    if (idx < 0) {
        goto fail;
    }
    
    struct NCDVal__list *list_e = NCDValMem__BufAt(mem, idx);
    list_e->type = NCDVAL_LIST;
    list_e->maxcount = maxcount;
    list_e->count = 0;
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

void NCDVal_ListAppend (NCDValRef list, NCDValRef elem)
{
    ASSERT(NCDVal_IsList(list))
    ASSERT(NCDVal_ListCount(list) < NCDVal_ListMaxCount(list))
    ASSERT(elem.mem == list.mem)
    NCDVal__AssertValOnly(list.mem, elem.idx);
    
    struct NCDVal__list *list_e = NCDValMem__BufAt(list.mem, list.idx);
    
    list_e->elem_indices[list_e->count++] = elem.idx;
}

size_t NCDVal_ListCount (NCDValRef list)
{
    ASSERT(NCDVal_IsList(list))
    
    struct NCDVal__list *list_e = NCDValMem__BufAt(list.mem, list.idx);
    
    return list_e->count;
}

size_t NCDVal_ListMaxCount (NCDValRef list)
{
    ASSERT(NCDVal_IsList(list))
    
    struct NCDVal__list *list_e = NCDValMem__BufAt(list.mem, list.idx);
    
    return list_e->maxcount;
}

NCDValRef NCDVal_ListGet (NCDValRef list, size_t pos)
{
    ASSERT(NCDVal_IsList(list))
    ASSERT(pos < NCDVal_ListCount(list))
    
    struct NCDVal__list *list_e = NCDValMem__BufAt(list.mem, list.idx);
    
    ASSERT(pos < list_e->count)
    NCDVal__AssertValOnly(list.mem, list_e->elem_indices[pos]);
    
    return NCDVal__Ref(list.mem, list_e->elem_indices[pos]);
}

int NCDVal_ListRead (NCDValRef list, int num, ...)
{
    ASSERT(NCDVal_IsList(list))
    ASSERT(num >= 0)
    
    size_t count = NCDVal_ListCount(list);
    
    if (num != count) {
        return 0;
    }
    
    va_list ap;
    va_start(ap, num);
    
    for (int i = 0; i < num; i++) {
        NCDValRef *dest = va_arg(ap, NCDValRef *);
        *dest = NCDVal_ListGet(list, i);
    }
    
    va_end(ap);
    
    return 1;
}

int NCDVal_ListReadHead (NCDValRef list, int num, ...)
{
    ASSERT(NCDVal_IsList(list))
    ASSERT(num >= 0)
    
    size_t count = NCDVal_ListCount(list);
    
    if (num > count) {
        return 0;
    }
    
    va_list ap;
    va_start(ap, num);
    
    for (int i = 0; i < num; i++) {
        NCDValRef *dest = va_arg(ap, NCDValRef *);
        *dest = NCDVal_ListGet(list, i);
    }
    
    va_end(ap);
    
    return 1;
}

int NCDVal_IsMap (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return NCDVal_Type(val) == NCDVAL_MAP;
}

NCDValRef NCDVal_NewMap (NCDValMem *mem, size_t maxcount)
{
    NCDVal__AssertMem(mem);
    
    bsize_t size = bsize_add(bsize_fromsize(sizeof(struct NCDVal__map)), bsize_mul(bsize_fromsize(maxcount), bsize_fromsize(sizeof(struct NCDVal__mapelem))));
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__map));
    if (idx < 0) {
        goto fail;
    }
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(mem, idx);
    map_e->type = NCDVAL_MAP;
    map_e->maxcount = maxcount;
    map_e->count = 0;
    NCDVal__MapTree_Init(&map_e->tree);
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

int NCDVal_MapInsert (NCDValRef map, NCDValRef key, NCDValRef val)
{
    ASSERT(NCDVal_IsMap(map))
    ASSERT(NCDVal_MapCount(map) < NCDVal_MapMaxCount(map))
    ASSERT(key.mem == map.mem)
    ASSERT(val.mem == map.mem)
    NCDVal__AssertValOnly(map.mem, key.idx);
    NCDVal__AssertValOnly(map.mem, val.idx);
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    NCDVal__idx elemidx = NCDVal__MapElemIdx(map.idx, map_e->count);
    
    struct NCDVal__mapelem *me_e = NCDValMem__BufAt(map.mem, elemidx);
    ASSERT(me_e == &map_e->elems[map_e->count])
    me_e->key_idx = key.idx;
    me_e->val_idx = val.idx;
    
    int res = NCDVal__MapTree_Insert(&map_e->tree, map.mem, NCDVal__MapTreeDeref(map.mem, elemidx), NULL);
    if (!res) {
        return 0;
    }
    
    map_e->count++;
    
    return 1;
}

size_t NCDVal_MapCount (NCDValRef map)
{
    ASSERT(NCDVal_IsMap(map))
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    return map_e->count;
}

size_t NCDVal_MapMaxCount (NCDValRef map)
{
    ASSERT(NCDVal_IsMap(map))
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    return map_e->maxcount;
}

int NCDVal_MapElemInvalid (NCDValMapElem me)
{
    ASSERT(me.elemidx >= 0 || me.elemidx == -1)
    
    return me.elemidx < 0;
}

NCDValMapElem NCDVal_MapFirst (NCDValRef map)
{
    ASSERT(NCDVal_IsMap(map))
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    if (map_e->count == 0) {
        return NCDVal__MapElem(-1);
    }
    
    NCDVal__idx elemidx = NCDVal__MapElemIdx(map.idx, 0);
    NCDVal__MapAssertElemOnly(map, elemidx);
    
    return NCDVal__MapElem(elemidx);
}

NCDValMapElem NCDVal_MapNext (NCDValRef map, NCDValMapElem me)
{
    NCDVal__MapAssertElem(map, me);
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    ASSERT(map_e->count > 0)
    
    NCDVal__idx last_elemidx = NCDVal__MapElemIdx(map.idx, map_e->count - 1);
    ASSERT(me.elemidx <= last_elemidx)
    
    if (me.elemidx == last_elemidx) {
        return NCDVal__MapElem(-1);
    }
    
    NCDVal__idx elemidx = me.elemidx + sizeof(struct NCDVal__mapelem);
    NCDVal__MapAssertElemOnly(map, elemidx);
    
    return NCDVal__MapElem(elemidx);
}

NCDValMapElem NCDVal_MapOrderedFirst (NCDValRef map)
{
    ASSERT(NCDVal_IsMap(map))
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    NCDVal__MapTreeRef ref = NCDVal__MapTree_GetFirst(&map_e->tree, map.mem);
    ASSERT(ref.link == -1 || (NCDVal__MapAssertElemOnly(map, ref.link), 1))
    
    return NCDVal__MapElem(ref.link);
}

NCDValMapElem NCDVal_MapOrderedNext (NCDValRef map, NCDValMapElem me)
{
    NCDVal__MapAssertElem(map, me);
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    NCDVal__MapTreeRef ref = NCDVal__MapTree_GetNext(&map_e->tree, map.mem, NCDVal__MapTreeDeref(map.mem, me.elemidx));
    ASSERT(ref.link == -1 || (NCDVal__MapAssertElemOnly(map, ref.link), 1))
    
    return NCDVal__MapElem(ref.link);
}

NCDValRef NCDVal_MapElemKey (NCDValRef map, NCDValMapElem me)
{
    NCDVal__MapAssertElem(map, me);
    
    struct NCDVal__mapelem *me_e = NCDValMem__BufAt(map.mem, me.elemidx);
    
    return NCDVal__Ref(map.mem, me_e->key_idx);
}

NCDValRef NCDVal_MapElemVal (NCDValRef map, NCDValMapElem me)
{
    NCDVal__MapAssertElem(map, me);
    
    struct NCDVal__mapelem *me_e = NCDValMem__BufAt(map.mem, me.elemidx);
    
    return NCDVal__Ref(map.mem, me_e->val_idx);
}

NCDValMapElem NCDVal_MapFindKey (NCDValRef map, NCDValRef key)
{
    ASSERT(NCDVal_IsMap(map))
    NCDVal__AssertVal(key);
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    NCDVal__MapTreeRef ref = NCDVal__MapTree_LookupExact(&map_e->tree, map.mem, key);
    ASSERT(ref.link == -1 || (NCDVal__MapAssertElemOnly(map, ref.link), 1))
    
    return NCDVal__MapElem(ref.link);
}

NCDValRef NCDVal_MapGetValue (NCDValRef map, const char *key_str)
{
    ASSERT(NCDVal_IsMap(map))
    ASSERT(key_str)
    
    NCDValMem mem;
    mem.buf = NULL;
    mem.size = NCDVAL_FASTBUF_SIZE;
    mem.used = sizeof(struct NCDVal__externalstring);
    mem.first_ref = -1;
    
    struct NCDVal__externalstring *exs_e = (void *)mem.fastbuf;
    exs_e->type = EXTERNALSTRING_TYPE;
    exs_e->data = key_str;
    exs_e->length = strlen(key_str);
    exs_e->ref.target = NULL;
    
    NCDValRef key = NCDVal__Ref(&mem, 0);
    
    NCDValMapElem elem = NCDVal_MapFindKey(map, key);
    if (NCDVal_MapElemInvalid(elem)) {
        return NCDVal_NewInvalid();
    }
    
    return NCDVal_MapElemVal(map, elem);
}

static void replaceprog_build_recurser (NCDValMem *mem, NCDVal__idx idx, size_t *out_num_instr, NCDValReplaceProg *prog)
{
    ASSERT(idx >= 0)
    NCDVal__AssertValOnly(mem, idx);
    ASSERT(out_num_instr)
    
    *out_num_instr = 0;
    
    void *ptr = NCDValMem__BufAt(mem, idx);
    
    struct NCDVal__instr instr;
    
    switch (*((int *)(ptr))) {
        case NCDVAL_STRING:
        case IDSTRING_TYPE:
        case EXTERNALSTRING_TYPE: {
        } break;
        
        case NCDVAL_LIST: {
            struct NCDVal__list *list_e = ptr;
            
            for (NCDVal__idx i = 0; i < list_e->count; i++) {
                if (list_e->elem_indices[i] < -1) {
                    if (prog) {
                        instr.type = NCDVAL_INSTR_PLACEHOLDER;
                        instr.placeholder.plid = list_e->elem_indices[i] - NCDVAL_MINIDX;
                        instr.placeholder.plidx = idx + offsetof(struct NCDVal__list, elem_indices) + i * sizeof(NCDVal__idx);
                        prog->instrs[prog->num_instrs++] = instr;
                    }
                    (*out_num_instr)++;
                } else {
                    size_t elem_num_instr;
                    replaceprog_build_recurser(mem, list_e->elem_indices[i], &elem_num_instr, prog);
                    (*out_num_instr) += elem_num_instr;
                }
            }
        } break;
        
        case NCDVAL_MAP: {
            struct NCDVal__map *map_e = ptr;
            
            for (NCDVal__idx i = 0; i < map_e->count; i++) {
                int need_reinsert = 0;
                
                if (map_e->elems[i].key_idx < -1) {
                    if (prog) {
                        instr.type = NCDVAL_INSTR_PLACEHOLDER;
                        instr.placeholder.plid = map_e->elems[i].key_idx - NCDVAL_MINIDX;
                        instr.placeholder.plidx = idx + offsetof(struct NCDVal__map, elems) + i * sizeof(struct NCDVal__mapelem) + offsetof(struct NCDVal__mapelem, key_idx);
                        prog->instrs[prog->num_instrs++] = instr;
                    }
                    (*out_num_instr)++;
                    need_reinsert = 1;
                } else {
                    size_t key_num_instr;
                    replaceprog_build_recurser(mem, map_e->elems[i].key_idx, &key_num_instr, prog);
                    (*out_num_instr) += key_num_instr;
                    if (key_num_instr > 0) {
                        need_reinsert = 1;
                    }
                }
                
                if (map_e->elems[i].val_idx < -1) {
                    if (prog) {
                        instr.type = NCDVAL_INSTR_PLACEHOLDER;
                        instr.placeholder.plid = map_e->elems[i].val_idx - NCDVAL_MINIDX;
                        instr.placeholder.plidx = idx + offsetof(struct NCDVal__map, elems) + i * sizeof(struct NCDVal__mapelem) + offsetof(struct NCDVal__mapelem, val_idx);
                        prog->instrs[prog->num_instrs++] = instr;
                    }
                    (*out_num_instr)++;
                } else {
                    size_t val_num_instr;
                    replaceprog_build_recurser(mem, map_e->elems[i].val_idx, &val_num_instr, prog);
                    (*out_num_instr) += val_num_instr;
                }
                
                if (need_reinsert) {
                    if (prog) {
                        instr.type = NCDVAL_INSTR_REINSERT;
                        instr.reinsert.mapidx = idx;
                        instr.reinsert.elempos = i;
                        prog->instrs[prog->num_instrs++] = instr;
                    }
                    (*out_num_instr)++;
                }
            }
        } break;
        
        default: ASSERT(0);
    }
}

int NCDValReplaceProg_Init (NCDValReplaceProg *o, NCDValRef val)
{
    NCDVal__AssertVal(val);
    ASSERT(!NCDVal_IsPlaceholder(val))
    
    size_t num_instrs;
    replaceprog_build_recurser(val.mem, val.idx, &num_instrs, NULL);
    
    if (!(o->instrs = BAllocArray(num_instrs, sizeof(o->instrs[0])))) {
        BLog(BLOG_ERROR, "BAllocArray failed");
        return 0;
    }
    
    o->num_instrs = 0;
    
    size_t num_instrs2;
    replaceprog_build_recurser(val.mem, val.idx, &num_instrs2, o);
    
    ASSERT(num_instrs2 == num_instrs)
    ASSERT(o->num_instrs == num_instrs)
    
    return 1;
}

void NCDValReplaceProg_Free (NCDValReplaceProg *o)
{
    BFree(o->instrs);
}

int NCDValReplaceProg_Execute (NCDValReplaceProg prog, NCDValMem *mem, NCDVal_replace_func replace, void *arg)
{
    NCDVal__AssertMem(mem);
    ASSERT(replace)
    
    for (size_t i = 0; i < prog.num_instrs; i++) {
        struct NCDVal__instr instr = prog.instrs[i];
        
        if (instr.type == NCDVAL_INSTR_PLACEHOLDER) {
#ifndef NDEBUG
            NCDVal__idx *check_plptr = NCDValMem__BufAt(mem, instr.placeholder.plidx);
            ASSERT(*check_plptr < -1)
            ASSERT(*check_plptr - NCDVAL_MINIDX == instr.placeholder.plid)
#endif
            NCDValRef repval;
            if (!replace(arg, instr.placeholder.plid, mem, &repval) || NCDVal_IsInvalid(repval)) {
                return 0;
            }
            ASSERT(repval.mem == mem)
            
            NCDVal__idx *plptr = NCDValMem__BufAt(mem, instr.placeholder.plidx);
            *plptr = repval.idx;
        } else {
            ASSERT(instr.type == NCDVAL_INSTR_REINSERT)
            
            NCDVal__AssertValOnly(mem, instr.reinsert.mapidx);
            struct NCDVal__map *map_e = NCDValMem__BufAt(mem, instr.reinsert.mapidx);
            ASSERT(map_e->type == NCDVAL_MAP)
            ASSERT(instr.reinsert.elempos >= 0)
            ASSERT(instr.reinsert.elempos < map_e->count)
            
            NCDVal__MapTreeRef ref = {&map_e->elems[instr.reinsert.elempos], NCDVal__MapElemIdx(instr.reinsert.mapidx, instr.reinsert.elempos)};
            NCDVal__MapTree_Remove(&map_e->tree, mem, ref);
            if (!NCDVal__MapTree_Insert(&map_e->tree, mem, ref, NULL)) {
                BLog(BLOG_ERROR, "duplicate key in map");
                return 0;
            }
        }
    }
    
    return 1;
}
