/**
 * @file regex_match.c
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
 * Regular expression matching module.
 * 
 * Synopsis: regex_match(string input, string regex)
 * Variables:
 *   succeeded - "true" or "false", indicating whether input matched regex
 *   matchN - for N=0,1,2,..., the matching data for the N-th subexpression
 *     (match0 = whole match)
 */

#include <stdlib.h>
#include <string.h>
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
    if (NCDValue_Type(input_arg) != NCDVALUE_STRING || NCDValue_Type(regex_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->input = NCDValue_StringValue(input_arg);
    char *regex = NCDValue_StringValue(regex_arg);
    
    // compile regex
    regex_t preg;
    int ret;
    if ((ret = regcomp(&preg, regex, REG_EXTENDED)) != 0) {
        ModuleLog(o->i, BLOG_ERROR, "regcomp failed (error=%d)", ret);
        goto fail1;
    }
    
    // execute match
    o->succeeded = (regexec(&preg, o->input, MAX_MATCHES, o->matches, 0) == 0);
    
    // free regex
    regfree(&preg);
    
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
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
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
            
            ASSERT(m->rm_so <= strlen(o->input))
            ASSERT(m->rm_eo >= m->rm_so)
            ASSERT(m->rm_eo <= strlen(o->input))
            
            size_t len = m->rm_eo - m->rm_so;
            
            char *str = malloc(len + 1);
            if (!str) {
                ModuleLog(o->i, BLOG_ERROR, "malloc failed");
                return 0;
            }
            
            memcpy(str, o->input + m->rm_so, len);
            str[len] = '\0';
            
            if (!NCDValue_InitString(out, str)) {
                ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
                free(str);
                return 0;
            }
            
            free(str);
            
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
