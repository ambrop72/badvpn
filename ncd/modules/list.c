/**
 * @file list.c
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
 * List construction module.
 * 
 * Synopsis:
 *   list(elem1, ..., elemN)
 *   list listfrom(list l1, ..., list lN)
 * 
 * Description:
 *   The first form creates a list with the given elements.
 *   The second form creates a list by concatenating the given
 *   lists.
 * 
 * Variables:
 *   (empty) - list containing elem1, ..., elemN
 *   length - number of elements in list
 * 
 * Synopsis: list::append(arg)
 * 
 * Synopsis: list::appendv(list arg)
 * Description: Appends the elements of arg to the list.
 * 
 * Synopsis: list::length()
 * Variables:
 *   (empty) - number of elements in list at the time of initialization
 *             of this method
 * 
 * Synopsis: list::get(string index)
 * Variables:
 *   (empty) - element of list at position index (starting from zero) at the time of initialization
 * 
 * Synopsis: list::shift()
 * 
 * Synopsis: list::contains(value)
 * Variables:
 *   (empty) - "true" if list contains value, "false" if not
 * 
 * Synopsis:
 *   list::find(start_pos, value)
 * Description:
 *   finds the first occurrence of 'value' in the list at position >='start_pos'.
 * Variables:
 *   pos - position of element, or "none" if not found
 *   found - "true" if found, "false" if not
 * 
 * Sysnopsis:
 *   list::remove_at(remove_pos)
 * Description:
 *   Removes the element at position 'remove_pos', which must refer to an existing element.
 * 
 * Synopsis:
 *   list::remove(value)
 * Description:
 *   Removes the first occurrence of value in the list.
 * 
 * Synopsis:
 *   list::set(list l1, ..., list lN)
 * Description:
 *   Replaces the list with the concatenation of given lists.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <misc/parse_number.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_list.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    NCDValue list;
};

struct append_instance {
    NCDModuleInst *i;
};

struct appendv_instance {
    NCDModuleInst *i;
};

struct length_instance {
    NCDModuleInst *i;
    size_t length;
};

struct get_instance {
    NCDModuleInst *i;
    NCDValue value;
};

struct shift_instance {
    NCDModuleInst *i;
};

struct contains_instance {
    NCDModuleInst *i;
    int contains;
};

struct find_instance {
    NCDModuleInst *i;
    int is_found;
    size_t found_pos;
};

struct removeat_instance {
    NCDModuleInst *i;
};

struct remove_instance {
    NCDModuleInst *i;
};

struct set_instance {
    NCDModuleInst *i;
};

static int append_list_args (NCDModuleInst *i, NCDValue *list, NCDValue *args)
{
    ASSERT(NCDValue_Type(list) == NCDVALUE_LIST)
    ASSERT(NCDValue_Type(args) == NCDVALUE_LIST)
    
    // append contents of list arguments
    for (NCDValue *arg = NCDValue_ListFirst(args); arg; arg = NCDValue_ListNext(args, arg)) {
        // check type
        if (NCDValue_Type(arg) != NCDVALUE_LIST) {
            ModuleLog(i, BLOG_ERROR, "wrong type");
            return 0;
        }
        
        // copy list
        NCDValue copy;
        if (!NCDValue_InitCopy(&copy, arg)) {
            ModuleLog(i, BLOG_ERROR, "NCDValue_InitCopy failed");
            return 0;
        }
        
        // append
        if (!NCDValue_ListAppendList(list, copy)) {
            ModuleLog(i, BLOG_ERROR, "NCDValue_ListAppendList failed");
            NCDValue_Free(&copy);
            return 0;
        }
    }
    
    return 1;
}

static NCDValue * find_in_list (NCDValue *list, NCDValue *val)
{
    ASSERT(NCDValue_Type(list) == NCDVALUE_LIST)
    
    for (NCDValue *e = NCDValue_ListFirst(list); e; e = NCDValue_ListNext(list, e)) {
        if (NCDValue_Compare(e, val) == 0) {
            return e;
        }
    }
    
    return NULL;
}

static void func_new_list (NCDModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // copy list
    if (!NCDValue_InitCopy(&o->list, i->args)) {
        ModuleLog(i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail1;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_listfrom (NCDModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // init list
    NCDValue_InitList(&o->list);
    
    // append contents of list arguments
    if (!append_list_args(i, &o->list, i->args)) {
        goto fail2;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail2:
    NCDValue_Free(&o->list);
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free list
    NCDValue_Free(&o->list);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        if (!NCDValue_InitCopy(out, &o->list)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
            return 0;
        }
        
        return 1;
    }
    
    if (!strcmp(name, "length")) {
        char str[64];
        snprintf(str, sizeof(str), "%zu", NCDValue_ListCount(&o->list));
        
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static void append_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct append_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    NCDValue *arg;
    if (!NCDValue_ListRead(o->i->args, 1, &arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // append
    NCDValue v;
    if (!NCDValue_InitCopy(&v, arg)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail1;
    }
    if (!NCDValue_ListAppend(&mo->list, v)) {
        NCDValue_Free(&v);
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_ListAppend failed");
        goto fail1;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void append_func_die (void *vo)
{
    struct append_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void appendv_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct appendv_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    NCDValue *arg;
    if (!NCDValue_ListRead(o->i->args, 1, &arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(arg) != NCDVALUE_LIST) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // append
    NCDValue l;
    if (!NCDValue_InitCopy(&l, arg)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail1;
    }
    if (!NCDValue_ListAppendList(&mo->list, l)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_ListAppendList failed");
        NCDValue_Free(&l);
        goto fail1;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void appendv_func_die (void *vo)
{
    struct appendv_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void length_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct length_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    if (!NCDValue_ListRead(o->i->args, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // remember length
    o->length = NCDValue_ListCount(&mo->list);
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void length_func_die (void *vo)
{
    struct length_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int length_func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct length_instance *o = vo;
    
    if (!strcmp(name, "")) {
        char str[64];
        snprintf(str, sizeof(str), "%zu", o->length);
        
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}
    
static void get_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct get_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    NCDValue *index_arg;
    if (!NCDValue_ListRead(o->i->args, 1, &index_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(index_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    uintmax_t index;
    if (!parse_unsigned_integer(NCDValue_StringValue(index_arg), &index)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong value");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // check index
    if (index >= NCDValue_ListCount(&mo->list)) {
        ModuleLog(o->i, BLOG_ERROR, "no element at index %"PRIuMAX, index);
        goto fail1;
    }
    
    // copy value
    if (!NCDValue_InitCopy(&o->value, NCDValue_ListGet(&mo->list, index))) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail1;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void get_func_die (void *vo)
{
    struct get_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free value
    NCDValue_Free(&o->value);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int get_func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct get_instance *o = vo;
    
    if (!strcmp(name, "")) {
        if (!NCDValue_InitCopy(out, &o->value)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static void shift_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct shift_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    if (!NCDValue_ListRead(o->i->args, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // shift
    if (!NCDValue_ListFirst(&mo->list)) {
        ModuleLog(o->i, BLOG_ERROR, "list has no elements");
        goto fail1;
    }
    NCDValue v = NCDValue_ListShift(&mo->list);
    NCDValue_Free(&v);
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void shift_func_die (void *vo)
{
    struct shift_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void contains_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct contains_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // read arguments
    NCDValue *value_arg;
    if (!NCDValue_ListRead(i->args, 1, &value_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // search
    o->contains = 0;
    for (NCDValue *v = NCDValue_ListFirst(&mo->list); v; v = NCDValue_ListNext(&mo->list, v)) {
        if (NCDValue_Compare(v, value_arg) == 0) {
            o->contains = 1;
            break;
        }
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void contains_func_die (void *vo)
{
    struct contains_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int contains_func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct contains_instance *o = vo;
    
    if (!strcmp(name, "")) {
        const char *value = (o->contains ? "true" : "false");
        
        if (!NCDValue_InitString(out, value)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static void find_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct find_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // read arguments
    NCDValue *start_pos_arg;
    NCDValue *value_arg;
    if (!NCDValue_ListRead(i->args, 2, &start_pos_arg, &value_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(start_pos_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // read start position
    uintmax_t start_pos;
    if (!parse_unsigned_integer(NCDValue_StringValue(start_pos_arg), &start_pos)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong start pos");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // search
    o->is_found = 0;
    size_t pos = 0;
    for (NCDValue *v = NCDValue_ListFirst(&mo->list); v; v = NCDValue_ListNext(&mo->list, v)) {
        if (pos >= start_pos && NCDValue_Compare(v, value_arg) == 0) {
            o->is_found = 1;
            o->found_pos = pos;
            break;
        }
        pos++;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void find_func_die (void *vo)
{
    struct find_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int find_func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct find_instance *o = vo;
    
    if (!strcmp(name, "pos")) {
        char value[64];
        
        if (o->is_found) {
            snprintf(value, sizeof(value), "%zu", o->found_pos);
        } else {
            snprintf(value, sizeof(value), "none");
        }
        
        if (!NCDValue_InitString(out, value)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    if (!strcmp(name, "found")) {
        const char *value = (o->is_found ? "true" : "false");
        
        if (!NCDValue_InitString(out, value)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static void removeat_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct removeat_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // read arguments
    NCDValue *remove_pos_arg;
    if (!NCDValue_ListRead(i->args, 1, &remove_pos_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(remove_pos_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // read position
    uintmax_t remove_pos;
    if (!parse_unsigned_integer(NCDValue_StringValue(remove_pos_arg), &remove_pos)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong pos");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // check position
    if (remove_pos >= NCDValue_ListCount(&mo->list)) {
        ModuleLog(o->i, BLOG_ERROR, "pos out of range");
        goto fail1;
    }
    
    // find and remove
    size_t pos = 0;
    for (NCDValue *v = NCDValue_ListFirst(&mo->list); v; v = NCDValue_ListNext(&mo->list, v)) {
        if (pos == remove_pos) {
            NCDValue removed_v = NCDValue_ListRemove(&mo->list, v);
            NCDValue_Free(&removed_v);
            break;
        }
        pos++;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void removeat_func_die (void *vo)
{
    struct removeat_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void remove_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct remove_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // read arguments
    NCDValue *value_arg;
    if (!NCDValue_ListRead(i->args, 1, &value_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // find value
    NCDValue *e = find_in_list(&mo->list, value_arg);
    if (!e) {
        ModuleLog(o->i, BLOG_ERROR, "value does not exist");
        goto fail1;
    }
    
    // remove it
    NCDValue removed_v = NCDValue_ListRemove(&mo->list, e);
    NCDValue_Free(&removed_v);
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void remove_func_die (void *vo)
{
    struct remove_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void set_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct set_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // init list
    NCDValue list;
    NCDValue_InitList(&list);
    
    // append to list
    if (!append_list_args(i, &list, i->args)) {
        goto fail2;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    // replace list
    NCDValue_Free(&mo->list);
    mo->list = list;
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail2:
    NCDValue_Free(&list);
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void set_func_die (void *vo)
{
    struct set_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static const struct NCDModule modules[] = {
    {
        .type = "list",
        .func_new = func_new_list,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "listfrom",
        .base_type = "list",
        .func_new = func_new_listfrom,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "concatlist", // alias for listfrom
        .base_type = "list",
        .func_new = func_new_listfrom,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "list::append",
        .func_new = append_func_new,
        .func_die = append_func_die
    }, {
        .type = "list::appendv",
        .func_new = appendv_func_new,
        .func_die = appendv_func_die
    }, {
        .type = "list::length",
        .func_new = length_func_new,
        .func_die = length_func_die,
        .func_getvar = length_func_getvar
    }, {
        .type = "list::get",
        .func_new = get_func_new,
        .func_die = get_func_die,
        .func_getvar = get_func_getvar
    }, {
        .type = "list::shift",
        .func_new = shift_func_new,
        .func_die = shift_func_die
    }, {
        .type = "list::contains",
        .func_new = contains_func_new,
        .func_die = contains_func_die,
        .func_getvar = contains_func_getvar
    }, {
        .type = "list::find",
        .func_new = find_func_new,
        .func_die = find_func_die,
        .func_getvar = find_func_getvar
    }, {
        .type = "list::remove_at",
        .func_new = removeat_func_new,
        .func_die = removeat_func_die
    }, {
        .type = "list::remove",
        .func_new = remove_func_new,
        .func_die = remove_func_die
    }, {
        .type = "list::set",
        .func_new = set_func_new,
        .func_die = set_func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_list = {
    .modules = modules
};
