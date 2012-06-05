/**
 * @file NCDValue.c
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

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>

#include <misc/debug.h>
#include <misc/offset.h>

#include <ncd/NCDValue.h>

#include "NCDValue_maptree.h"
#include <structure/CAvl_impl.h>

static int ncdvalue_comparator (void *unused, void *vv1, void *vv2)
{
    NCDValue *v1 = vv1;
    NCDValue *v2 = vv2;
    
    return NCDValue_Compare(v1, v2);
}

static void value_assert (NCDValue *o)
{
    switch (o->type) {
        case NCDVALUE_STRING:
        case NCDVALUE_LIST:
        case NCDVALUE_MAP:
        case NCDVALUE_VAR:
            return;
        default:
            ASSERT(0);
    }
}

int NCDValue_InitCopy (NCDValue *o, NCDValue *v)
{
    value_assert(v);
    
    switch (v->type) {
        case NCDVALUE_STRING: {
            return NCDValue_InitStringBin(o, v->string, v->string_len);
        } break;
        
        case NCDVALUE_LIST: {
            NCDValue_InitList(o);
            
            for (LinkedList1Node *n = LinkedList1_GetFirst(&v->list); n; n = LinkedList1Node_Next(n)) {
                NCDListElement *e = UPPER_OBJECT(n, NCDListElement, list_node);
                
                NCDValue tmp;
                if (!NCDValue_InitCopy(&tmp, &e->v)) {
                    goto fail;
                }
                
                if (!NCDValue_ListAppend(o, tmp)) {
                    NCDValue_Free(&tmp);
                    goto fail;
                }
            }
            
            return 1;
            
        fail:
            NCDValue_Free(o);
            return 0;
        } break;
        
        case NCDVALUE_MAP: {
            NCDValue_InitMap(o);
            
            for (NCDValue *ekey = NCDValue_MapFirstKey(v); ekey; ekey = NCDValue_MapNextKey(v, ekey)) {
                NCDValue *eval = NCDValue_MapKeyValue(v, ekey);
                
                NCDValue tmp_key;
                NCDValue tmp_val;
                if (!NCDValue_InitCopy(&tmp_key, ekey)) {
                    goto mapfail;
                }
                if (!NCDValue_InitCopy(&tmp_val, eval)) {
                    NCDValue_Free(&tmp_key);
                    goto mapfail;
                }
                
                if (!NCDValue_MapInsert(o, tmp_key, tmp_val)) {
                    NCDValue_Free(&tmp_key);
                    NCDValue_Free(&tmp_val);
                    goto mapfail;
                }
            }
            
            return 1;
            
        mapfail:
            NCDValue_Free(o);
            return 0;
        } break;
        
        case NCDVALUE_VAR: {
            return NCDValue_InitVar(o, v->var_name);
        } break;
        
        default:
            ASSERT(0);
    }
    
    return 0;
}

void NCDValue_Free (NCDValue *o)
{
    switch (o->type) {
        case NCDVALUE_STRING: {
            free(o->string);
        } break;
        
        case NCDVALUE_LIST: {
            LinkedList1Node *n;
            while (n = LinkedList1_GetFirst(&o->list)) {
                NCDListElement *e = UPPER_OBJECT(n, NCDListElement, list_node);
                
                NCDValue_Free(&e->v);
                LinkedList1_Remove(&o->list, &e->list_node);
                free(e);
            }
        } break;
        
        case NCDVALUE_MAP: {
            NCDValue__MapTreeNode tn;
            while ((tn = NCDValue__MapTree_GetFirst(&o->map_tree, 0)).link != NCDValue__MapTreeNullLink) {
                NCDValue__MapTree_Remove(&o->map_tree, 0, tn);
                NCDValue_Free(&tn.ptr->key);
                NCDValue_Free(&tn.ptr->val);
                free(tn.ptr);
            }
        } break;
        
        case NCDVALUE_VAR: {
            free(o->var_name);
        } break;
        
        default:
            ASSERT(0);
    }
}

int NCDValue_Type (NCDValue *o)
{
    value_assert(o);
    
    return o->type;
}

int NCDValue_IsString (NCDValue *o)
{
    value_assert(o);
    
    return o->type == NCDVALUE_STRING;
}

int NCDValue_IsStringNoNulls (NCDValue *o)
{
    return NCDValue_IsString(o) && NCDValue_StringHasNoNulls(o);
}

int NCDValue_InitString (NCDValue *o, const char *str)
{
    return NCDValue_InitStringBin(o, (const uint8_t *)str, strlen(str));
}

int NCDValue_InitStringBin (NCDValue *o, const uint8_t *str, size_t len)
{
    if (len == SIZE_MAX) {
        return 0;
    }
    
    if (!(o->string = malloc(len + 1))) {
        return 0;
    }
    
    memcpy(o->string, str, len);
    o->string[len] = '\0';
    o->string_len = len;
    
    o->type = NCDVALUE_STRING;
    
    return 1;
}

char * NCDValue_StringValue (NCDValue *o)
{
    ASSERT(o->type == NCDVALUE_STRING)
    
    return (char *)o->string;
}

size_t NCDValue_StringLength (NCDValue *o)
{
    ASSERT(o->type == NCDVALUE_STRING)
    
    return o->string_len;
}

int NCDValue_StringHasNoNulls (NCDValue *o)
{
    ASSERT(o->type == NCDVALUE_STRING)
    
    return strlen((char *)o->string) == o->string_len;
}

int NCDValue_StringHasNulls (NCDValue *o)
{
    ASSERT(o->type == NCDVALUE_STRING)
    
    return !NCDValue_StringHasNoNulls(o);
}

int NCDValue_StringEquals (NCDValue *o, const char *str)
{
    ASSERT(o->type == NCDVALUE_STRING)
    
    return NCDValue_StringHasNoNulls(o) && !strcmp((const char *)o->string, str);
}

int NCDValue_IsList (NCDValue *o)
{
    value_assert(o);
    
    return o->type == NCDVALUE_LIST;
}

void NCDValue_InitList (NCDValue *o)
{
    LinkedList1_Init(&o->list);
    o->list_count = 0;
    
    o->type = NCDVALUE_LIST;
}

int NCDValue_ListAppend (NCDValue *o, NCDValue v)
{
    value_assert(o);
    value_assert(&v);
    ASSERT(o->type == NCDVALUE_LIST)
    
    if (o->list_count == SIZE_MAX) {
        return 0;
    }
    
    NCDListElement *e = malloc(sizeof(*e));
    if (!e) {
        return 0;
    }
    
    LinkedList1_Append(&o->list, &e->list_node);
    o->list_count++;
    e->v = v;
    
    return 1;
}

int NCDValue_ListPrepend (NCDValue *o, NCDValue v)
{
    value_assert(o);
    value_assert(&v);
    ASSERT(o->type == NCDVALUE_LIST)
    
    if (o->list_count == SIZE_MAX) {
        return 0;
    }
    
    NCDListElement *e = malloc(sizeof(*e));
    if (!e) {
        return 0;
    }
    
    LinkedList1_Prepend(&o->list, &e->list_node);
    o->list_count++;
    e->v = v;
    
    return 1;
}

int NCDValue_ListAppendList (NCDValue *o, NCDValue l)
{
    value_assert(o);
    value_assert(&l);
    ASSERT(o->type == NCDVALUE_LIST)
    ASSERT(l.type == NCDVALUE_LIST)
    
    if (l.list_count > SIZE_MAX - o->list_count) {
        return 0;
    }
    
    LinkedList1Node *n;
    while (n = LinkedList1_GetFirst(&l.list)) {
        NCDListElement *e = UPPER_OBJECT(n, NCDListElement, list_node);
        LinkedList1_Remove(&l.list, &e->list_node);
        LinkedList1_Append(&o->list, &e->list_node);
    }
    
    o->list_count += l.list_count;
    
    return 1;
}

size_t NCDValue_ListCount (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    
    return o->list_count;
}

NCDValue * NCDValue_ListFirst (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    
    LinkedList1Node *ln = LinkedList1_GetFirst(&o->list);
    
    if (!ln) {
        return NULL;
    }
    
    NCDListElement *e = UPPER_OBJECT(ln, NCDListElement, list_node);
    
    return &e->v;
}

NCDValue * NCDValue_ListNext (NCDValue *o, NCDValue *ev)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    
    NCDListElement *cur_e = UPPER_OBJECT(ev, NCDListElement, v);
    LinkedList1Node *ln = LinkedList1Node_Next(&cur_e->list_node);
    
    if (!ln) {
        return NULL;
    }
    
    NCDListElement *e = UPPER_OBJECT(ln, NCDListElement, list_node);
    
    return &e->v;
}

int NCDValue_ListRead (NCDValue *o, int num, ...)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    ASSERT(num >= 0)
    
    if (num != NCDValue_ListCount(o)) {
        return 0;
    }
    
    va_list ap;
    va_start(ap, num);
    
    for (LinkedList1Node *n = LinkedList1_GetFirst(&o->list); n; n = LinkedList1Node_Next(n)) {
        NCDListElement *e = UPPER_OBJECT(n, NCDListElement, list_node);
        
        NCDValue **dest = va_arg(ap, NCDValue **);
        *dest = &e->v;
    }
    
    va_end(ap);
    
    return 1;
}

int NCDValue_ListReadHead (NCDValue *o, int num, ...)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    ASSERT(num >= 0)
    
    if (num > NCDValue_ListCount(o)) {
        return 0;
    }
    
    va_list ap;
    va_start(ap, num);
    
    LinkedList1Node *n = LinkedList1_GetFirst(&o->list);
    while (num > 0) {
        ASSERT(n)
        NCDListElement *e = UPPER_OBJECT(n, NCDListElement, list_node);
        
        NCDValue **dest = va_arg(ap, NCDValue **);
        *dest = &e->v;
        
        n = LinkedList1Node_Next(n);
        num--;
    }
    
    va_end(ap);
    
    return 1;
}

NCDValue * NCDValue_ListGet (NCDValue *o, size_t pos)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    ASSERT(pos < o->list_count)
    
    NCDValue *e = NCDValue_ListFirst(o);
    while (e) {
        if (pos == 0) {
            break;
        }
        pos--;
        e = NCDValue_ListNext(o, e);
    }
    ASSERT(e)
    
    return e;
}

NCDValue NCDValue_ListShift (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    ASSERT(o->list_count > 0)
    
    NCDListElement *e = UPPER_OBJECT(LinkedList1_GetFirst(&o->list), NCDListElement, list_node);
    
    NCDValue v = e->v;
    
    LinkedList1_Remove(&o->list, &e->list_node);
    o->list_count--;
    free(e);
    
    return v;
}

NCDValue NCDValue_ListRemove (NCDValue *o, NCDValue *ev)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_LIST)
    ASSERT(o->list_count > 0)
    
    NCDListElement *e = UPPER_OBJECT(ev, NCDListElement, v);
    
    NCDValue v = e->v;
    
    LinkedList1_Remove(&o->list, &e->list_node);
    o->list_count--;
    free(e);
    
    return v;
}

int NCDValue_IsMap (NCDValue *o)
{
    value_assert(o);
    
    return o->type == NCDVALUE_MAP;
}

void NCDValue_InitMap (NCDValue *o)
{
    o->type = NCDVALUE_MAP;
    NCDValue__MapTree_Init(&o->map_tree);
    o->map_count = 0;
}

size_t NCDValue_MapCount (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    
    return o->map_count;
}

NCDValue * NCDValue_MapFirstKey (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    
    NCDValue__MapTreeNode tn = NCDValue__MapTree_GetFirst(&o->map_tree, 0);
    if (tn.link == NCDValue__MapTreeNullLink) {
        return NULL;
    }
    
    value_assert(&tn.ptr->key);
    value_assert(&tn.ptr->val);
    
    return &tn.ptr->key;
}

NCDValue * NCDValue_MapNextKey (NCDValue *o, NCDValue *ekey)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    
    NCDMapElement *e = UPPER_OBJECT(ekey, NCDMapElement, key);
    value_assert(&e->key);
    value_assert(&e->val);
    
    NCDValue__MapTreeNode tn = NCDValue__MapTree_GetNext(&o->map_tree, 0, NCDValue__MapTree_Deref(0, e));
    if (tn.link == NCDValue__MapTreeNullLink) {
        return NULL;
    }
    
    value_assert(&tn.ptr->key);
    value_assert(&tn.ptr->val);
    
    return &tn.ptr->key;
}

NCDValue * NCDValue_MapKeyValue (NCDValue *o, NCDValue *ekey)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    
    NCDMapElement *e = UPPER_OBJECT(ekey, NCDMapElement, key);
    value_assert(&e->key);
    value_assert(&e->val);
    
    return &e->val;
}

NCDValue * NCDValue_MapFindKey (NCDValue *o, NCDValue *key)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    value_assert(key);
    
    NCDValue__MapTreeNode tn = NCDValue__MapTree_LookupExact(&o->map_tree, 0, key);
    if (tn.link == NCDValue__MapTreeNullLink) {
        return NULL;
    }
    
    value_assert(&tn.ptr->key);
    value_assert(&tn.ptr->val);
    ASSERT(!NCDValue_Compare(&tn.ptr->key, key))
    
    return &tn.ptr->key;
}

NCDValue * NCDValue_MapInsert (NCDValue *o, NCDValue key, NCDValue val)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    value_assert(&key);
    value_assert(&val);
    ASSERT(!NCDValue_MapFindKey(o, &key))
    
    if (o->map_count == SIZE_MAX) {
        return NULL;
    }
    
    NCDMapElement *e = malloc(sizeof(*e));
    if (!e) {
        return NULL;
    }
    
    e->key = key;
    e->val = val;
    int res = NCDValue__MapTree_Insert(&o->map_tree, 0, NCDValue__MapTree_Deref(0, e), NULL);
    ASSERT(res)
    
    o->map_count++;
    
    return &e->key;
}

void NCDValue_MapRemove (NCDValue *o, NCDValue *ekey, NCDValue *out_key, NCDValue *out_val)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    ASSERT(out_key)
    ASSERT(out_val)
    ASSERT(o->map_count > 0)
    
    NCDMapElement *e = UPPER_OBJECT(ekey, NCDMapElement, key);
    value_assert(&e->key);
    value_assert(&e->val);
    
    NCDValue__MapTree_Remove(&o->map_tree, 0, NCDValue__MapTree_Deref(0, e));
    
    *out_key = e->key;
    *out_val = e->val;
    
    o->map_count--;
    
    free(e);
}

NCDValue * NCDValue_MapFindValueByString (NCDValue *o, const char *key_str)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_MAP)
    ASSERT(key_str)
    
    NCDValue key;
    key.type = NCDVALUE_STRING;
    key.string = (uint8_t *)key_str;
    key.string_len = strlen(key_str);
    
    NCDValue *ekey = NCDValue_MapFindKey(o, &key);
    if (!ekey) {
        return NULL;
    }
    
    return NCDValue_MapKeyValue(o, ekey);
}

int NCDValue_IsVar (NCDValue *o)
{
    value_assert(o);
    
    return o->type == NCDVALUE_VAR;
}

int NCDValue_InitVar (NCDValue *o, const char *var_name)
{
    ASSERT(var_name)
    
    if (!(o->var_name = strdup(var_name))) {
        return 0;
    }
    
    o->type = NCDVALUE_VAR;
    
    return 1;
}

const char * NCDValue_VarName (NCDValue *o)
{
    value_assert(o);
    ASSERT(o->type == NCDVALUE_VAR)
    
    return o->var_name;
}

int NCDValue_Compare (NCDValue *o, NCDValue *v)
{
    value_assert(o);
    value_assert(v);
    
    int cmp = (o->type > v->type) - (o->type < v->type);
    if (cmp) {
        return cmp;
    }
    
    ASSERT(o->type == v->type)
    
    if (o->type == NCDVALUE_STRING) {
        size_t min_len = o->string_len < v->string_len ? o->string_len : v->string_len;
        
        int cmp = memcmp(o->string, v->string, min_len);
        if (cmp) {
            return (cmp > 0) - (cmp < 0);
        }
        
        return (o->string_len > v->string_len) - (o->string_len < v->string_len);
    }
    
    if (o->type == NCDVALUE_LIST) {
        NCDValue *x = NCDValue_ListFirst(o);
        NCDValue *y = NCDValue_ListFirst(v);
        
        while (1) {
            if (!x && y) {
                return -1;
            }
            if (x && !y) {
                return 1;
            }
            if (!x && !y) {
                return 0;
            }
            
            int res = NCDValue_Compare(x, y);
            if (res) {
                return res;
            }
            
            x = NCDValue_ListNext(o, x);
            y = NCDValue_ListNext(v, y);
        }
    }
    
    if (o->type == NCDVALUE_MAP) {
        NCDValue *key1 = NCDValue_MapFirstKey(o);
        NCDValue *key2 = NCDValue_MapFirstKey(v);
        
        while (1) {
            if (!key1 && key2) {
                return -1;
            }
            if (key1 && !key2) {
                return 1;
            }
            if (!key1 && !key2) {
                return 0;
            }
            
            int res = NCDValue_Compare(key1, key2);
            if (res) {
                return res;
            }
            
            NCDValue *val1 = NCDValue_MapKeyValue(o, key1);
            NCDValue *val2 = NCDValue_MapKeyValue(v, key2);
            
            res = NCDValue_Compare(val1, val2);
            if (res) {
                return res;
            }
            
            key1 = NCDValue_MapNextKey(o, key1);
            key2 = NCDValue_MapNextKey(v, key2);
        }
    }
    
    if (o->type == NCDVALUE_VAR) {
        int res = strcmp(o->var_name, v->var_name);
        
        return (res > 0) - (res < 0);
    }
    
    ASSERT(0)
    return 0;
}
