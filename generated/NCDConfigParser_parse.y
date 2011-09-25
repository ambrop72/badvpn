/**
 * @file NCDConfigParser.y
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

%include {

#include <string.h>
#include <stddef.h>

#include <misc/debug.h>
#include <ncd/NCDConfig.h>

struct parser_out {
    int out_of_memory;
    int syntax_error;
    struct NCDConfig_processes *ast;
};

}

%extra_argument {struct parser_out *parser_out}

%token_type {void *}

%token_destructor { free($$); }

%type processes {struct NCDConfig_processes *}
%type statements {struct NCDConfig_statements *}
%type statement_names {struct NCDConfig_strings *}
%type statement_args_maybe {struct NCDConfig_list *}
%type statement_args {struct NCDConfig_list *}
%type name_maybe {char *}
%type process_or_template {int}

%destructor processes { NCDConfig_free_processes($$); }
%destructor statements { NCDConfig_free_statements($$); }
%destructor statement_names { NCDConfig_free_strings($$); }
%destructor statement_args_maybe { NCDConfig_free_list($$); }
%destructor statement_args { NCDConfig_free_list($$); }
%destructor name_maybe { free($$); }

%stack_size 0

%syntax_error {
    parser_out->syntax_error = 1;
}

// workaroud Lemon bug: if the stack overflows, the token that caused the overflow will be leaked
%stack_overflow {
    if (yypMinor) {
        free(yypMinor->yy0);
    }
}

input ::= processes(A). {
    parser_out->ast = A;

    if (!A) {
        parser_out->out_of_memory = 1;
    }
}

processes(R) ::= process_or_template(T) NAME(A) CURLY_OPEN statements(B) CURLY_CLOSE. {
    R = NCDConfig_make_processes(T, A, B, 0, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

processes(R) ::= process_or_template(T) NAME(A) CURLY_OPEN statements(B) CURLY_CLOSE processes(N). {
    R = NCDConfig_make_processes(T, A, B, 1, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) ROUND_OPEN statement_args_maybe(B) ROUND_CLOSE name_maybe(C) SEMICOLON. {
    R = NCDConfig_make_statements(NULL, A, B, C, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) ROUND_OPEN statement_args_maybe(B) ROUND_CLOSE name_maybe(C) SEMICOLON statements(N). {
    R = NCDConfig_make_statements(NULL, A, B, C, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(M) ARROW statement_names(A) ROUND_OPEN statement_args_maybe(B) ROUND_CLOSE name_maybe(C) SEMICOLON. {
    R = NCDConfig_make_statements(M, A, B, C, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(M) ARROW statement_names(A) ROUND_OPEN statement_args_maybe(B) ROUND_CLOSE name_maybe(C) SEMICOLON statements(N). {
    R = NCDConfig_make_statements(M, A, B, C, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statement_names(R) ::= NAME(A). {
    R = NCDConfig_make_strings(A, 0, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statement_names(R) ::= NAME(A) DOT statement_names(N). {
    R = NCDConfig_make_strings(A, 1, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statement_args_maybe(R) ::= . {
    R = NULL;
}

statement_args_maybe(R) ::= statement_args(A). {
    R = A;
}

statement_args(R) ::= STRING(A). {
    R = NCDConfig_make_list_string(A, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statement_args(R) ::= statement_names(A). {
    R = NCDConfig_make_list_var(A, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statement_args(R) ::= STRING(A) COMMA statement_args(N). {
    R = NCDConfig_make_list_string(A, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statement_args(R) ::= statement_names(A) COMMA statement_args(N). {
    R = NCDConfig_make_list_var(A, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

name_maybe(R) ::= . {
    R = NULL;
}

name_maybe(R) ::= NAME(A). {
    R = A;
}

process_or_template(R) ::= PROCESS. {
    R = 0;
}

process_or_template(R) ::= TEMPLATE. {
    R = 1;
}
