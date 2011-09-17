/**
 * @file index.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * This file is part of BadVPN.
 * 
 * BadVPN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * BadVPN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * @section DESCRIPTION
 * 
 * Synopsis:
 *   index index(string value)
 *   index index::next()
 * 
 * Description:
 *   Non-negative integer with range of a size_t.
 *   The first form creates an index from the given decimal string.
 *   The second form cretes an index with value one more than an existing
 *   index.
 * 
 * Variables:
 *   string (empty) - the index value. Note this may be different from
 *     than the value given to index() if it was not in normal form.
 */

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

#include <misc/parse_number.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_index.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    size_t value;
};

static void func_new_templ (NCDModuleInst *i, size_t value)
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
    o->value = value;
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_from_value (NCDModuleInst *i)
{
    // read arguments
    NCDValue *arg_value;
    if (!NCDValue_ListRead(i->args, 1, &arg_value)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (NCDValue_Type(arg_value) != NCDVALUE_STRING) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    // parse value
    uintmax_t value;
    if (!parse_unsigned_integer(NCDValue_StringValue(arg_value), &value)) {
        ModuleLog(i, BLOG_ERROR, "wrong value");
        goto fail0;
    }
    
    // check overflow
    if (value > SIZE_MAX) {
        ModuleLog(i, BLOG_ERROR, "value too large");
        goto fail0;
    }
    
    func_new_templ(i, value);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_from_index (NCDModuleInst *i)
{
    struct instance *index = i->method_object->inst_user;
    
    // check overflow
    if (index->value == SIZE_MAX) {
        ModuleLog(i, BLOG_ERROR, "overflow");
        goto fail0;
    }
    
    func_new_templ(i, index->value + 1);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        char str[64];
        snprintf(str, sizeof(str), "%zu", o->value);
        
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "index",
        .func_new = func_new_from_value,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "index::next",
        .base_type = "index",
        .func_new = func_new_from_index,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_index = {
    .modules = modules
};
