/**
 * @file NCDConfig.h
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

#ifndef BADVPN_NCDCONFIG_NCDCONFIG_H
#define BADVPN_NCDCONFIG_NCDCONFIG_H

struct NCDConfig_processes;
struct NCDConfig_statements;
struct NCDConfig_arguments;
struct NCDConfig_strings;

struct NCDConfig_processes {
    int is_template;
    char *name;
    struct NCDConfig_statements *statements;
    struct NCDConfig_processes *next;
};

struct NCDConfig_statements {
    struct NCDConfig_strings *objname;
    struct NCDConfig_strings *names;
    struct NCDConfig_arguments *args;
    char *name;
    struct NCDConfig_statements *next;
};

#define NCDCONFIG_ARG_STRING 1
#define NCDCONFIG_ARG_VAR 2

struct NCDConfig_arguments {
    int type;
    union {
        char *string;
        struct NCDConfig_strings *var;
    };
    struct NCDConfig_arguments *next;
};

struct NCDConfig_strings {
    char *value;
    struct NCDConfig_strings *next;
};

void NCDConfig_free_processes (struct NCDConfig_processes *v);
void NCDConfig_free_statements (struct NCDConfig_statements *v);
void NCDConfig_free_arguments (struct NCDConfig_arguments *v);
void NCDConfig_free_strings (struct NCDConfig_strings *v);
struct NCDConfig_processes * NCDConfig_make_processes (int is_template, char *name, struct NCDConfig_statements *statements, int have_next, struct NCDConfig_processes *next);
struct NCDConfig_statements * NCDConfig_make_statements (struct NCDConfig_strings *objname, struct NCDConfig_strings *names, struct NCDConfig_arguments *args, char *name, struct NCDConfig_statements *next);
struct NCDConfig_arguments * NCDConfig_make_arguments_string (char *str, struct NCDConfig_arguments *next);
struct NCDConfig_arguments * NCDConfig_make_arguments_var (struct NCDConfig_strings *var, struct NCDConfig_arguments *next);
struct NCDConfig_strings * NCDConfig_make_strings (char *value, int have_next, struct NCDConfig_strings *next);

int NCDConfig_statement_name_is (struct NCDConfig_statements *st, const char *needle);
struct NCDConfig_statements * NCDConfig_find_statement (struct NCDConfig_statements *st, const char *needle);

char * NCDConfig_concat_strings (struct NCDConfig_strings *s);

#endif
