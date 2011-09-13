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
 * Synopsis: or([string val1, ...])
 * Variables:
 *   string (empty) - "true" if at least one of the values equals "true", "false" otherwise
 * 
 * Synopsis: and([string val1, ...])
 * Variables:
 *   string (empty) - "true" if all of the values equal "true", "false" otherwise
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

static void func_new (NCDModuleInst *i, int not, int or)
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
    
    // compute value from arguments
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
        o->value = (or ? 0 : 1);
        
        NCDValue *arg = NCDValue_ListFirst(o->i->args);
        while (arg) {
            if (NCDValue_Type(arg) != NCDVALUE_STRING) {
                ModuleLog(o->i, BLOG_ERROR, "wrong type");
                goto fail1;
            }
            
            int this_value = !strcmp(NCDValue_StringValue(arg), "true");
            if (or) {
                o->value = o->value || this_value;
            } else {
                o->value = o->value && this_value;
            }
            
            arg = NCDValue_ListNext(o->i->args, arg);
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

static void func_new_not (NCDModuleInst *i)
{
    func_new(i, 1, 0);
}

static void func_new_or (NCDModuleInst *i)
{
    func_new(i, 0, 1);
}

static void func_new_and (NCDModuleInst *i)
{
    func_new(i, 0, 0);
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
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "or",
        .func_new = func_new_or,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "and",
        .func_new = func_new_and,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_logical = {
    .modules = modules
};
