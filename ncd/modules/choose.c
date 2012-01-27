/**
 * @file choose.c
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
 * Multiple value selection based on boolean conditions.
 * 
 * Synopsis:
 *   choose({{string cond1, result1}, ..., {string condN, resultN}}, default_result)
 * 
 * Variables:
 *   (empty) - If cond1="true" then result1,
 *             else if cond2="true" then result2,
 *             ...,
 *             else default_result.
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_choose.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    NCDValue *result;
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
    NCDValue *arg_choices;
    NCDValue *arg_default_result;
    if (!NCDValue_ListRead(i->args, 2, &arg_choices, &arg_default_result)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(arg_choices) != NCDVALUE_LIST) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // set no result
    o->result = NULL;
    
    // iterate choices
    for (NCDValue *c = NCDValue_ListFirst(arg_choices); c; c = NCDValue_ListNext(arg_choices, c)) {
        // check choice type
        if (NCDValue_Type(c) != NCDVALUE_LIST) {
            ModuleLog(i, BLOG_ERROR, "wrong choice type");
            goto fail1;
        }
        
        // read choice
        NCDValue *c_cond;
        NCDValue *c_result;
        if (!NCDValue_ListRead(c, 2, &c_cond, &c_result)) {
            ModuleLog(i, BLOG_ERROR, "wrong choice contents arity");
            goto fail1;
        }
        if (NCDValue_Type(c_cond) != NCDVALUE_STRING) {
            ModuleLog(i, BLOG_ERROR, "wrong choice condition type");
            goto fail1;
        }
        
        // update result
        if (!o->result && !strcmp(NCDValue_StringValue(c_cond), "true")) {
            o->result = c_result;
        }
    }
    
    // default?
    if (!o->result) {
        o->result = arg_default_result;
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
        if (!NCDValue_InitCopy(out, o->result)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "choose",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_choose = {
    .modules = modules
};
