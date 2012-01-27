/**
 * @file alias.c
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
 *   alias(string target)
 * 
 * Variables and objects:
 *   - empty name - resolves target
 *   - nonempty name N - resolves target.N
 */

#include <stdlib.h>
#include <string.h>

#include <misc/concat_strings.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_alias.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    char *target;
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
    NCDValue *target_arg;
    if (!NCDValue_ListRead(o->i->args, 1, &target_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(target_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->target = NCDValue_StringValue(target_arg);
    
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
        return NCDModuleInst_Backend_GetVar(o->i, o->target, out);
    }
    
    char *target_name = concat_strings(3, o->target, ".", name);
    if (!target_name) {
        ModuleLog(o->i, BLOG_ERROR, "concat_strings failed");
        return 0;
    }
    
    int res = NCDModuleInst_Backend_GetVar(o->i, target_name, out);
    
    free(target_name);
    
    return res;
}

static NCDModuleInst * func_getobj (void *vo, const char *name)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        return NCDModuleInst_Backend_GetObj(o->i, o->target);
    }
    
    char *target_name = concat_strings(3, o->target, ".", name);
    if (!target_name) {
        ModuleLog(o->i, BLOG_ERROR, "concat_strings failed");
        return NULL;
    }
    
    NCDModuleInst *res = NCDModuleInst_Backend_GetObj(o->i, target_name);
    
    free(target_name);
    
    return res;
}

static const struct NCDModule modules[] = {
    {
        .type = "alias",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar,
        .func_getobj = func_getobj
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_alias = {
    .modules = modules
};
