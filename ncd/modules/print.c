/**
 * @file print.c
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
 * Modules for printing to standard output.
 * 
 * Synopsis:
 *   print([string str ...])
 * Description:
 *   On initialization, prints strings to standard output.
 * 
 * Synopsis:
 *   println([string str ...])
 * Description:
 *   On initialization, prints strings to standard output, and a newline.
 * 
 * Synopsis:
 *   rprint([string str ...])
 * Description:
 *   On deinitialization, prints strings to standard output.
 * 
 * Synopsis:
 *   rprintln([string str ...])
 * Description:
 *   On deinitialization, prints strings to standard output, and a newline.
 */

#include <stdlib.h>
#include <stdio.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_print.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct rprint_instance {
    NCDModuleInst *i;
    int ln;
};

static int check_args (NCDModuleInst *i)
{
    size_t num_args = NCDVal_ListCount(i->args);
    
    for (size_t j = 0; j < num_args; j++) {
        NCDValRef arg = NCDVal_ListGet(i->args, j);
        if (!NCDVal_IsString(arg)) {
            ModuleLog(i, BLOG_ERROR, "wrong type");
            return 0;
        }
    }
    
    return 1;
}

static void do_print (NCDModuleInst *i, int ln)
{
    size_t num_args = NCDVal_ListCount(i->args);
    
    for (size_t j = 0; j < num_args; j++) {
        NCDValRef arg = NCDVal_ListGet(i->args, j);
        ASSERT(NCDVal_IsString(arg))
        
        const char *str = NCDVal_StringValue(arg);
        size_t len = NCDVal_StringLength(arg);
        size_t pos = 0;
        
        while (pos < len) {
            ssize_t res = fwrite(str + pos, 1, len - pos, stdout);
            if (res <= 0) {
                break;
            }
            
            pos += res;
            len -= res;
        }
    }
    
    if (ln) {
        printf("\n");
    }
}

static void rprint_func_new_common (void *vo, NCDModuleInst *i, int ln)
{
    struct rprint_instance *o = vo;
    o->i = i;
    o->ln = ln;
    
    if (!check_args(i)) {
        goto fail0;
    }
    
    NCDModuleInst_Backend_Up(i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void rprint_func_die (void *vo)
{
    struct rprint_instance *o = vo;
    
    do_print(o->i, o->ln);
    
    NCDModuleInst_Backend_Dead(o->i);
}

static void print_func_new (NCDModuleInst *i)
{
    if (!check_args(i)) {
        goto fail0;
    }
    
    do_print(i, 0);
    
    NCDModuleInst_Backend_Up(i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void println_func_new (NCDModuleInst *i)
{
    if (!check_args(i)) {
        goto fail0;
    }
    
    do_print(i, 1);
    
    NCDModuleInst_Backend_Up(i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void rprint_func_new (void *vo, NCDModuleInst *i)
{
    return rprint_func_new_common(vo, i, 0);
}

static void rprintln_func_new (void *vo, NCDModuleInst *i)
{
    return rprint_func_new_common(vo, i, 1);
}

static const struct NCDModule modules[] = {
    {
        .type = "print",
        .func_new = print_func_new
    }, {
        .type = "println",
        .func_new = println_func_new
    }, {
        .type = "rprint",
        .func_new2 = rprint_func_new,
        .func_die = rprint_func_die,
        .alloc_size = sizeof(struct rprint_instance)
     }, {
        .type = "rprintln",
        .func_new2 = rprintln_func_new,
        .func_die = rprint_func_die,
        .alloc_size = sizeof(struct rprint_instance)
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_print = {
    .modules = modules
};
