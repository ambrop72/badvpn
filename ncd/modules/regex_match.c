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
 * 
 * Synopsis:
 *   regex_replace(string input, list(string) regex, list(string) replace)
 * 
 * Variables:
 *   string (empty) - transformed input
 * 
 * Description:
 *   Replaces matching parts of the input string. Replacement is performed one regular
 *   expression after another: starting with the input string, for each given regular
 *   expression, matching substrings of the current string are replaced with the
 *   corresponding replacement string.
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <regex.h>

#include <misc/string_begins_with.h>
#include <misc/parse_number.h>
#include <misc/expstring.h>
#include <misc/debug.h>
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

struct replace_instance {
    NCDModuleInst *i;
    char *output;
    size_t output_len;
    int output_free;
};

static int regex_replace (const char *input, size_t input_len, const char *regex, const char *replace, size_t replace_len, char **out_output, size_t *out_output_len, NCDModuleInst *i)
{
    int res = 0;
    
    // make sure we don't overflow regoff_t
    if (input_len > INT_MAX) {
        ModuleLog(i, BLOG_ERROR, "string is too long");
        goto fail0;
    }
    
    // compile regex
    regex_t preg;
    int ret;
    if ((ret = regcomp(&preg, regex, REG_EXTENDED)) != 0) {
        ModuleLog(i, BLOG_ERROR, "regcomp failed (error=%d)", ret);
        goto fail0;
    }
    
    // init output string
    ExpString str;
    if (!ExpString_Init(&str)) {
        ModuleLog(i, BLOG_ERROR, "ExpString_Init failed");
        goto fail1;
    }
    
    while (1) {
        // execute match
        regmatch_t matches[MAX_MATCHES];
        matches[0].rm_so = 0;
        matches[0].rm_eo = input_len;
        if (regexec(&preg, input, MAX_MATCHES, matches, REG_STARTEND) != 0) {
            break;
        }
        
        ASSERT(matches[0].rm_so >= 0)
        ASSERT(matches[0].rm_so <= input_len)
        ASSERT(matches[0].rm_eo >= matches[0].rm_so)
        ASSERT(matches[0].rm_eo <= input_len)
        
        // append data before match
        if (!ExpString_AppendBinary(&str, input, matches[0].rm_so)) {
            ModuleLog(i, BLOG_ERROR, "ExpString_AppendBinary failed");
            goto fail2;
        }
        
        // append replace data
        if (!ExpString_AppendBinary(&str, replace, replace_len)) {
            ModuleLog(i, BLOG_ERROR, "ExpString_AppendBinary failed");
            goto fail2;
        }
        
        // go on matching the rest
        input += matches[0].rm_eo;
        input_len -= matches[0].rm_eo;
    }
    
    // append remaining data
    if (!ExpString_AppendBinary(&str, input, input_len)) {
        ModuleLog(i, BLOG_ERROR, "ExpString_AppendBinary failed");
        goto fail2;
    }
    
    // success
    *out_output = ExpString_Get(&str);
    *out_output_len = ExpString_Length(&str);
    res = 1;
    
fail2:
    if (!res) {
        ExpString_Free(&str);
    }
fail1:
    regfree(&preg);
fail0:
    return res;
}

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

static void replace_func_new (NCDModuleInst *i)
{
    // allocate structure
    struct replace_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // read arguments
    NCDValue *input_arg;
    NCDValue *regex_arg;
    NCDValue *replace_arg;
    if (!NCDValue_ListRead(i->args, 3, &input_arg, &regex_arg, &replace_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDValue_IsString(input_arg) || !NCDValue_IsList(regex_arg) || !NCDValue_IsList(replace_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // check number of regex/replace
    if (NCDValue_ListCount(regex_arg) != NCDValue_ListCount(replace_arg)) {
        ModuleLog(i, BLOG_ERROR, "number of regex's is not the same as number of replacements");
        goto fail1;
    }
    
    // start with input as current text
    char *current = NCDValue_StringValue(input_arg);
    size_t current_len = NCDValue_StringLength(input_arg);
    int current_free = 0;
    
    NCDValue *regex = NCDValue_ListFirst(regex_arg);
    NCDValue *replace = NCDValue_ListFirst(replace_arg);
    
    while (regex) {
        // check type of regex and replace
        if (!NCDValue_IsStringNoNulls(regex) || !NCDValue_IsString(replace)) {
            ModuleLog(i, BLOG_ERROR, "regex/replace element has wrong type");
            goto fail2;
        }
        
        // perform the replacing
        char *replaced;
        size_t replaced_len;
        if (!regex_replace(current, current_len, NCDValue_StringValue(regex), NCDValue_StringValue(replace), NCDValue_StringLength(replace), &replaced, &replaced_len, i)) {
            goto fail2;
        }
        
        // update current text
        if (current_free) {
            free(current);
        }
        current = replaced;
        current_len = replaced_len;
        current_free = 1;
        
        regex = NCDValue_ListNext(regex_arg, regex);
        replace = NCDValue_ListNext(replace_arg, replace);
    }
    
    // set output
    o->output = current;
    o->output_len = current_len;
    o->output_free = current_free;
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail2:
    if (current_free) {
        free(current);
    }
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void replace_func_die (void *vo)
{
    struct replace_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free output
    if (o->output_free) {
        free(o->output);
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int replace_func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct replace_instance *o = vo;
    
    if (!strcmp(name, "")) {
        if (!NCDValue_InitStringBin(out, o->output, o->output_len)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitStringBin failed");
            return 0;
        }
        return 1;
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
        .type = "regex_replace",
        .func_new = replace_func_new,
        .func_die = replace_func_die,
        .func_getvar = replace_func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_regex_match = {
    .modules = modules
};
