/**
 * @file implode.c
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
 *   implode(string glue, list(string) pieces)
 * 
 * Variables:
 *   string (empty) - concatenation of strings in 'pieces', with 'glue' in between
 *                    every two elements.
 */

#include <stdlib.h>
#include <string.h>

#include <misc/expstring.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_implode.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    char *result;
    size_t result_len;
};

static void func_new (NCDModuleInst *i)
{
    // allocate structure
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // read arguments
    NCDValRef glue_arg;
    NCDValRef pieces_arg;
    if (!NCDVal_ListRead(i->args, 2, &glue_arg, &pieces_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDVal_IsString(glue_arg) || !NCDVal_IsList(pieces_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // init result string
    ExpString str;
    if (!ExpString_Init(&str)) {
        ModuleLog(i, BLOG_ERROR, "ExpString_Init failed");
        goto fail1;
    }
    
    size_t count = NCDVal_ListCount(pieces_arg);
    for (size_t j = 0; j < count; j++) {
        NCDValRef piece = NCDVal_ListGet(pieces_arg, j);
        
        // check piece type
        if (!NCDVal_IsString(piece)) {
            ModuleLog(i, BLOG_ERROR, "wrong piece type");
            goto fail2;
        }
        
        // append glue
        if (j > 0) {
            if (!ExpString_AppendBinary(&str, (const uint8_t *)NCDVal_StringValue(glue_arg), NCDVal_StringLength(glue_arg))) {
                ModuleLog(i, BLOG_ERROR, "ExpString_AppendBinary failed");
                goto fail2;
            }
        }
        
        // append piece
        if (!ExpString_AppendBinary(&str, (const uint8_t *)NCDVal_StringValue(piece), NCDVal_StringLength(piece))) {
            ModuleLog(i, BLOG_ERROR, "ExpString_AppendBinary failed");
            goto fail2;
        }
    }
    
    // store result
    o->result = ExpString_Get(&str);
    o->result_len = ExpString_Length(&str);
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail2:
    ExpString_Free(&str);
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
    
    // free result
    free(o->result);
    
    // free structure
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int func_getvar (void *vo, const char *name, NCDValMem *mem, NCDValRef *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        *out = NCDVal_NewStringBin(mem, (uint8_t *)o->result, o->result_len);
        if (NCDVal_IsInvalid(*out)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewStringBin failed");
        }
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "implode",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_implode = {
    .modules = modules
};
