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

#include <ncdconfig/NCDConfig.h>

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
    
    NCDConfig_free_strings(v->names);
    NCDConfig_free_strings(v->args);
    NCDConfig_free_statements(v->next);
    
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

struct NCDConfig_interfaces * NCDConfig_make_interfaces (char *name, struct NCDConfig_statements *statements, int need_next, struct NCDConfig_interfaces *next)
{
    if (!name || !statements || (need_next && !next)) {
        goto fail;
    }
    
    struct NCDConfig_interfaces *v = malloc(sizeof(*v));
    if (!v) {
        goto fail;
    }
    
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

struct NCDConfig_statements * NCDConfig_make_statements (struct NCDConfig_strings *names, int need_args, struct NCDConfig_strings *args, int need_next, struct NCDConfig_statements *next)
{
    if (!names || (need_args && !args) || (need_next && !next)) {
        goto fail;
    }
    
    struct NCDConfig_statements *v = malloc(sizeof(*v));
    if (!v) {
        goto fail;
    }

    v->names = names;
    v->args = args;
    v->next = next;

    return v;
    
fail:
    NCDConfig_free_strings(names);
    NCDConfig_free_strings(args);
    NCDConfig_free_statements(next);
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

int NCDConfig_statement_has_one_arg (struct NCDConfig_statements *st, char **arg1_out)
{
    if (!(st->args && !st->args->next)) {
        return 0;
    }
    
    *arg1_out = st->args->value;
    return 1;
}

int NCDConfig_statement_has_two_args (struct NCDConfig_statements *st, char **arg1_out, char **arg2_out)
{
    if (!(st->args && st->args->next && !st->args->next->next)) {
        return 0;
    }
    
    *arg1_out = st->args->value;
    *arg2_out = st->args->next->value;
    return 1;
}

int NCDConfig_statement_has_three_args (struct NCDConfig_statements *st, char **arg1_out, char **arg2_out, char **arg3_out)
{
    if (!(st->args && st->args->next && st->args->next->next && !st->args->next->next->next)) {
        return 0;
    }
    
    *arg1_out = st->args->value;
    *arg2_out = st->args->next->value;
    *arg3_out = st->args->next->next->value;
    return 1;
}
