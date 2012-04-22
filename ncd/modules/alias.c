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
#include <misc/balloc.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_alias.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    char **target;
};

static char ** split_string (const char *str, char del)
{
    size_t len = strlen(str);
    
    // count parts
    size_t num_parts = 0;
    size_t i = 0;
    while (1) {
        size_t j = i;
        while (j < len && str[j] != del) j++;
        if (j == i) {
            goto fail0;
        }
        
        num_parts++;
        if (j == len) {
            break;
        }
        i = j + 1;
    }
    
    // allocate array for part pointers
    char **result = BAllocArray(num_parts + 1, sizeof(*result));
    if (!result) {
        goto fail0;
    }
    
    num_parts = 0;
    i = 0;
    while (1) {
        size_t j = i;
        while (j < len && str[j] != del) j++;
        if (j == i) {
            goto fail1;
        }
        
        if (!(result[num_parts] = malloc(j - i + 1))) {
            goto fail1;
        }
        memcpy(result[num_parts], str + i, j - i);
        result[num_parts][j - i] = '\0';
        
        num_parts++;
        if (j == len) {
            break;
        }
        i = j + 1;
    }
    
    result[num_parts] = NULL;
    
    return result;
    
fail1:
    while (num_parts-- > 0) {
        free(result[num_parts]);
    }
    BFree(result);
fail0:
    return NULL;
}

static void func_new (NCDModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // read arguments
    NCDValue *target_arg;
    if (!NCDValue_ListRead(i->args, 1, &target_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDValue_IsStringNoNulls(target_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // split target into components
    if (!(o->target = split_string(NCDValue_StringValue(target_arg), '.'))) {
        ModuleLog(o->i, BLOG_ERROR, "bad target");
        goto fail1;
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
    
    // free target
    char **str = o->target;
    while (*str) {
        free(*str);
        str++;
    }
    free(o->target);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int func_getobj (void *vo, const char *name, NCDObject *out_object)
{
    struct instance *o = vo;
    
    NCDObject object;
    if (!NCDModuleInst_Backend_GetObj(o->i, o->target[0], &object)) {
        return 0;
    }
    
    NCDObject obj2;
    if (!NCDObject_ResolveObjExpr(&object, o->target + 1, &obj2)) {
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
        .func_new = func_new,
        .func_die = func_die,
        .func_getobj = func_getobj
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_alias = {
    .modules = modules
};
