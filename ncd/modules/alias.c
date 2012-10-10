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
#include <ncd/static_strings.h>

#include <generated/blog_channel_ncd_alias.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define NUM_STATIC_NAMES 4

struct instance {
    NCDModuleInst *i;
    NCD_string_id_t *dynamic_names;
    size_t num_names;
    NCD_string_id_t static_names[NUM_STATIC_NAMES];
};

static size_t count_names (const char *str)
{
    size_t count = 1;
    
    while (*str) {
        if (*str == '.') {
            count++;
        }
        str++;
    }
    
    return count;
}

static int add_name (struct instance *o, const char *str, size_t str_len, const char *remain)
{
    ASSERT(str)
    ASSERT(!memchr(str, '\0', str_len))
    ASSERT(!!o->dynamic_names == (o->num_names > NUM_STATIC_NAMES))
    
    NCD_string_id_t id = NCDStringIndex_GetBin(o->i->params->iparams->string_index, str, str_len);
    if (id < 0) {
        return 0;
    }
    
    if (o->num_names < NUM_STATIC_NAMES) {
        o->static_names[o->num_names++] = id;
        return 1;
    }
    
    if (o->num_names == NUM_STATIC_NAMES) {
        size_t num_more = (!remain ? 0 : count_names(remain));
        size_t num_all = o->num_names + 1 + num_more;
        
        if (!(o->dynamic_names = BAllocArray(num_all, sizeof(o->dynamic_names[0])))) {
            return 0;
        }
        
        memcpy(o->dynamic_names, o->static_names, NUM_STATIC_NAMES * sizeof(o->dynamic_names[0]));
    }
    
    o->dynamic_names[o->num_names++] = id;
    
    return 1;
}

static int make_names (struct instance *o, const char *str)
{
    ASSERT(str)
    
    o->num_names = 0;
    o->dynamic_names = NULL;
    
    size_t i = 0;
    while (str[i]) {
        if (str[i] == '.') {
            if (!add_name(o, str, i, str + i + 1)) {
                goto fail;
            }
            str += i + 1;
            i = 0;
            continue;
        }
        i++;
    }
    
    if (!add_name(o, str, i, NULL)) {
        goto fail;
    }
    
    return 1;
    
fail:
    BFree(o->dynamic_names);
    return 0;
}

static void func_new (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    struct instance *o = vo;
    o->i = i;
    
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
    
    // parse name string
    if (!make_names(o, target)) {
        ModuleLog(i, BLOG_ERROR, "make_names failed");
        goto fail0;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    
    BFree(o->dynamic_names);
    
    NCDModuleInst_Backend_Dead(o->i);
}

static int func_getobj (void *vo, NCD_string_id_t name, NCDObject *out_object)
{
    struct instance *o = vo;
    ASSERT(o->num_names > 0)
    
    NCD_string_id_t *names = (o->dynamic_names ? o->dynamic_names : o->static_names);
    
    NCDObject object;
    if (!NCDModuleInst_Backend_GetObj(o->i, names[0], &object)) {
        return 0;
    }
    
    NCDObject obj2;
    if (!NCDObject_ResolveObjExprCompact(&object, names + 1, o->num_names - 1, &obj2)) {
        return 0;
    }
    
    if (name == NCD_STRING_EMPTY) {
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
        .func_getobj = func_getobj,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = NULL
    }
};

struct NCDModuleGroup ncdmodule_alias = {
    .modules = modules
};
