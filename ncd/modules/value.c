/**
 * @file value.c
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
 * Synopsis:
 *   value(value)
 *   value value::get(where)
 *   value value::try_get(where)
 *   value value::getpath(list path)
 *   value value::insert(where, what)
 * 
 * Description:
 *   Value objects allow examining and manipulating values.
 * 
 *   value(value) constructs a new value object from the given value.
 * 
 *   value::get(where) constructs a value object for the element at position 'where'
 *   (for a list), or the value corresponding to key 'where' (for a map). It is an
 *   error if the base value is not a list or a map, the index is out of bounds of
 *   the list, or the key does not exist in the map.
 *   The resulting value object is NOT a copy, and shares (part of) the same
 *   underlying value structure as the base value object. Deleting it will remove
 *   it from the list or map it is part of.
 * 
 *   value::try_get(where) is like get(), except that if any restriction on 'where'
 *   is violated, no error is triggered; instead, the value object is constructed
 *   as being deleted; this state is exposed via the 'exists' variable.
 *   This can be used to check for the presence of a key in a map, and in case it
 *   exists, allow access to the corresponding value without another get() statement.
 * 
 *   value::getpath(path) is like get(), except that it performs multiple
 *   consecutive resolutions. Also, if the path is an empty list, it performs
 *   no resulution at all.
 * 
 *   value::insert(where, what) constructs a value object by inserting into an
 *   existing value object.
 *   For lists, 'where' is the index of the element to insert before, or the length
 *   of the list to append to it.
 *   For maps, 'where' is the key to insert under. If the key already exists in the
 *   map, its value is replaced; any references to the old value however remain valid.
 * 
 * Variables:
 *   (empty) - the value stored in the value object
 *   type - type of the value; "string", "list" or "map"
 *   length - number of elements in the list or map (only if the value if a list
 *            or a map)
 *   keys - a list of keys in the map (only if the value is a map)
 *   exists - "true" or "false", reflecting whether the value object holds a value
 *            (is not in deleted state)
 * 
 * Synopsis:
 *   value::remove(where)
 *   value::delete()
 * 
 * Description:
 *   value::remove(where) removes from an existing value object.
 *   For lists, 'where' is the index of the element to remove, and must be in range.
 *   For maps, 'where' is the key to remove, and must be an existing key.
 *   In any case, any references to the removed value remain valid.
 * 
 *   value::delete() deletes the underlying value data of this value object.
 *   After delection, the value object enters a deleted state, which will cause any
 *   operation on it to fail. Any other value objects which referred to the same value
 *   or parts of it will too enter deleted state. If the value was an element
 *   in a list or map, is is removed from it.
 */

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>

#include <misc/offset.h>
#include <misc/debug.h>
#include <misc/parse_number.h>
#include <structure/LinkedList0.h>
#include <structure/IndexedList.h>
#include <structure/BCountAVL.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_value.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct value;

struct instance {
    NCDModuleInst *i;
    struct value *v;
    LinkedList0Node refs_list_node;
};

struct value {
    LinkedList0 refs_list;
    
    struct value *parent;
    union {
        struct {
            IndexedListNode list_contents_il_node;
        } list_parent;
        struct {
            NCDValue key;
            BCountAVLNode map_contents_tree_node;
        } map_parent;
    };
    
    int type;
    union {
        struct {
            char *string;
        } string;
        struct {
            IndexedList list_contents_il;
        } list;
        struct {
            BCountAVL map_contents_tree;
        } map;
    };
};

static int ncdvalue_comparator (void *unused, void *vv1, void *vv2);
static const char * get_type_str (int type);
static void value_cleanup (struct value *v);
static void value_delete (struct value *v);
static struct value * value_init_string (NCDModuleInst *i, const char *str);
static struct value * value_init_list (NCDModuleInst *i);
static size_t value_list_len (struct value *v);
static struct value * value_list_at (struct value *v, size_t index);
static int value_list_insert (NCDModuleInst *i, struct value *list, struct value *v, size_t index);
static void value_list_remove (struct value *list, struct value *v);
static struct value * value_init_map (NCDModuleInst *i);
static size_t value_map_len (struct value *map);
static struct value * value_map_at (struct value *map, size_t index);
static struct value * value_map_find (struct value *map, NCDValue *key);
static int value_map_insert (struct value *map, struct value *v, NCDValue key, NCDModuleInst *i);
static void value_map_remove (struct value *map, struct value *v);
static struct value * value_init_fromvalue (NCDModuleInst *i, NCDValue *value);
static int value_to_value (NCDModuleInst *i, struct value *v, NCDValue *out_value);
static struct value * value_get (NCDModuleInst *i, struct value *v, NCDValue *where, int no_error);
static struct value * value_get_path (NCDModuleInst *i, struct value *v, NCDValue *path);
static struct value * value_insert (NCDModuleInst *i, struct value *v, NCDValue *where, NCDValue *what);
static int value_remove (NCDModuleInst *i, struct value *v, NCDValue *where);

static int ncdvalue_comparator (void *unused, void *vv1, void *vv2)
{
    NCDValue *v1 = vv1;
    NCDValue *v2 = vv2;
    
    return NCDValue_Compare(v1, v2);
}

static const char * get_type_str (int type)
{
    switch (type) {
        case NCDVALUE_STRING: return "string";
        case NCDVALUE_LIST: return "list";
        case NCDVALUE_MAP: return "map";
    }
    ASSERT(0)
    return NULL;
}

static void value_cleanup (struct value *v)
{
    if (v->parent || !LinkedList0_IsEmpty(&v->refs_list)) {
        return;
    }
    
    switch (v->type) {
        case NCDVALUE_STRING: {
            free(v->string.string);
        } break;
        
        case NCDVALUE_LIST: {
            while (value_list_len(v) > 0) {
                struct value *ev = value_list_at(v, 0);
                value_list_remove(v, ev);
                value_cleanup(ev);
            }
        } break;
        
        case NCDVALUE_MAP: {
            while (value_map_len(v) > 0) {
                struct value *ev = value_map_at(v, 0);
                value_map_remove(v, ev);
                value_cleanup(ev);
            }
        } break;
        
        default: ASSERT(0);
    }
    
    free(v);
}

static void value_delete (struct value *v)
{
    if (v->parent) {
        switch (v->parent->type) {
            case NCDVALUE_LIST: {
                value_list_remove(v->parent, v);
            } break;
            case NCDVALUE_MAP: {
                value_map_remove(v->parent, v);
            } break;
            default: ASSERT(0);
        }
    }
    
    LinkedList0Node *ln;
    while (ln = LinkedList0_GetFirst(&v->refs_list)) {
        struct instance *inst = UPPER_OBJECT(ln, struct instance, refs_list_node);
        ASSERT(inst->v == v)
        LinkedList0_Remove(&v->refs_list, &inst->refs_list_node);
        inst->v = NULL;
    }
    
    switch (v->type) {
        case NCDVALUE_STRING: {
            free(v->string.string);
        } break;
        
        case NCDVALUE_LIST: {
            while (value_list_len(v) > 0) {
                struct value *ev = value_list_at(v, 0);
                value_delete(ev);
            }
        } break;
        
        case NCDVALUE_MAP: {
            while (value_map_len(v) > 0) {
                struct value *ev = value_map_at(v, 0);
                value_delete(ev);
            }
        } break;
        
        default: ASSERT(0);
    }
    
    free(v);
}

static struct value * value_init_string (NCDModuleInst *i, const char *str)
{
    struct value *v = malloc(sizeof(*v));
    if (!v) {
        ModuleLog(i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    
    LinkedList0_Init(&v->refs_list);
    v->parent = NULL;
    v->type = NCDVALUE_STRING;
    
    if (!(v->string.string = strdup(str))) {
        ModuleLog(i, BLOG_ERROR, "strdup failed");
        goto fail1;
    }
    
    return v;
    
fail1:
    free(v);
fail0:
    return NULL;
}

static struct value * value_init_list (NCDModuleInst *i)
{
    struct value *v = malloc(sizeof(*v));
    if (!v) {
        ModuleLog(i, BLOG_ERROR, "malloc failed");
        return NULL;
    }
    
    LinkedList0_Init(&v->refs_list);
    v->parent = NULL;
    v->type = NCDVALUE_LIST;
    
    IndexedList_Init(&v->list.list_contents_il);
    
    return v;
}

static size_t value_list_len (struct value *v)
{
    ASSERT(v->type == NCDVALUE_LIST)
    
    return IndexedList_Count(&v->list.list_contents_il);
}

static struct value * value_list_at (struct value *v, size_t index)
{
    ASSERT(v->type == NCDVALUE_LIST)
    ASSERT(index < value_list_len(v))
    
    IndexedListNode *iln = IndexedList_GetAt(&v->list.list_contents_il, index);
    ASSERT(iln)
    
    struct value *e = UPPER_OBJECT(iln, struct value, list_parent.list_contents_il_node);
    ASSERT(e->parent == v)
    
    return e;
}

static int value_list_insert (NCDModuleInst *i, struct value *list, struct value *v, size_t index)
{
    ASSERT(list->type == NCDVALUE_LIST)
    ASSERT(!v->parent)
    ASSERT(index <= value_list_len(list))
    
    if (value_list_len(list) == SIZE_MAX) {
        ModuleLog(i, BLOG_ERROR, "list has too many elements");
        return 0;
    }
    
    IndexedList_InsertAt(&list->list.list_contents_il, &v->list_parent.list_contents_il_node, index);
    v->parent = list;
    
    return 1;
}

static void value_list_remove (struct value *list, struct value *v)
{
    ASSERT(list->type == NCDVALUE_LIST)
    ASSERT(v->parent == list)
    
    IndexedList_Remove(&list->list.list_contents_il, &v->list_parent.list_contents_il_node);
    v->parent = NULL;
}

static struct value * value_init_map (NCDModuleInst *i)
{
    struct value *v = malloc(sizeof(*v));
    if (!v) {
        ModuleLog(i, BLOG_ERROR, "malloc failed");
        return NULL;
    }
    
    LinkedList0_Init(&v->refs_list);
    v->parent = NULL;
    v->type = NCDVALUE_MAP;
    
    BCountAVL_Init(&v->map.map_contents_tree, OFFSET_DIFF(struct value, map_parent.key, map_parent.map_contents_tree_node), ncdvalue_comparator, NULL);
    
    return v;
}

static size_t value_map_len (struct value *map)
{
    ASSERT(map->type == NCDVALUE_MAP)
    
    return BCountAVL_Count(&map->map.map_contents_tree);
}

static struct value * value_map_at (struct value *map, size_t index)
{
    ASSERT(map->type == NCDVALUE_MAP)
    ASSERT(index < value_map_len(map))
    
    BCountAVLNode *tn = BCountAVL_GetAt(&map->map.map_contents_tree, index);
    ASSERT(tn)
    
    struct value *e = UPPER_OBJECT(tn, struct value, map_parent.map_contents_tree_node);
    ASSERT(e->parent == map)
    
    return e;
}

static struct value * value_map_find (struct value *map, NCDValue *key)
{
    ASSERT(map->type == NCDVALUE_MAP)
    ASSERT(key)
    
    BCountAVLNode *tn = BCountAVL_LookupExact(&map->map.map_contents_tree, key);
    if (!tn) {
        return NULL;
    }
    
    struct value *e = UPPER_OBJECT(tn, struct value, map_parent.map_contents_tree_node);
    ASSERT(e->parent == map)
    
    return e;
}

static int value_map_insert (struct value *map, struct value *v, NCDValue key, NCDModuleInst *i)
{
    ASSERT(map->type == NCDVALUE_MAP)
    ASSERT(!v->parent)
    ASSERT(!value_map_find(map, &key))
    
    if (value_map_len(map) == SIZE_MAX) {
        ModuleLog(i, BLOG_ERROR, "map has too many elements");
        return 0;
    }
    
    v->map_parent.key = key;
    int res = BCountAVL_Insert(&map->map.map_contents_tree, &v->map_parent.map_contents_tree_node, NULL);
    ASSERT(res)
    v->parent = map;
    
    return 1;
}

static void value_map_remove (struct value *map, struct value *v)
{
    ASSERT(map->type == NCDVALUE_MAP)
    ASSERT(v->parent == map)
    
    BCountAVL_Remove(&map->map.map_contents_tree, &v->map_parent.map_contents_tree_node);
    NCDValue_Free(&v->map_parent.key);
    v->parent = NULL;
}

static struct value * value_init_fromvalue (NCDModuleInst *i, NCDValue *value)
{
    struct value *v;
    
    switch (NCDValue_Type(value)) {
        case NCDVALUE_STRING: {
            if (!(v = value_init_string(i, NCDValue_StringValue(value)))) {
                goto fail0;
            }
        } break;
        
        case NCDVALUE_LIST: {
            if (!(v = value_init_list(i))) {
                goto fail0;
            }
            
            for (NCDValue *eval = NCDValue_ListFirst(value); eval; eval = NCDValue_ListNext(value, eval)) {
                struct value *ev = value_init_fromvalue(i, eval);
                if (!ev) {
                    goto fail1;
                }
                
                if (!value_list_insert(i, v, ev, value_list_len(v))) {
                    value_cleanup(ev);
                    goto fail1;
                }
            }
        } break;
        
        case NCDVALUE_MAP: {
            if (!(v = value_init_map(i))) {
                goto fail0;
            }
            
            for (NCDValue *ekey = NCDValue_MapFirstKey(value); ekey; ekey = NCDValue_MapNextKey(value, ekey)) {
                NCDValue *eval = NCDValue_MapKeyValue(value, ekey);
                
                NCDValue key;
                if (!NCDValue_InitCopy(&key, ekey)) {
                    BLog(BLOG_ERROR, "NCDValue_InitCopy failed");
                    goto fail1;
                }
                
                struct value *ev = value_init_fromvalue(i, eval);
                if (!ev) {
                    NCDValue_Free(&key);
                    goto fail1;
                }
                
                if (!value_map_insert(v, ev, key, i)) {
                    NCDValue_Free(&key);
                    value_cleanup(ev);
                    goto fail1;
                }
            }
        } break;
        
        default: ASSERT(0);
    }
    
    return v;
    
fail1:
    value_cleanup(v);
fail0:
    return NULL;
}

static int value_to_value (NCDModuleInst *i, struct value *v, NCDValue *out_value)
{
    switch (v->type) {
        case NCDVALUE_STRING: {
            if (!(NCDValue_InitString(out_value, v->string.string))) {
                ModuleLog(i, BLOG_ERROR, "NCDValue_InitString failed");
                goto fail0;
            }
        } break;
        
        case NCDVALUE_LIST: {
            NCDValue_InitList(out_value);
            
            for (size_t index = 0; index < value_list_len(v); index++) {
                NCDValue eval;
                if (!value_to_value(i, value_list_at(v, index), &eval)) {
                    goto fail1;
                }
                
                if (!NCDValue_ListAppend(out_value, eval)) {
                    ModuleLog(i, BLOG_ERROR, "NCDValue_ListAppend failed");
                    NCDValue_Free(&eval);
                    goto fail1;
                }
            }
        } break;
        
        case NCDVALUE_MAP: {
            NCDValue_InitMap(out_value);
            
            for (size_t index = 0; index < value_map_len(v); index++) {
                struct value *ev = value_map_at(v, index);
                
                NCDValue key;
                NCDValue val;
                
                if (!NCDValue_InitCopy(&key, &ev->map_parent.key)) {
                    ModuleLog(i, BLOG_ERROR, "NCDValue_InitCopy failed");
                    goto fail1;
                }
                
                if (!value_to_value(i, ev, &val)) {
                    NCDValue_Free(&key);
                    goto fail1;
                }
                
                if (!NCDValue_MapInsert(out_value, key, val)) {
                    ModuleLog(i, BLOG_ERROR, "NCDValue_MapInsert failed");
                    NCDValue_Free(&key);
                    NCDValue_Free(&val);
                    goto fail1;
                }
            }
        } break;
        
        default: ASSERT(0);
    }
    
    return 1;
    
fail1:
    NCDValue_Free(out_value);
fail0:
    return 0;
}

static struct value * value_get (NCDModuleInst *i, struct value *v, NCDValue *where, int no_error)
{
    switch (v->type) {
        case NCDVALUE_STRING: {
            if (!no_error) ModuleLog(i, BLOG_ERROR, "cannot resolve into a string");
            goto fail;
        } break;
        
        case NCDVALUE_LIST: {
            if (NCDValue_Type(where) != NCDVALUE_STRING) {
                if (!no_error) ModuleLog(i, BLOG_ERROR, "index is not a string (resolving into list)");
                goto fail;
            }
            
            uintmax_t index;
            if (!parse_unsigned_integer(NCDValue_StringValue(where), &index)) {
                if (!no_error) ModuleLog(i, BLOG_ERROR, "index is not a valid number (resolving into list)");
                goto fail;
            }
            
            if (index >= value_list_len(v)) {
                if (!no_error) ModuleLog(i, BLOG_ERROR, "index is out of bounds (resolving into list)");
                goto fail;
            }
            
            v = value_list_at(v, index);
        } break;
        
        case NCDVALUE_MAP: {
            v = value_map_find(v, where);
            if (!v) {
                if (!no_error) ModuleLog(i, BLOG_ERROR, "key does not exist (resolving into map)");
                goto fail;
            }
        } break;
        
        default: ASSERT(0);
    }
    
    return v;
    
fail:
    return NULL;
}

static struct value * value_get_path (NCDModuleInst *i, struct value *v, NCDValue *path)
{
    ASSERT(NCDValue_Type(path) == NCDVALUE_LIST)
    
    for (NCDValue *ev = NCDValue_ListFirst(path); ev; ev = NCDValue_ListNext(path, ev)) {
        if (!(v = value_get(i, v, ev, 0))) {
            goto fail;
        }
    }
    
    return v;
    
fail:
    return NULL;
}

static struct value * value_insert (NCDModuleInst *i, struct value *v, NCDValue *where, NCDValue *what)
{
    struct value *nv;
    
    switch (v->type) {
        case NCDVALUE_STRING: {
            ModuleLog(i, BLOG_ERROR, "cannot insert into a string");
            goto fail;
        } break;
        
        case NCDVALUE_LIST: {
            if (NCDValue_Type(where) != NCDVALUE_STRING) {
                ModuleLog(i, BLOG_ERROR, "index is not a string (inserting into list)");
                goto fail;
            }
            
            uintmax_t index;
            if (!parse_unsigned_integer(NCDValue_StringValue(where), &index)) {
                ModuleLog(i, BLOG_ERROR, "index is not a valid number (inserting into list)");
                goto fail;
            }
            
            if (index > value_list_len(v)) {
                ModuleLog(i, BLOG_ERROR, "index is out of bounds (inserting into list)");
                goto fail;
            }
            
            nv = value_init_fromvalue(i, what);
            if (!nv) {
                goto fail;
            }
            
            if (!value_list_insert(i, v, nv, index)) {
                value_cleanup(nv);
                goto fail;
            }
        } break;
        
        case NCDVALUE_MAP: {
            struct value *ov = value_map_find(v, where);
            
            if (!ov && value_map_len(v) == SIZE_MAX) {
                ModuleLog(i, BLOG_ERROR, "map has too many elements");
                goto fail;
            }
            
            NCDValue key;
            if (!NCDValue_InitCopy(&key, where)) {
                ModuleLog(i, BLOG_ERROR, "NCDValue_InitCopy failed");
                goto fail;
            }
            
            nv = value_init_fromvalue(i, what);
            if (!nv) {
                NCDValue_Free(&key);
                goto fail;
            }
            
            if (ov) {
                value_map_remove(v, ov);
                value_cleanup(ov);
            }
            
            int res = value_map_insert(v, nv, key, i);
            ASSERT(res)
        } break;
        
        default: ASSERT(0);
    }
    
    return nv;
    
fail:
    return NULL;
}

static int value_remove (NCDModuleInst *i, struct value *v, NCDValue *where)
{
    switch (v->type) {
        case NCDVALUE_STRING: {
            ModuleLog(i, BLOG_ERROR, "cannot remove from a string");
            goto fail;
        } break;
        
        case NCDVALUE_LIST: {
            if (NCDValue_Type(where) != NCDVALUE_STRING) {
                ModuleLog(i, BLOG_ERROR, "index is not a string (removing from list)");
                goto fail;
            }
            
            uintmax_t index;
            if (!parse_unsigned_integer(NCDValue_StringValue(where), &index)) {
                ModuleLog(i, BLOG_ERROR, "index is not a valid number (removing from list)");
                goto fail;
            }
            
            if (index >= value_list_len(v)) {
                ModuleLog(i, BLOG_ERROR, "index is out of bounds (removing from list)");
                goto fail;
            }
            
            struct value *ov = value_list_at(v, index);
            
            value_list_remove(v, ov);
            value_cleanup(ov);
        } break;
        
        case NCDVALUE_MAP: {
            struct value *ov = value_map_find(v, where);
            if (!ov) {
                ModuleLog(i, BLOG_ERROR, "key does not exist (removing from map)");
                goto fail;
            }
            
            value_map_remove(v, ov);
            value_cleanup(ov);
        } break;
        
        default: ASSERT(0);
    }
    
    return 1;
    
fail:
    return 0;
}

static void func_new_common (NCDModuleInst *i, struct value *v)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // set value
    o->v = v;
    
    if (v) {
        // add reference
        LinkedList0_Prepend(&o->v->refs_list, &o->refs_list_node);
    }
    
    NCDModuleInst_Backend_Up(i);
    return;
    
fail1:
    free(o);
fail0:
    value_cleanup(v);
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    NCDModuleInst *i = o->i;
    
    if (o->v) {
        // remove reference
        LinkedList0_Remove(&o->v->refs_list, &o->refs_list_node);
        
        // cleanup after removing reference
        value_cleanup(o->v);
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int func_getvar (void *vo, const char *name, NCDValue *out_value)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "exists")) {
        const char *str = o->v ? "true" : "false";
        if (!NCDValue_InitString(out_value, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        return 1;
    }
    
    if (strcmp(name, "type") && strcmp(name, "length") && strcmp(name, "keys") && strcmp(name, "")) {
        return 0;
    }
    
    if (!o->v) {
        ModuleLog(o->i, BLOG_ERROR, "value was deleted");
        return 0;
    }
    
    if (!strcmp(name, "type")) {
        if (!NCDValue_InitString(out_value, get_type_str(o->v->type))) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
    }
    else if (!strcmp(name, "length")) {
        size_t len;
        switch (o->v->type) {
            case NCDVALUE_LIST:
                len = value_list_len(o->v);
                break;
            case NCDVALUE_MAP:
                len = value_map_len(o->v);
                break;
            default:
                ModuleLog(o->i, BLOG_ERROR, "value is not a list or map");
                return 0;
        }
        
        char str[64];
        snprintf(str, sizeof(str), "%zu", len);
        if (!NCDValue_InitString(out_value, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
    }
    else if (!strcmp(name, "keys")) {
        if (o->v->type != NCDVALUE_MAP) {
            ModuleLog(o->i, BLOG_ERROR, "value is not a map (reading keys variable)");
            return 0;
        }
        
        NCDValue_InitList(out_value);
        
        for (size_t i = 0; i < value_map_len(o->v); i++) {
            struct value *ev = value_map_at(o->v, i);
            
            NCDValue key;
            if (!NCDValue_InitCopy(&key, &ev->map_parent.key)) {
                ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
                goto map_fail1;
            }
            
            if (!NCDValue_ListAppend(out_value, key)) {
                ModuleLog(o->i, BLOG_ERROR, "NCDValue_ListAppend failed");
                NCDValue_Free(&key);
                goto map_fail1;
            }
        }
        
        return 1;
        
    map_fail1:
        NCDValue_Free(out_value);
        return 0;
    }
    else if (!strcmp(name, "")) {
        if (!value_to_value(o->i, o->v, out_value)) {
            return 0;
        }
    }
    else {
        ASSERT(0);
    }
    
    return 1;
}

static void func_new_value (NCDModuleInst *i)
{
    NCDValue *value_arg;
    if (!NCDValue_ListRead(i->args, 1, &value_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    
    struct value *v = value_init_fromvalue(i, value_arg);
    if (!v) {
        goto fail0;
    }
    
    func_new_common(i, v);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_get (NCDModuleInst *i)
{
    NCDValue *where_arg;
    if (!NCDValue_ListRead(i->args, 1, &where_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    
    struct instance *mo = ((NCDModuleInst *)i->method_user)->inst_user;
    
    if (!mo->v) {
        ModuleLog(i, BLOG_ERROR, "value was deleted");
        goto fail0;
    }
    
    struct value *v = value_get(i, mo->v, where_arg, 0);
    if (!v) {
        goto fail0;
    }
    
    func_new_common(i, v);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_try_get (NCDModuleInst *i)
{
    NCDValue *where_arg;
    if (!NCDValue_ListRead(i->args, 1, &where_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    
    struct instance *mo = ((NCDModuleInst *)i->method_user)->inst_user;
    
    if (!mo->v) {
        ModuleLog(i, BLOG_ERROR, "value was deleted");
        goto fail0;
    }
    
    struct value *v = value_get(i, mo->v, where_arg, 1);
    
    func_new_common(i, v);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_getpath (NCDModuleInst *i)
{
    NCDValue *path_arg;
    if (!NCDValue_ListRead(i->args, 1, &path_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (NCDValue_Type(path_arg) != NCDVALUE_LIST) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    struct instance *mo = ((NCDModuleInst *)i->method_user)->inst_user;
    
    if (!mo->v) {
        ModuleLog(i, BLOG_ERROR, "value was deleted");
        goto fail0;
    }
    
    struct value *v = value_get_path(i, mo->v, path_arg);
    if (!v) {
        goto fail0;
    }
    
    func_new_common(i, v);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_insert (NCDModuleInst *i)
{
    NCDValue *where_arg;
    NCDValue *what_arg;
    if (!NCDValue_ListRead(i->args, 2, &where_arg, &what_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    
    struct instance *mo = ((NCDModuleInst *)i->method_user)->inst_user;
    
    if (!mo->v) {
        ModuleLog(i, BLOG_ERROR, "value was deleted");
        goto fail0;
    }
    
    struct value *v = value_insert(i, mo->v, where_arg, what_arg);
    if (!v) {
        goto fail0;
    }
    
    func_new_common(i, v);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void remove_func_new (NCDModuleInst *i)
{
    NCDValue *where_arg;
    if (!NCDValue_ListRead(i->args, 1, &where_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    
    struct instance *mo = ((NCDModuleInst *)i->method_user)->inst_user;
    
    if (!mo->v) {
        ModuleLog(i, BLOG_ERROR, "value was deleted");
        goto fail0;
    }
    
    if (!value_remove(i, mo->v, where_arg)) {
        goto fail0;
    }
    
    NCDModuleInst_Backend_Up(i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void delete_func_new (NCDModuleInst *i)
{
    if (!NCDValue_ListRead(i->args, 0)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    
    struct instance *mo = ((NCDModuleInst *)i->method_user)->inst_user;
    
    if (!mo->v) {
        ModuleLog(i, BLOG_ERROR, "value was deleted");
        goto fail0;
    }
    
    value_delete(mo->v);
    
    NCDModuleInst_Backend_Up(i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static const struct NCDModule modules[] = {
    {
        .type = "value",
        .func_new = func_new_value,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "value::get",
        .base_type = "value",
        .func_new = func_new_get,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "value::try_get",
        .base_type = "value",
        .func_new = func_new_try_get,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "value::getpath",
        .base_type = "value",
        .func_new = func_new_getpath,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "value::insert",
        .base_type = "value",
        .func_new = func_new_insert,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "value::remove",
        .func_new = remove_func_new
    }, {
        .type = "value::delete",
        .func_new = delete_func_new
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_value = {
    .modules = modules
};
