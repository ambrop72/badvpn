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

struct NCDConfig_interfaces;
struct NCDConfig_statements;
struct NCDConfig_strings;

struct NCDConfig_interfaces {
    char *name;
    struct NCDConfig_statements *statements;
    struct NCDConfig_interfaces *next;
};

struct NCDConfig_statements {
    struct NCDConfig_strings *names;
    struct NCDConfig_strings *args;
    struct NCDConfig_statements *next;
};

struct NCDConfig_strings {
    char *value;
    struct NCDConfig_strings *next;
};

void NCDConfig_free_interfaces (struct NCDConfig_interfaces *v);
void NCDConfig_free_statements (struct NCDConfig_statements *v);
void NCDConfig_free_strings (struct NCDConfig_strings *v);
struct NCDConfig_interfaces * NCDConfig_make_interfaces (char *name, struct NCDConfig_statements *statements, int have_next, struct NCDConfig_interfaces *next);
struct NCDConfig_statements * NCDConfig_make_statements (struct NCDConfig_strings *names, int have_args, struct NCDConfig_strings *args, int have_next, struct NCDConfig_statements *next);
struct NCDConfig_strings * NCDConfig_make_strings (char *value, int have_next, struct NCDConfig_strings *next);

int NCDConfig_statement_name_is (struct NCDConfig_statements *st, const char *needle);
struct NCDConfig_statements * NCDConfig_find_statement (struct NCDConfig_statements *st, const char *needle);
int NCDConfig_statement_has_one_arg (struct NCDConfig_statements *st, char **arg1_out);
int NCDConfig_statement_has_two_args (struct NCDConfig_statements *st, char **arg1_out, char **arg2_out);
int NCDConfig_statement_has_three_args (struct NCDConfig_statements *st, char **arg1_out, char **arg2_out, char **arg3_out);

#endif
