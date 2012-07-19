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

#include "NCDVal.h"

static void * NCDValMem__BufAt (NCDValMem *o, NCDVal__idx idx)
{
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
    ASSERT(idx >= 0 || idx == -1)
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
    const char *e_cbuf = e_buf;
    char *buf = (mem->buf ? mem->buf : mem->fastbuf);
    ASSERT(e_cbuf >= buf + mem->size || e_cbuf + e_len <= buf)
}

static void NCDVal__AssertValOnly (NCDValMem *mem, NCDVal__idx idx)
{
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
}

void NCDValMem_Free (NCDValMem *o)
{
    free(o->buf);
}

void NCDVal_Assert (NCDValRef val)
{
    ASSERT(val.idx == -1 || (NCDVal__AssertVal(val), 1))
}

int NCDVal_IsInvalid (NCDValRef val)
{
    NCDVal_Assert(val);
    
    return val.idx < 0;
}

int NCDVal_Type (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    int *type_ptr = NCDValMem__BufAt(val.mem, val.idx);
    
    return *type_ptr;
}

NCDValRef NCDVal_NewInvalid (void)
{
    NCDValRef ref = {NULL, -1};
    return ref;
}

NCDValRef NCDVal_NewCopy (NCDValMem *mem, NCDValRef val)
{
    NCDVal__AssertMem(mem);
    NCDVal__AssertVal(val);
    
    switch (NCDVal_Type(val)) {
        case NCDVAL_STRING: {
            size_t len = NCDVal_StringLength(val);
            
            NCDValRef copy = NCDVal_NewStringUninitialized(mem, len);
            if (NCDVal_IsInvalid(copy)) {
                goto fail;
            }
            
            memcpy((char *)NCDVal_StringValue(copy), NCDVal_StringValue(val), len);
            
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
                ASSERT(res)
            }
            
            return copy;
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
            
            int cmp = memcmp(NCDVal_StringValue(val1), NCDVal_StringValue(val2), min_len);
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

int NCDVal_IsStringNoNulls (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return NCDVal_Type(val) == NCDVAL_STRING && strlen(NCDVal_StringValue(val)) == NCDVal_StringLength(val);
}

NCDValRef NCDVal_NewString (NCDValMem *mem, const char *data)
{
    NCDVal__AssertMem(mem);
    ASSERT(data)
    NCDVal_AssertExternal(mem, data, strlen(data));
    
    return NCDVal_NewStringBin(mem, (const uint8_t *)data, strlen(data));
}

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

const char * NCDVal_StringValue (NCDValRef string)
{
    ASSERT(NCDVal_IsString(string))
    
    struct NCDVal__string *str_e = NCDValMem__BufAt(string.mem, string.idx);
    
    return str_e->data;
}

size_t NCDVal_StringLength (NCDValRef string)
{
    ASSERT(NCDVal_IsString(string))
    
    struct NCDVal__string *str_e = NCDValMem__BufAt(string.mem, string.idx);
    
    return str_e->length;
}

int NCDVal_StringHasNulls (NCDValRef string)
{
    ASSERT(NCDVal_IsString(string))
    
    return strlen(NCDVal_StringValue(string)) != NCDVal_StringLength(string);
}

int NCDVal_StringEquals (NCDValRef string, const char *data)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(data)
    
    return !NCDVal_StringHasNulls(string) && !strcmp(NCDVal_StringValue(string), data);
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
