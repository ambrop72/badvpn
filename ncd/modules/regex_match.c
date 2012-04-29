/**
 * @file regex_match.c
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
 * Regular expression matching module.
 * 
 * Synopsis:
 *   regex_match(string input, string regex)
 * 
 * Variables:
 *   succeeded - "true" or "false", indicating whether input matched regex
 *   matchN - for N=0,1,2,..., the matching data for the N-th subexpression
 *     (match0 = whole match)
 * 
 * Description:
 *   Matches 'input' with the POSIX extended regular expression 'regex'.
 *   'regex' must be a string without null bytes, but 'input' can contain null bytes.
 *   However, it's difficult, if not impossible, to actually match nulls with the regular
 *   expression.
 *   The input and regex strings are interpreted according to the POSIX regex functions
 *   (regcomp(), regexec()); in particular, the current locale setting affects the
 *   interpretation.
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <regex.h>

#include <misc/string_begins_with.h>
#include <misc/parse_number.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_regex_match.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define MAX_MATCHES 64

struct instance {
    NCDModuleInst *i;
    char *input;
    size_t input_len;
    int succeeded;
    int num_matches;
    regmatch_t matches[MAX_MATCHES];
};

static void func_new (NCDModuleInst *i)
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
    
    // read arguments
    NCDValue *input_arg;
    NCDValue *regex_arg;
    if (!NCDValue_ListRead(o->i->args, 2, &input_arg, &regex_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(input_arg) != NCDVALUE_STRING || !NCDValue_IsStringNoNulls(regex_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->input = NCDValue_StringValue(input_arg);
    o->input_len = NCDValue_StringLength(input_arg);
    char *regex = NCDValue_StringValue(regex_arg);
    
    // make sure we don't overflow regoff_t
    if (o->input_len > INT_MAX) {
        ModuleLog(o->i, BLOG_ERROR, "input string too long");
        goto fail1;
    }
    
    // compile regex
    regex_t preg;
    int ret;
    if ((ret = regcomp(&preg, regex, REG_EXTENDED)) != 0) {
        ModuleLog(o->i, BLOG_ERROR, "regcomp failed (error=%d)", ret);
        goto fail1;
    }
    
    // execute match
    o->matches[0].rm_so = 0;
    o->matches[0].rm_eo = o->input_len;
    o->succeeded = (regexec(&preg, o->input, MAX_MATCHES, o->matches, REG_STARTEND) == 0);
    
    // free regex
    regfree(&preg);
    
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
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "succeeded")) {
        if (!NCDValue_InitString(out, (o->succeeded ? "true" : "false"))) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
            return 0;
        }
        
        return 1;
    }
    
    size_t pos;
    uintmax_t n;
    if ((pos = string_begins_with(name, "match")) && parse_unsigned_integer(name + pos, &n)) {
        if (o->succeeded && n < MAX_MATCHES && o->matches[n].rm_so >= 0) {
            regmatch_t *m = &o->matches[n];
            
            ASSERT(m->rm_so <= o->input_len)
            ASSERT(m->rm_eo >= m->rm_so)
            ASSERT(m->rm_eo <= o->input_len)
            
            size_t len = m->rm_eo - m->rm_so;
            
            if (!NCDValue_InitStringBin(out, o->input + m->rm_so, len)) {
                ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitStringBin failed");
                return 0;
            }
            
            return 1;
        }
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "regex_match",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_regex_match = {
    .modules = modules
};
