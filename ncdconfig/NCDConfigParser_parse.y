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
#include <ncdconfig/NCDConfig.h>

struct parser_out {
    int out_of_memory;
    int syntax_error;
    struct NCDConfig_interfaces *ast;
};

}

%extra_argument {struct parser_out *parser_out}

%token_type {void *}

%token_destructor { free($$); }

%type interfaces {struct NCDConfig_interfaces *}
%type statements {struct NCDConfig_statements *}
%type statement_names {struct NCDConfig_strings *}
%type statement_args {struct NCDConfig_arguments *}

%destructor interfaces { NCDConfig_free_interfaces($$); }
%destructor statements { NCDConfig_free_statements($$); }
%destructor statement_names { NCDConfig_free_strings($$); }
%destructor statement_args { NCDConfig_free_arguments($$); }

%stack_size 1000

%syntax_error {
    parser_out->syntax_error = 1;
}

// workaroud Lemon bug: if the stack overflows, the token that caused the overflow will be leaked
%stack_overflow {
    if (yypMinor) {
        free(yypMinor->yy0);
    }
}

input ::= interfaces(A). {
    parser_out->ast = A;

    if (!A) {
        parser_out->out_of_memory = 1;
    }
}

interfaces(R) ::= PROCESS NAME(A) CURLY_OPEN statements(B) CURLY_CLOSE. {
    R = NCDConfig_make_interfaces(A, B, 0, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

interfaces(R) ::= PROCESS NAME(A) CURLY_OPEN statements(B) CURLY_CLOSE interfaces(N). {
    R = NCDConfig_make_interfaces(A, B, 1, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) ROUND_OPEN ROUND_CLOSE SEMICOLON. {
    R = NCDConfig_make_statements(A, NULL, NULL, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) ROUND_OPEN statement_args(B) ROUND_CLOSE SEMICOLON. {
    R = NCDConfig_make_statements(A, B, NULL, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) ROUND_OPEN ROUND_CLOSE SEMICOLON statements(N). {
    R = NCDConfig_make_statements(A, NULL, NULL, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) ROUND_OPEN statement_args(B) ROUND_CLOSE SEMICOLON statements(N). {
    R = NCDConfig_make_statements(A, B, NULL, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) ROUND_OPEN ROUND_CLOSE NAME(C) SEMICOLON. {
    R = NCDConfig_make_statements(A, NULL, C, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) ROUND_OPEN statement_args(B) ROUND_CLOSE NAME(C) SEMICOLON. {
    R = NCDConfig_make_statements(A, B, C, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) ROUND_OPEN ROUND_CLOSE NAME(C) SEMICOLON statements(N). {
    R = NCDConfig_make_statements(A, NULL, C, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) ROUND_OPEN statement_args(B) ROUND_CLOSE NAME(C) SEMICOLON statements(N). {
    R = NCDConfig_make_statements(A, B, C, N);
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

statement_args(R) ::= STRING(A). {
    R = NCDConfig_make_arguments_string(A, 0, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statement_args(R) ::= statement_names(A). {
    R = NCDConfig_make_arguments_var(A, 0, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statement_args(R) ::= STRING(A) COMMA statement_args(N). {
    R = NCDConfig_make_arguments_string(A, 1, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statement_args(R) ::= statement_names(A) COMMA statement_args(N). {
    R = NCDConfig_make_arguments_var(A, 1, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}
