/**
 * @file NCDConfig.h
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

#ifndef BADVPN_NCDCONFIG_NCDCONFIG_H
#define BADVPN_NCDCONFIG_NCDCONFIG_H

struct NCDConfig_processes;
struct NCDConfig_statements;
struct NCDConfig_list;
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
    struct NCDConfig_list *args;
    char *name;
    struct NCDConfig_statements *next;
};

#define NCDCONFIG_ARG_STRING 1
#define NCDCONFIG_ARG_VAR 2
#define NCDCONFIG_ARG_LIST 3
#define NCDCONFIG_ARG_MAPLIST 4

struct NCDConfig_list {
    int type;
    union {
        char *string;
        struct NCDConfig_strings *var;
        struct NCDConfig_list *list;
    };
    struct NCDConfig_list *next;
};

struct NCDConfig_strings {
    char *value;
    struct NCDConfig_strings *next;
};

void NCDConfig_free_processes (struct NCDConfig_processes *v);
void NCDConfig_free_statements (struct NCDConfig_statements *v);
void NCDConfig_free_list (struct NCDConfig_list *v);
void NCDConfig_free_strings (struct NCDConfig_strings *v);
struct NCDConfig_processes * NCDConfig_make_processes (int is_template, char *name, struct NCDConfig_statements *statements, int have_next, struct NCDConfig_processes *next);
struct NCDConfig_statements * NCDConfig_make_statements (struct NCDConfig_strings *objname, struct NCDConfig_strings *names, struct NCDConfig_list *args, char *name, struct NCDConfig_statements *next);
struct NCDConfig_list * NCDConfig_make_list_string (char *str, struct NCDConfig_list *next);
struct NCDConfig_list * NCDConfig_make_list_var (struct NCDConfig_strings *var, struct NCDConfig_list *next);
struct NCDConfig_list * NCDConfig_make_list_list (struct NCDConfig_list *list, struct NCDConfig_list *next);
struct NCDConfig_list * NCDConfig_make_list_maplist (struct NCDConfig_list *list, struct NCDConfig_list *next);
struct NCDConfig_strings * NCDConfig_make_strings (char *value, int have_next, struct NCDConfig_strings *next);

int NCDConfig_statement_name_is (struct NCDConfig_statements *st, const char *needle);
struct NCDConfig_statements * NCDConfig_find_statement (struct NCDConfig_statements *st, const char *needle);

char * NCDConfig_concat_strings (struct NCDConfig_strings *s);

#endif
