/**
 * @file NCDConfigParser.y
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

%include {

#include <string.h>
#include <stddef.h>

#include <misc/debug.h>
#include <ncd/NCDConfig.h>

struct parser_minor {
    char *str;
    size_t len;
};

struct parser_out {
    int out_of_memory;
    int syntax_error;
    struct NCDConfig_processes *ast;
};

}

%extra_argument {struct parser_out *parser_out}

%token_type {struct parser_minor}

%token_destructor { free($$.str); }

%type processes {struct NCDConfig_processes *}
%type statements {struct NCDConfig_statements *}
%type statement_names {struct NCDConfig_strings *}
%type statement_args_maybe {struct NCDConfig_list *}
%type list_contents {struct NCDConfig_list *}
%type list {struct NCDConfig_list *}
%type map_contents {struct NCDConfig_list *}
%type map {struct NCDConfig_list *}
%type value {struct NCDConfig_list *}
%type name_maybe {char *}
%type process_or_template {int}

%destructor processes { NCDConfig_free_processes($$); }
%destructor statements { NCDConfig_free_statements($$); }
%destructor statement_names { NCDConfig_free_strings($$); }
%destructor statement_args_maybe { NCDConfig_free_list($$); }
%destructor list_contents { NCDConfig_free_list($$); }
%destructor list { NCDConfig_free_list($$); }
%destructor map_contents { NCDConfig_free_list($$); }
%destructor map { NCDConfig_free_list($$); }
%destructor value { NCDConfig_free_list($$); }
%destructor name_maybe { free($$); }

%stack_size 0

%syntax_error {
    parser_out->syntax_error = 1;
}

// workaroud Lemon bug: if the stack overflows, the token that caused the overflow will be leaked
%stack_overflow {
    if (yypMinor) {
        free(yypMinor->yy0.str);
    }
}

input ::= processes(A). {
    parser_out->ast = A;

    if (!A) {
        parser_out->out_of_memory = 1;
    }
}

processes(R) ::= process_or_template(T) NAME(A) CURLY_OPEN statements(B) CURLY_CLOSE. {
    R = NCDConfig_make_processes(T, A.str, B, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

processes(R) ::= process_or_template(T) NAME(A) CURLY_OPEN statements(B) CURLY_CLOSE processes(N). {
    R = NCDConfig_make_processes(T, A.str, B, N);
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
    R = NCDConfig_make_strings(A.str, 0, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statement_names(R) ::= NAME(A) DOT statement_names(N). {
    R = NCDConfig_make_strings(A.str, 1, N);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

statement_args_maybe(R) ::= . {
    R = NULL;
}

statement_args_maybe(R) ::= list_contents(A). {
    R = A;
}

list_contents(R) ::= value(A). {
    R = A;
}

list_contents(R) ::= value(A) COMMA list_contents(N). {
    if (!A) {
        NCDConfig_free_list(N);
    } else {
        ASSERT(!A->next)
        A->next = N;
    }
    R = A;
}

list(R) ::= CURLY_OPEN CURLY_CLOSE. {
    R = NULL;
}

list(R) ::= CURLY_OPEN list_contents(A) CURLY_CLOSE. {
    R = A;
}

map_contents(R) ::= value(A) COLON value(B). {
    if (!A || !B) {
        NCDConfig_free_list(A);
        NCDConfig_free_list(B);
        R = NULL;
    } else {
        ASSERT(!A->next)
        ASSERT(!B->next)
        A->next = B;
        R = A;
    }
}

map_contents(R) ::= value(A) COLON value(B) COMMA map_contents(N). {
    if (!A || !B) {
        NCDConfig_free_list(A);
        NCDConfig_free_list(B);
        NCDConfig_free_list(N);
        R = NULL;
    } else {
        ASSERT(!A->next)
        ASSERT(!B->next)
        A->next = B;
        B->next = N;
        R = A;
    }
}

map(R) ::= BRACKET_OPEN BRACKET_CLOSE. {
    R = NULL;
}

map(R) ::= BRACKET_OPEN map_contents(A) BRACKET_CLOSE. {
    R = A;
}

value(R) ::= STRING(A). {
    R = NCDConfig_make_list_string(A.str, A.len, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

value(R) ::= statement_names(A). {
    R = NCDConfig_make_list_var(A, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

value(R) ::= list(A). {
    R = NCDConfig_make_list_list(A, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

value(R) ::= map(A). {
    R = NCDConfig_make_list_maplist(A, NULL);
    if (!R) {
        parser_out->out_of_memory = 1;
    }
}

name_maybe(R) ::= . {
    R = NULL;
}

name_maybe(R) ::= NAME(A). {
    R = A.str;
}

process_or_template(R) ::= PROCESS. {
    R = 0;
}

process_or_template(R) ::= TEMPLATE. {
    R = 1;
}
