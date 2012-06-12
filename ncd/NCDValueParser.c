/**
 * @file NCDValueParser.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <base/BLog.h>
#include <ncd/NCDConfigTokenizer.h>
#include <ncd/NCDValCompat.h>

#include "NCDValueParser.h"

#include <generated/blog_channel_NCDValueParser.h>

struct token {
    char *str;
    size_t len;
};

struct value {
    int have;
    NCDValue v;
};

struct parser_state {
    int have_value;
    NCDValue value;
    int out_of_memory;
    int syntax_error;
    int error;
    void *parser;
};

static void free_token (struct token o)
{
    free(o.str);
};

static void free_value (struct value o)
{
    if (o.have) {
        NCDValue_Free(&o.v);
    }
}

// rename non-static functions defined by our Lemon parser
// to avoid clashes with other Lemon parsers
#define ParseTrace ParseTrace_NCDValueParser
#define ParseAlloc ParseAlloc_NCDValueParser
#define ParseFree ParseFree_NCDValueParser
#define Parse Parse_NCDValueParser

#include "../generated/NCDValueParser_parse.c"
#include "../generated/NCDValueParser_parse.h"

static int tokenizer_output (void *user, int token, char *value, size_t value_len, size_t line, size_t line_char)
{
    struct parser_state *state = user;
    ASSERT(!state->out_of_memory)
    ASSERT(!state->syntax_error)
    ASSERT(!state->error)
    
    if (token == NCD_ERROR) {
        BLog(BLOG_ERROR, "line %zu, character %zu: tokenizer error", line, line_char);
        goto fail;
    }
    
    struct token minor;
    minor.str = value;
    minor.len = value_len;
    
    switch (token) {
        case NCD_EOF: {
            Parse(state->parser, 0, minor, state);
        } break;
        
        case NCD_TOKEN_CURLY_OPEN: {
            Parse(state->parser, CURLY_OPEN, minor, state);
        } break;
        
        case NCD_TOKEN_CURLY_CLOSE: {
            Parse(state->parser, CURLY_CLOSE, minor, state);
        } break;
        
        case NCD_TOKEN_COMMA: {
            Parse(state->parser, COMMA, minor, state);
        } break;
        
        case NCD_TOKEN_STRING: {
            Parse(state->parser, STRING, minor, state);
        } break;
        
        case NCD_TOKEN_COLON: {
            Parse(state->parser, COLON, minor, state);
        } break;
        
        case NCD_TOKEN_BRACKET_OPEN: {
            Parse(state->parser, BRACKET_OPEN, minor, state);
        } break;
        
        case NCD_TOKEN_BRACKET_CLOSE: {
            Parse(state->parser, BRACKET_CLOSE, minor, state);
        } break;
        
        default:
            BLog(BLOG_ERROR, "line %zu, character %zu: invalid token", line, line_char);
            free_token(minor);
            goto fail;
    }
    
    if (state->syntax_error) {
        BLog(BLOG_ERROR, "line %zu, character %zu: syntax error", line, line_char);
        goto fail;
    }
    
    if (state->out_of_memory) {
        BLog(BLOG_ERROR, "line %zu, character %zu: out of memory", line, line_char);
        goto fail;
    }
    
    return 1;
    
fail:
    state->error = 1;
    return 0;
}

int NCDValueParser_Parse (const char *str, size_t str_len, NCDValue *out_value)
{
    ASSERT(str_len == 0 || str)
    ASSERT(out_value)
    
    struct parser_state state;
    state.have_value = 0;
    state.out_of_memory = 0;
    state.syntax_error = 0;
    state.error = 0;
    
    if (!(state.parser = ParseAlloc(malloc))) {
        BLog(BLOG_ERROR, "ParseAlloc failed");
        return 0;
    }
    
    NCDConfigTokenizer_Tokenize((char *)str, str_len, tokenizer_output, &state);
    
    ParseFree(state.parser, free);
    
    if (state.error) {
        if (state.have_value) {
            NCDValue_Free(&state.value);
        }
        return 0;
    }
    
    ASSERT(state.have_value)
    
    *out_value = state.value;
    return 1;
}

int NCDValParser_Parse (const char *str, size_t str_len, NCDValMem *mem, NCDValRef *out_value)
{
    ASSERT(str_len == 0 || str)
    ASSERT(mem)
    ASSERT(out_value)
    
    NCDValue value;
    if (!NCDValueParser_Parse(str, str_len, &value)) {
        return 0;
    }
    
    if (!NCDValCompat_ValueToVal(&value, mem, out_value)) {
        BLog(BLOG_ERROR, "NCDValCompat_ValueToVal failed");
        NCDValue_Free(&value);
        return 0;
    }
    
    NCDValue_Free(&value);
    return 1;
}
