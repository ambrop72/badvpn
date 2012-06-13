/**
 * @file concatv.c
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
    uint8_t *string;
    size_t len;
};

static void func_new (void *vo, NCDModuleInst *i)
{
    struct instance *o = vo;
    o->i = i;
    
    // read arguments
    NCDValRef strings_arg;
    if (!NCDVal_ListRead(o->i->args, 1, &strings_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsList(strings_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    // init string
    ExpString s;
    if (!ExpString_Init(&s)) {
        ModuleLog(i, BLOG_ERROR, "ExpString_Init failed");
        goto fail0;
    }
    
    // append arguments
    size_t count = NCDVal_ListCount(strings_arg);
    for (size_t j = 0; j < count; j++) {
        NCDValRef arg = NCDVal_ListGet(strings_arg, j);
        
        if (!NCDVal_IsString(arg)) {
            ModuleLog(i, BLOG_ERROR, "wrong type");
            goto fail1;
        }
        
        if (!ExpString_AppendBinary(&s, (const uint8_t *)NCDVal_StringValue(arg), NCDVal_StringLength(arg))) {
            ModuleLog(i, BLOG_ERROR, "ExpString_Append failed");
            goto fail1;
        }
    }
    
    // set string
    o->string = (uint8_t *)ExpString_Get(&s);
    o->len = ExpString_Length(&s);
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail1:
    ExpString_Free(&s);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    
    // free string
    free(o->string);
    
    NCDModuleInst_Backend_Dead(o->i);
}

static int func_getvar (void *vo, const char *name, NCDValMem *mem, NCDValRef *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        *out = NCDVal_NewStringBin(mem, o->string, o->len);
        if (NCDVal_IsInvalid(*out)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewStringBin failed");
        }
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "concatv",
        .func_new2 = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_concatv = {
    .modules = modules
};
