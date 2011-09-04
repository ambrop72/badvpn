/**
 * @file NCDConfig.c
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
 */

#include <stdlib.h>
#include <string.h>

#include <misc/string_begins_with.h>
#include <misc/expstring.h>

#include <ncd/NCDConfig.h>

void NCDConfig_free_interfaces (struct NCDConfig_interfaces *v)
{
    if (!v) {
        return;
    }
    
    free(v->name);
    NCDConfig_free_statements(v->statements);
    NCDConfig_free_interfaces(v->next);
    
    free(v);
}

void NCDConfig_free_statements (struct NCDConfig_statements *v)
{
    if (!v) {
        return;
    }
    
    NCDConfig_free_strings(v->objname);
    NCDConfig_free_strings(v->names);
    NCDConfig_free_arguments(v->args);
    free(v->name);
    NCDConfig_free_statements(v->next);
    
    free(v);
}

void NCDConfig_free_arguments (struct NCDConfig_arguments *v)
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
        default:
            ASSERT(0);
    }
    
    NCDConfig_free_arguments(v->next);
    
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

struct NCDConfig_interfaces * NCDConfig_make_interfaces (int is_template, char *name, struct NCDConfig_statements *statements, int need_next, struct NCDConfig_interfaces *next)
{
    if (!name || !statements || (need_next && !next)) {
        goto fail;
    }
    
    struct NCDConfig_interfaces *v = malloc(sizeof(*v));
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
    NCDConfig_free_interfaces(next);
    return NULL;
}

struct NCDConfig_statements * NCDConfig_make_statements (struct NCDConfig_strings *objname, struct NCDConfig_strings *names, struct NCDConfig_arguments *args, char *name, struct NCDConfig_statements *next)
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
    NCDConfig_free_arguments(args);
    free(name);
    NCDConfig_free_statements(next);
    return NULL;
}

struct NCDConfig_arguments * NCDConfig_make_arguments_string (char *str, struct NCDConfig_arguments *next)
{
    struct NCDConfig_arguments *v = malloc(sizeof(*v));
    if (!v) {
        goto fail;
    }
    
    v->type = NCDCONFIG_ARG_STRING;
    v->string = str;
    v->next = next;
    
    return v;
    
fail:
    free(str);
    NCDConfig_free_arguments(next);
    return NULL;
}

struct NCDConfig_arguments * NCDConfig_make_arguments_var (struct NCDConfig_strings *var, struct NCDConfig_arguments *next)
{
    struct NCDConfig_arguments *v = malloc(sizeof(*v));
    if (!v) {
        goto fail;
    }
    
    v->type = NCDCONFIG_ARG_VAR;
    v->var = var;
    v->next = next;
    
    return v;
    
fail:
    NCDConfig_free_strings(var);
    NCDConfig_free_arguments(next);
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

int NCDConfig_statement_name_is (struct NCDConfig_statements *st, const char *needle)
{
    ASSERT(st->names)
    
    size_t l;
    
    struct NCDConfig_strings *name = st->names;
    if (!(l = string_begins_with(needle, name->value))) {
        return 0;
    }
    needle += l;
    
    name = name->next;
    
    while (name) {
        if (!(l = string_begins_with(needle, "."))) {
            return 0;
        }
        needle += l;
        
        if (!(l = string_begins_with(needle, name->value))) {
            return 0;
        }
        needle += l;
        
        name = name->next;
    }
    
    if (*needle) {
        return 0;
    }
    
    return 1;
}

struct NCDConfig_statements * NCDConfig_find_statement (struct NCDConfig_statements *st, const char *needle)
{
    while (st) {
        if (NCDConfig_statement_name_is(st, needle)) {
            return st;
        }
        
        st = st->next;
    }
    
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
