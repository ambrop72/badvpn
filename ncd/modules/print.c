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

struct instance {
    NCDModuleInst *i;
    int ln;
    int rev;
};

static void do_print (NCDModuleInst *i, int ln)
{
    for (NCDValue *arg = NCDValue_ListFirst(i->args); arg; arg = NCDValue_ListNext(i->args, arg)) {
        ASSERT(NCDValue_Type(arg) == NCDVALUE_STRING)
        printf("%s", NCDValue_StringValue(arg));
    }
    
    if (ln) {
        printf("\n");
    }
}

static void func_new_temp (NCDModuleInst *i, int ln, int rev)
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
    o->ln = ln;
    o->rev = rev;
    
    // check arguments
    for (NCDValue *arg = NCDValue_ListFirst(i->args); arg; arg = NCDValue_ListNext(i->args, arg)) {
        if (NCDValue_Type(arg) != NCDVALUE_STRING) {
            ModuleLog(o->i, BLOG_ERROR, "wrong type");
            goto fail1;
        }
    }
    
    // print
    if (!o->rev) {
        do_print(o->i, o->ln);
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
    
    // print
    if (o->rev) {
        do_print(o->i, o->ln);
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void print_func_new (NCDModuleInst *i)
{
    return func_new_temp(i, 0, 0);
}

static void println_func_new (NCDModuleInst *i)
{
    return func_new_temp(i, 1, 0);
}

static void rprint_func_new (NCDModuleInst *i)
{
    return func_new_temp(i, 0, 1);
}

static void rprintln_func_new (NCDModuleInst *i)
{
    return func_new_temp(i, 1, 1);
}

static const struct NCDModule modules[] = {
    {
        .type = "print",
        .func_new = print_func_new,
        .func_die = func_die
    }, {
        .type = "println",
        .func_new = println_func_new,
        .func_die = func_die
    }, {
        .type = "rprint",
        .func_new = rprint_func_new,
        .func_die = func_die
     }, {
        .type = "rprintln",
        .func_new = rprintln_func_new,
        .func_die = func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_print = {
    .modules = modules
};
