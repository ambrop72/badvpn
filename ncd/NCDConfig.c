/**
 * @file NCDConfig.c
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
 */

#include <stdlib.h>
#include <string.h>

#include <misc/string_begins_with.h>
#include <misc/expstring.h>
#include <misc/debug.h>

#include <ncd/NCDConfig.h>

void NCDConfig_free_processes (struct NCDConfig_processes *v)
{
    if (!v) {
        return;
    }
    
    free(v->name);
    NCDConfig_free_statements(v->statements);
    NCDConfig_free_processes(v->next);
    
    free(v);
}

void NCDConfig_free_statements (struct NCDConfig_statements *v)
{
    if (!v) {
        return;
    }
    
    NCDConfig_free_strings(v->objname);
    NCDConfig_free_strings(v->names);
    NCDConfig_free_list(v->args);
    free(v->name);
    NCDConfig_free_statements(v->next);
    
    free(v);
}

void NCDConfig_free_list (struct NCDConfig_list *v)
{
    if (!v) {
        return;
    }
    
    switch (v->type) {
        case NCDCONFIG_ARG_STRING:
            free(v->string);
            break;
        case NCDCONFIG_ARG_VAR:
            NCDConfig_free_strings(v->var);
            break;
        case NCDCONFIG_ARG_LIST:
        case NCDCONFIG_ARG_MAPLIST:
            NCDConfig_free_list(v->list);
            break;
        default:
            ASSERT(0);
    }
    
    NCDConfig_free_list(v->next);
    
    free(v);
}

void NCDConfig_free_strings (struct NCDConfig_strings *v)
{
    if (!v) {
        return;
    }
    
    free(v->value);
    NCDConfig_free_strings(v->next);
    
    free(v);
}

struct NCDConfig_processes * NCDConfig_make_processes (int is_template, char *name, struct NCDConfig_statements *statements, struct NCDConfig_processes *next)
{
    struct NCDConfig_processes *v = malloc(sizeof(*v));
    if (!v) {
        goto fail;
    }
    
    v->is_template = is_template;
    v->name = name;
    v->statements = statements;
    v->next = next;
    
    return v;
    
fail:
    free(name);
    NCDConfig_free_statements(statements);
    NCDConfig_free_processes(next);
    return NULL;
}

struct NCDConfig_statements * NCDConfig_make_statements (struct NCDConfig_strings *objname, struct NCDConfig_strings *names, struct NCDConfig_list *args, char *name, struct NCDConfig_statements *next)
{
    struct NCDConfig_statements *v = malloc(sizeof(*v));
    if (!v) {
        goto fail;
    }
    
    v->objname = objname;
    v->names = names;
    v->args = args;
    v->name = name;
    v->next = next;

    return v;
    
fail:
    NCDConfig_free_strings(names);
    NCDConfig_free_list(args);
    free(name);
    NCDConfig_free_statements(next);
    return NULL;
}

struct NCDConfig_list * NCDConfig_make_list_string (char *str, size_t len, struct NCDConfig_list *next)
{
    ASSERT(str[len] == '\0')
    
    struct NCDConfig_list *v = malloc(sizeof(*v));
    if (!v) {
        goto fail;
    }
    
    v->type = NCDCONFIG_ARG_STRING;
    v->string = str;
    v->string_len = len;
    v->next = next;
    
    return v;
    
fail:
    free(str);
    NCDConfig_free_list(next);
    return NULL;
}

struct NCDConfig_list * NCDConfig_make_list_var (struct NCDConfig_strings *var, struct NCDConfig_list *next)
{
    struct NCDConfig_list *v = malloc(sizeof(*v));
    if (!v) {
        goto fail;
    }
    
    v->type = NCDCONFIG_ARG_VAR;
    v->var = var;
    v->next = next;
    
    return v;
    
fail:
    NCDConfig_free_strings(var);
    NCDConfig_free_list(next);
    return NULL;
}

struct NCDConfig_list * NCDConfig_make_list_list (struct NCDConfig_list *list, struct NCDConfig_list *next)
{
    struct NCDConfig_list *v = malloc(sizeof(*v));
    if (!v) {
        goto fail;
    }
    
    v->type = NCDCONFIG_ARG_LIST;
    v->list = list;
    v->next = next;
    
    return v;
    
fail:
    NCDConfig_free_list(list);
    NCDConfig_free_list(next);
    return NULL;
}

struct NCDConfig_list * NCDConfig_make_list_maplist (struct NCDConfig_list *list, struct NCDConfig_list *next)
{
    struct NCDConfig_list *v = malloc(sizeof(*v));
    if (!v) {
        goto fail;
    }
    
    v->type = NCDCONFIG_ARG_MAPLIST;
    v->list = list;
    v->next = next;
    
    return v;
    
fail:
    NCDConfig_free_list(list);
    NCDConfig_free_list(next);
    return NULL;
}

struct NCDConfig_strings * NCDConfig_make_strings (char *value, int need_next, struct NCDConfig_strings *next)
{
    if (!value || (need_next && !next)) {
        goto fail;
    }
    
    struct NCDConfig_strings *v = malloc(sizeof(*v));
    if (!v) {
        goto fail;
    }
    
    v->value = value;
    v->next = next;
    
    return v;
    
fail:
    free(value);
    NCDConfig_free_strings(next);
    return NULL;
}

char * NCDConfig_concat_strings (struct NCDConfig_strings *s)
{
    ExpString str;
    if (!ExpString_Init(&str)) {
        goto fail0;
    }
    
    if (!ExpString_Append(&str, s->value)) {
        goto fail1;
    }
    
    s = s->next;
    
    while (s) {
        if (!ExpString_Append(&str, ".")) {
            goto fail1;
        }
        if (!ExpString_Append(&str, s->value)) {
            goto fail1;
        }
        
        s = s->next;
    }
    
    return ExpString_Get(&str);
    
fail1:
    ExpString_Free(&str);
fail0:
    return NULL;
}
