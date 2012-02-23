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
 *   value value::get(string index)
 *   value value::getpath(list(string) path)
 * 
 * Description:
 *   Value objects allow examining and manipulating values.
 * 
 *   value(value) constructs a value object from the given value.
 * 
 *   value::get(index) constructs a value object from the list element
 *   of the value, which must be a list. This it *not* a copy, and the
 *   two value objects will share the same value data (more correctly, different
 *   portions of it).
 * 
 *   value::getpath(path) is like get(), except that it performs multiple
 *   consecutive resolutions. Also, if the path is an empty list, it performs
 *   no resulution at all.
 * 
 * Variables:
 *   type - type of the value; "string" or "list"
 *   length - length of the list (only if the value if a list)
 * 
 * Synopsis:
 *   value::delete()
 * 
 * Description:
 *   Deletes the underlying value data of this value object. After delection,
 *   the value object enters a deleted state, which will cause any operation
 *   on it to fail. Any other value objects which were sharing the deleted
 *   value data or portions of it will too enter deleted state.
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
    };
    
    int type;
    union {
        struct {
            char *string;
        } string;
        struct {
            IndexedList list_contents_il;
        } list;
    };
};

static const char * get_type_str (int type);
static void value_cleanup (struct value *v);
static void value_delete (struct value *v);
static struct value * value_init_string (NCDModuleInst *i, const char *str);
static struct value * value_init_list (NCDModuleInst *i);
static size_t value_list_len (struct value *v);
static struct value * value_list_at (struct value *v, size_t index);
static int value_list_insert (NCDModuleInst *i, struct value *list, struct value *v, size_t index);
static void value_list_remove (struct value *list, struct value *v);
static struct value * value_init_fromvalue (NCDModuleInst *i, NCDValue *value);
static int value_to_value (NCDModuleInst *i, struct value *v, NCDValue *out_value);
static struct value * value_get (NCDModuleInst *i, struct value *v, const char *index_str);
static struct value * value_get_path (NCDModuleInst *i, struct value *v, NCDValue *path);

static const char * get_type_str (int type)
{
    switch (type) {
        case NCDVALUE_STRING: return "string";
        case NCDVALUE_LIST: return "list";
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
        
        default: ASSERT(0);
    }
    
    free(v);
}

static void value_delete (struct value *v)
{
    if (v->parent) {
        ASSERT(v->parent->type == NCDVALUE_LIST)
        value_list_remove(v->parent, v);
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
                return 0;
            }
            
            return 1;
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
            
            return 1;
            
        fail1:
            NCDValue_Free(out_value);
            return 0;
        } break;
        
        default: ASSERT(0);
    }
}

static struct value * value_get (NCDModuleInst *i, struct value *v, const char *index_str)
{
    ASSERT(index_str)
    
    switch (v->type) {
        case NCDVALUE_STRING: {
            ModuleLog(i, BLOG_ERROR, "cannot resolve into a string");
            goto fail;
        } break;
        
        case NCDVALUE_LIST: {
            uintmax_t index;
            if (!parse_unsigned_integer(index_str, &index)) {
                ModuleLog(i, BLOG_ERROR, "index string is not a valid number (resolving into list)");
                goto fail;
            }
            
            if (index >= value_list_len(v)) {
                ModuleLog(i, BLOG_ERROR, "index string is out of bounds (resolving into list)");
                goto fail;
            }
            
            v = value_list_at(v, index);
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
        if (NCDValue_Type(ev) != NCDVALUE_STRING) {
            ModuleLog(i, BLOG_ERROR, "path component is not a string");
            goto fail;
        }
        
        if (!(v = value_get(i, v, NCDValue_StringValue(ev)))) {
            goto fail;
        }
    }
    
    return v;
    
fail:
    return NULL;
}

static void func_new_common (NCDModuleInst *i, struct value *v)
{
    ASSERT(v)
    
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
    
    // add reference
    LinkedList0_Prepend(&o->v->refs_list, &o->refs_list_node);
    
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
    
    if (strcmp(name, "type") && strcmp(name, "length") && strcmp(name, "")) {
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
        if (o->v->type != NCDVALUE_LIST) {
            ModuleLog(o->i, BLOG_ERROR, "value is not a list");
            return 0;
        }
        char str[64];
        snprintf(str, sizeof(str), "%zu", value_list_len(o->v));
        if (!NCDValue_InitString(out_value, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
    }
    else if (!strcmp(name, "")) {
        if (!value_to_value(o->i, o->v, out_value)) {
            return 0;
        }
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
    NCDValue *index_arg;
    if (!NCDValue_ListRead(i->args, 1, &index_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (NCDValue_Type(index_arg) != NCDVALUE_STRING) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    struct instance *mo = ((NCDModuleInst *)i->method_user)->inst_user;
    
    if (!mo->v) {
        ModuleLog(i, BLOG_ERROR, "value was deleted");
        goto fail0;
    }
    
    struct value *v = value_get(i, mo->v, NCDValue_StringValue(index_arg));
    if (!v) {
        goto fail0;
    }
    
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
        .type = "value::getpath",
        .base_type = "value",
        .func_new = func_new_getpath,
        .func_die = func_die,
        .func_getvar = func_getvar
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
