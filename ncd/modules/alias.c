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
#include <stdint.h>
#include <limits.h>

#include <misc/debug.h>
#include <misc/balloc.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_alias.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    int num_extra_parts;
    char strings[];
};

static int split_string_inplace (char *str, char del)
{
    ASSERT(str)
    
    int num_extra_parts = 0;
    
    while (*str) {
        if (*str == del) {
            if (num_extra_parts == INT_MAX) {
                return -1;
            }
            *str = '\0';
            num_extra_parts++;
        }
        str++;
    }
    
    return num_extra_parts;
}

static void func_new (void *unused, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    // read arguments
    NCDValRef target_arg;
    if (!NCDVal_ListRead(params->args, 1, &target_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsStringNoNulls(target_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    const char *target = NCDVal_StringValue(target_arg);
    size_t target_len = strlen(target);
    
    // calculate size
    bsize_t size = bsize_add(bsize_fromsize(sizeof(struct instance)), bsize_fromsize(target_len + 1));
    
    // allocate instance
    struct instance *o = BAllocSize(size);
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // copy target
    memcpy(o->strings, target, target_len + 1);
    
    // split target into components
    if ((o->num_extra_parts = split_string_inplace(o->strings, '.')) < 0) {
        ModuleLog(o->i, BLOG_ERROR, "split_string_inplace failed");
        goto fail1;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail1:
    BFree(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    BFree(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int func_getobj (void *vo, const char *name, NCDObject *out_object)
{
    struct instance *o = vo;
    
    NCDObject object;
    if (!NCDModuleInst_Backend_GetObj(o->i, o->strings, &object)) {
        return 0;
    }
    
    NCDObject obj2;
    if (!NCDObject_ResolveObjExprCompact(&object, o->strings + strlen(o->strings) + 1, o->num_extra_parts, &obj2)) {
        return 0;
    }
    
    if (!strcmp(name, "")) {
        *out_object = obj2;
        return 1;
    }
    
    return NCDObject_GetObj(&obj2, name, out_object);
}

static const struct NCDModule modules[] = {
    {
        .type = "alias",
        .func_new2 = func_new,
        .func_die = func_die,
        .func_getobj = func_getobj
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_alias = {
    .modules = modules
};
