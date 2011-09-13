/**
 * @file concatv.c
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
 * String concatenation module.
 * 
 * Synopsis: concatv(list(string) strings)
 * Variables:
 *   string (empty) - elements of strings concatenated
 */

#include <stdlib.h>

#include <misc/expstring.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_concatv.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    char *string;
};

static void func_new (NCDModuleInst *i)
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
    
    // read arguments
    NCDValue *strings_arg;
    if (!NCDValue_ListRead(o->i->args, 1, &strings_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(strings_arg) != NCDVALUE_LIST) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // init string
    ExpString s;
    if (!ExpString_Init(&s)) {
        ModuleLog(i, BLOG_ERROR, "ExpString_Init failed");
        goto fail1;
    }
    
    // append arguments
    NCDValue *arg = NCDValue_ListFirst(strings_arg);
    while (arg) {
        if (NCDValue_Type(arg) != NCDVALUE_STRING) {
            ModuleLog(i, BLOG_ERROR, "wrong type");
            goto fail2;
        }
        
        if (!ExpString_Append(&s, NCDValue_StringValue(arg))) {
            ModuleLog(i, BLOG_ERROR, "ExpString_Append failed");
            goto fail2;
        }
        
        arg = NCDValue_ListNext(strings_arg, arg);
    }
    
    // set string
    o->string = ExpString_Get(&s);
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    
    return;
    
fail2:
    ExpString_Free(&s);
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
    
    // free string
    free(o->string);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        if (!NCDValue_InitString(out, o->string)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "concatv",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_concatv = {
    .modules = modules
};
