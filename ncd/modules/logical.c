/**
 * @file logical.c
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
 * Module for logical operators.
 * 
 * Synopsis: not(string val)
 * Variables:
 *   string (empty) - "true" if val does not equal "true", "false" otherwise
 * 
 * Synopsis: or(string val1, string val2)
 * Variables:
 *   string (empty) - "true" if val1 or val2 equal "true", "false" otherwise
 * 
 * Synopsis: and(string val1, string val2)
 * Variables:
 *   string (empty) - "true" if val1 and val2 equal "true", "false" otherwise
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_logical.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    int value;
};

static void * func_new (NCDModuleInst *i, int not, int or)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    
    // init arguments
    o->i = i;
    
    if (not) {
        NCDValue *arg;
        if (!NCDValue_ListRead(o->i->args, 1, &arg)) {
            ModuleLog(o->i, BLOG_ERROR, "wrong arity");
            goto fail1;
        }
        if (NCDValue_Type(arg) != NCDVALUE_STRING) {
            ModuleLog(o->i, BLOG_ERROR, "wrong type");
            goto fail1;
        }
        
        o->value = !!strcmp(NCDValue_StringValue(arg), "true");
    } else {
        NCDValue *arg1;
        NCDValue *arg2;
        if (!NCDValue_ListRead(o->i->args, 2, &arg1, &arg2)) {
            ModuleLog(o->i, BLOG_ERROR, "wrong arity");
            goto fail1;
        }
        if (NCDValue_Type(arg1) != NCDVALUE_STRING || NCDValue_Type(arg2) != NCDVALUE_STRING) {
            ModuleLog(o->i, BLOG_ERROR, "wrong type");
            goto fail1;
        }
        
        if (or) {
            o->value = (!strcmp(NCDValue_StringValue(arg1), "true") || !strcmp(NCDValue_StringValue(arg2), "true"));
        } else {
            o->value = (!strcmp(NCDValue_StringValue(arg1), "true") && !strcmp(NCDValue_StringValue(arg2), "true"));
        }
    }
    
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    return o;
    
fail1:
    free(o);
fail0:
    return NULL;
}

static void * func_new_not (NCDModuleInst *i)
{
    return func_new(i, 1, 0);
}

static void * func_new_or (NCDModuleInst *i)
{
    return func_new(i, 0, 1);
}

static void * func_new_and (NCDModuleInst *i)
{
    return func_new(i, 0, 0);
}

static void func_free (void *vo)
{
    struct instance *o = vo;
    
    // free instance
    free(o);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        const char *v = (o->value ? "true" : "false");
        
        if (!NCDValue_InitString(out, v)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "not",
        .func_new = func_new_not,
        .func_free = func_free,
        .func_getvar = func_getvar
    }, {
        .type = "or",
        .func_new = func_new_or,
        .func_free = func_free,
        .func_getvar = func_getvar
    }, {
        .type = "and",
        .func_new = func_new_and,
        .func_free = func_free,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_logical = {
    .modules = modules
};
