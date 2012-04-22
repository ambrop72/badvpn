/**
 * @file logical.c
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
        
        o->value = !NCDValue_StringEquals(arg, "true");
    } else {
        o->value = (or ? 0 : 1);
        
        NCDValue *arg = NCDValue_ListFirst(o->i->args);
        while (arg) {
            if (NCDValue_Type(arg) != NCDVALUE_STRING) {
                ModuleLog(o->i, BLOG_ERROR, "wrong type");
                goto fail1;
            }
            
            int this_value = NCDValue_StringEquals(arg, "true");
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
