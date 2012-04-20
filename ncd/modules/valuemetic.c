/**
 * @file valuemetic.c
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
 * Comparison functions for values.
 * 
 * Synopsis:
 *   val_lesser(v1, v2)
 *   val_greater(v1, v2)
 *   val_lesser_equal(v1, v2)
 *   val_greater_equal(v1, v2)
 *   val_equal(v1, v2)
 * 
 * Variables:
 *   (empty) - "true" or "false", reflecting the value of the relation in question
 * 
 * Description:
 *   These statements perform comparisons of values. Order of values is defined by the
 *   following rules:
 *   1. Values of different types have the following order: strings, lists, maps.
 *   2. String values are ordered lexicographically, with respect to the numeric values
 *      of their bytes.
 *   3. List values are ordered lexicographically, where the elements are compared by
 *      recursive application of these rules.
 *   4. Map values are ordered lexicographically, as if a map was as list of (key, value)
 *      pairs, where both the keys and values are compared by recursive application of
 *      these rules.
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_valuemetic.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    int result;
};

typedef int (*compute_func) (NCDValue *v1, NCDValue *v2);

static int compute_lesser (NCDValue *v1, NCDValue *v2)
{
    return NCDValue_Compare(v1, v2) < 0;
}

static int compute_greater (NCDValue *v1, NCDValue *v2)
{
    return NCDValue_Compare(v1, v2) > 0;
}

static int compute_lesser_equal (NCDValue *v1, NCDValue *v2)
{
    return NCDValue_Compare(v1, v2) <= 0;
}

static int compute_greater_equal (NCDValue *v1, NCDValue *v2)
{
    return NCDValue_Compare(v1, v2) >= 0;
}

static int compute_equal (NCDValue *v1, NCDValue *v2)
{
    return NCDValue_Compare(v1, v2) == 0;
}

static void new_templ (NCDModuleInst *i, compute_func cfunc)
{
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    NCDValue *v1_arg;
    NCDValue *v2_arg;
    if (!NCDValue_ListRead(i->args, 2, &v1_arg, &v2_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    o->result = cfunc(v1_arg, v2_arg);
    
    NCDModuleInst_Backend_Up(i);
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

static int func_getvar (void *vo, const char *name, NCDValue *out_value)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        const char *str = o->result ? "true" : "false";
        
        if (!NCDValue_InitString(out_value, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        return 1;
    }
    
    return 0;
}

static void func_new_lesser (NCDModuleInst *i)
{
    new_templ(i, compute_lesser);
}

static void func_new_greater (NCDModuleInst *i)
{
    new_templ(i, compute_greater);
}

static void func_new_lesser_equal (NCDModuleInst *i)
{
    new_templ(i, compute_lesser_equal);
}

static void func_new_greater_equal (NCDModuleInst *i)
{
    new_templ(i, compute_greater_equal);
}

static void func_new_equal (NCDModuleInst *i)
{
    new_templ(i, compute_equal);
}

static const struct NCDModule modules[] = {
    {
        .type = "val_lesser",
        .func_new = func_new_lesser,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "val_greater",
        .func_new = func_new_greater,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "val_lesser_equal",
        .func_new = func_new_lesser_equal,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "val_greater_equal",
        .func_new = func_new_greater_equal,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "val_equal",
        .func_new = func_new_equal,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_valuemetic = {
    .modules = modules
};
