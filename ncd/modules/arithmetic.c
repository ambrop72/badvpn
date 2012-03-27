/**
 * @file arithmetic.c
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
 * Arithmetic functions for unsigned integers.
 * 
 * Synopsis:
 *   num_lesser(string n1, string n2)
 *   num_greater(string n1, string n2)
 *   num_lesser_equal(string n1, string n2)
 *   num_greater_equal(string n1, string n2)
 *   num_equal(string n1, string n2)
 * 
 * Variables:
 *   (empty) - "true" or "false", reflecting the value of the relation in question
 * 
 * Description:
 *   These statements perform arithmetic comparisons. The operands passed must be
 *   non-negative decimal integers representable in a uintmax_t. Otherwise, an error
 *   is triggered.
 * 
 * Synopsis:
 *   num_add(string n1, string n2)
 *   num_subtract(string n1, string n2)
 *   num_multiply(string n1, string n2)
 *   num_divide(string n1, string n2)
 *   num_modulo(string n1, string n2)
 * 
 * Description:
 *   These statements perform arithmetic operations. The operands passed must be
 *   non-negative decimal integers representable in a uintmax_t, and the result must
 *   also be representable and non-negative. For divide and modulo, n2 must be non-zero.
 *   If any of these restrictions is violated, an error is triggered.
 * 
 * Variables:
 *   (empty) - the result of the operation as a string representing a decimal number
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>

#include <misc/parse_number.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_arithmetic.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    char value[25];
};

typedef int (*compute_func) (NCDModuleInst *i, uintmax_t n1, uintmax_t n2, char *out_str);

static int compute_lesser (NCDModuleInst *i, uintmax_t n1, uintmax_t n2, char *out_str)
{
    strcpy(out_str, (n1 < n2) ? "true" : "false");
    return 1;
}

static int compute_greater (NCDModuleInst *i, uintmax_t n1, uintmax_t n2, char *out_str)
{
    strcpy(out_str, (n1 > n2) ? "true" : "false");
    return 1;
}

static int compute_lesser_equal (NCDModuleInst *i, uintmax_t n1, uintmax_t n2, char *out_str)
{
    strcpy(out_str, (n1 <= n2) ? "true" : "false");
    return 1;
}

static int compute_greater_equal (NCDModuleInst *i, uintmax_t n1, uintmax_t n2, char *out_str)
{
    strcpy(out_str, (n1 >= n2) ? "true" : "false");
    return 1;
}

static int compute_equal (NCDModuleInst *i, uintmax_t n1, uintmax_t n2, char *out_str)
{
    strcpy(out_str, (n1 == n2) ? "true" : "false");
    return 1;
}

static int compute_add (NCDModuleInst *i, uintmax_t n1, uintmax_t n2, char *out_str)
{
    if (n1 > UINTMAX_MAX - n2) {
        ModuleLog(i, BLOG_ERROR, "addition overflow");
        return 0;
    }
    uintmax_t r = n1 + n2;
    sprintf(out_str, "%"PRIuMAX, r);
    return 1;
}

static int compute_subtract (NCDModuleInst *i, uintmax_t n1, uintmax_t n2, char *out_str)
{
    if (n1 < n2) {
        ModuleLog(i, BLOG_ERROR, "subtraction underflow");
        return 0;
    }
    uintmax_t r = n1 - n2;
    sprintf(out_str, "%"PRIuMAX, r);
    return 1;
}

static int compute_multiply (NCDModuleInst *i, uintmax_t n1, uintmax_t n2, char *out_str)
{
    if (n1 > UINTMAX_MAX / n2) {
        ModuleLog(i, BLOG_ERROR, "multiplication overflow");
        return 0;
    }
    uintmax_t r = n1 * n2;
    sprintf(out_str, "%"PRIuMAX, r);
    return 1;
}

static int compute_divide (NCDModuleInst *i, uintmax_t n1, uintmax_t n2, char *out_str)
{
    if (n2 == 0) {
        ModuleLog(i, BLOG_ERROR, "division quotient is zero");
        return 0;
    }
    uintmax_t r = n1 / n2;
    sprintf(out_str, "%"PRIuMAX, r);
    return 1;
}

static int compute_modulo (NCDModuleInst *i, uintmax_t n1, uintmax_t n2, char *out_str)
{
    if (n2 == 0) {
        ModuleLog(i, BLOG_ERROR, "modulo modulus is zero");
        return 0;
    }
    uintmax_t r = n1 % n2;
    sprintf(out_str, "%"PRIuMAX, r);
    return 1;
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
    
    NCDValue *n1_arg;
    NCDValue *n2_arg;
    if (!NCDValue_ListRead(i->args, 2, &n1_arg, &n2_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(n1_arg) != NCDVALUE_STRING || NCDValue_Type(n2_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    uintmax_t n1;
    uintmax_t n2;
    if (!parse_unsigned_integer(NCDValue_StringValue(n1_arg), &n1) || !parse_unsigned_integer(NCDValue_StringValue(n2_arg), &n2)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong value");
        goto fail1;
    }
    
    if (!cfunc(i, n1, n2, o->value)) {
        goto fail1;
    }
    
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
        if (!NCDValue_InitString(out_value, o->value)) {
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

static void func_new_add (NCDModuleInst *i)
{
    new_templ(i, compute_add);
}

static void func_new_subtract (NCDModuleInst *i)
{
    new_templ(i, compute_subtract);
}

static void func_new_multiply (NCDModuleInst *i)
{
    new_templ(i, compute_multiply);
}

static void func_new_divide (NCDModuleInst *i)
{
    new_templ(i, compute_divide);
}

static void func_new_modulo (NCDModuleInst *i)
{
    new_templ(i, compute_modulo);
}

static const struct NCDModule modules[] = {
    {
        .type = "num_lesser",
        .func_new = func_new_lesser,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "num_greater",
        .func_new = func_new_greater,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "num_lesser_equal",
        .func_new = func_new_lesser_equal,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "num_greater_equal",
        .func_new = func_new_greater_equal,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "num_equal",
        .func_new = func_new_equal,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "num_add",
        .func_new = func_new_add,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "num_subtract",
        .func_new = func_new_subtract,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "num_multiply",
        .func_new = func_new_multiply,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "num_divide",
        .func_new = func_new_divide,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "num_modulo",
        .func_new = func_new_modulo,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_arithmetic = {
    .modules = modules
};
