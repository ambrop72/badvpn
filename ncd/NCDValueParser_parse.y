/**
 * @file NCDConfigParser_parse.y
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

%extra_argument { struct parser_state *parser_out }

%token_type { struct token }

%token_destructor { free_token($$); }

%type list_contents { struct value }
%type list { struct value }
%type map_contents { struct value }
%type map  { struct value }
%type value  { struct value }

%destructor list_contents { free_value($$); }
%destructor list { free_value($$); }
%destructor map_contents { free_value($$); }
%destructor map { free_value($$); }
%destructor value { free_value($$); }

%stack_size 0

%syntax_error {
    parser_out->syntax_error = 1;
}

// workaroud Lemon bug: if the stack overflows, the token that caused the overflow will be leaked
%stack_overflow {
    if (yypMinor) {
        free_token(yypMinor->yy0);
    }
}

input ::= value(A). {
    if (!A.have || parser_out->have_value) {
        free_value(A);
    } else {
        parser_out->have_value = 1;
        parser_out->value = A.v;
    }
}

list_contents(R) ::= value(A). {
    if (!A.have) {
        goto failL0;
    }

    NCDValue_InitList(&R.v);

    if (!NCDValue_ListPrepend(&R.v, A.v)) {
        goto failL1;
    }
    A.have = 0;

    R.have = 1;
    goto doneL;

failL1:
    NCDValue_Free(&R.v);
failL0:
    R.have = 0;
    parser_out->out_of_memory = 1;
doneL:
    free_value(A);
}

list_contents(R) ::= value(A) COMMA list_contents(N). {
    if (!A.have || !N.have) {
        goto failM0;
    }

    if (!NCDValue_ListPrepend(&N.v, A.v)) {
        goto failM0;
    }
    A.have = 0;

    R.have = 1;
    R.v = N.v;
    N.have = 0;
    goto doneM;

failM0:
    R.have = 0;
    parser_out->out_of_memory = 1;
doneM:
    free_value(A);
    free_value(N);
}

list(R) ::= CURLY_OPEN CURLY_CLOSE. {
    R.have = 1;
    NCDValue_InitList(&R.v);
}

list(R) ::= CURLY_OPEN list_contents(A) CURLY_CLOSE. {
    R = A;
}

map_contents(R) ::= value(A) COLON value(B). {
    if (!A.have || !B.have) {
        goto failS0;
    }

    NCDValue_InitMap(&R.v);

    if (!NCDValue_MapInsert(&R.v, A.v, B.v)) {
        goto failS1;
    }
    A.have = 0;
    B.have = 0;

    R.have = 1;
    goto doneS;

failS1:
    NCDValue_Free(&R.v);
failS0:
    R.have = 0;
    parser_out->out_of_memory = 1;
doneS:
    free_value(A);
    free_value(B);
}

map_contents(R) ::= value(A) COLON value(B) COMMA map_contents(N). {
    if (!A.have || !B.have || !N.have) {
        goto failT0;
    }

    if (NCDValue_MapFindKey(&N.v, &A.v)) {
        BLog(BLOG_ERROR, "duplicate key in map");
        R.have = 0;
        parser_out->syntax_error = 1;
        goto doneT;
    }

    if (!NCDValue_MapInsert(&N.v, A.v, B.v)) {
        goto failT0;
    }
    A.have = 0;
    B.have = 0;

    R.have = 1;
    R.v = N.v;
    N.have = 0;
    goto doneT;

failT0:
    R.have = 0;
    parser_out->out_of_memory = 1;
doneT:
    free_value(A);
    free_value(B);
    free_value(N);
}

map(R) ::= BRACKET_OPEN BRACKET_CLOSE. {
    R.have = 1;
    NCDValue_InitMap(&R.v);
}

map(R) ::= BRACKET_OPEN map_contents(A) BRACKET_CLOSE. {
    R = A;
}

value(R) ::= STRING(A). {
    ASSERT(A.str)

    if (!NCDValue_InitStringBin(&R.v, (uint8_t *)A.str, A.len)) {
        goto failU0;
    }

    R.have = 1;
    goto doneU;

failU0:
    R.have = 0;
    parser_out->out_of_memory = 1;
doneU:
    free_token(A);
}

value(R) ::= list(A). {
    R = A;
}

value(R) ::= map(A). {
    R = A;
}
