/**
 * @file if.c
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
 * Conditional module.
 * 
 * Synopsis: if(string cond)
 * Description: on initialization, transitions to UP state if cond equals "true", else
 *      remains in the DOWN state indefinitely.
 * 
 * Synopsis: ifnot(string cond)
 * Description: on initialization, transitions to UP state if cond does not equal "true", else
 *      remains in the DOWN state indefinitely.
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_if.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
};

static void * new_templ (NCDModuleInst *i, int not)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    
    // init arguments
    o->i = i;
    
    // check arguments
    NCDValue *arg;
    if (!NCDValue_ListRead(i->args, 1, &arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    int c = !strcmp(NCDValue_StringValue(arg), "true");
    if ((not && !c) || (!not && c)) {
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    }
    
    return o;
    
fail1:
    free(o);
fail0:
    return NULL;
}

static void * func_new (NCDModuleInst *i)
{
    return new_templ(i, 0);
}

static void * func_new_not (NCDModuleInst *i)
{
    return new_templ(i, 1);
}

static void func_free (void *vo)
{
    struct instance *o = vo;
    
    // free instance
    free(o);
}

static const struct NCDModule modules[] = {
    {
        .type = "if",
        .func_new = func_new,
        .func_free = func_free
    }, {
        .type = "ifnot",
        .func_new = func_new_not,
        .func_free = func_free
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_if = {
    .modules = modules
};
