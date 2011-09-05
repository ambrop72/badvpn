/**
 * @file print.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * This file is part of BadVPN.
 * 
 * BadVPN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * BadVPN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
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
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
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
