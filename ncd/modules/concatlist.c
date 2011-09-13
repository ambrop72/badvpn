/**
 * @file concatlist.c
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
 * List concatenation module.
 * 
 * Synopsis: concatlist(list elem1, ..., list elemN)
 * Variables:
 *   list (empty) - elem1, ..., elemN concatenated
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_concatlist.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
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
    
    // check arguments
    NCDValue *arg = NCDValue_ListFirst(o->i->args);
    while (arg) {
        if (NCDValue_Type(arg) != NCDVALUE_LIST) {
            ModuleLog(i, BLOG_ERROR, "wrong type");
            goto fail1;
        }
        arg = NCDValue_ListNext(o->i->args, arg);
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
        NCDValue_InitList(out);
        
        NCDValue *arg = NCDValue_ListFirst(o->i->args);
        while (arg) {
            NCDValue *val = NCDValue_ListFirst(arg);
            while (val) {
                NCDValue copy;
                if (!NCDValue_InitCopy(&copy, val)) {
                    ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
                    goto fail1;
                }
                
                if (!NCDValue_ListAppend(out, copy)) {
                    ModuleLog(o->i, BLOG_ERROR, "NCDValue_ListAppend failed");
                    NCDValue_Free(&copy);
                    goto fail1;
                }
                
                val = NCDValue_ListNext(arg, val);
            }
            
            arg = NCDValue_ListNext(o->i->args, arg);
        }
        
        return 1;
        
    fail1:
        NCDValue_Free(out);
    fail0:
        return 0;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "concatlist",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_concatlist = {
    .modules = modules
};
