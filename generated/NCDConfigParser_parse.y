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
%type statement_args {struct NCDConfig_strings *}

%destructor interfaces { NCDConfig_free_interfaces($$); }
%destructor statements { NCDConfig_free_statements($$); }
%destructor statement_names { NCDConfig_free_strings($$); }
%destructor statement_args { NCDConfig_free_strings($$); }

%syntax_error {
    parser_out->syntax_error = 1;
}

input ::= interfaces(A). {
    parser_out->ast = A;

    if (!A) {
        parser_out->out_of_memory = 1;
    }
}

interfaces(R) ::= INTERFACE NAME(A) CURRLY_OPEN statements(B) CURLY_CLOSE. {
    R = NCDConfig_make_interfaces(A, B, 0, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

interfaces(R) ::= INTERFACE NAME(A) CURRLY_OPEN statements(B) CURLY_CLOSE interfaces(N). {
    R = NCDConfig_make_interfaces(A, B, 1, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) SEMICOLON. {
    R = NCDConfig_make_statements(A, 0, NULL, 0, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) statement_args(B) SEMICOLON. {
    R = NCDConfig_make_statements(A, 1, B, 0, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) SEMICOLON statements(N). {
    R = NCDConfig_make_statements(A, 0, NULL, 1, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statements(R) ::= statement_names(A) statement_args(B) SEMICOLON statements(N). {
    R = NCDConfig_make_statements(A, 1, B, 1, N);
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
    R = NCDConfig_make_strings(A, 0, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statement_args(R) ::= STRING(A) statement_args(N). {
    R = NCDConfig_make_strings(A, 1, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}
