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

#include <misc/debug.h>
#include <base/BLog.h>
#include <ncd/NCDConfigTokenizer.h>
#include <ncd/NCDValCompat.h>

#include "NCDValueParser.h"

#include <generated/blog_channel_NCDValueParser.h>

// rename non-static functions defined by our Lemon parser
// to avoid clashes with other Lemon parsers
#define ParseTrace ParseTrace_NCDValueParser
#define ParseAlloc ParseAlloc_NCDValueParser
#define ParseFree ParseFree_NCDValueParser
#define Parse Parse_NCDValueParser

#include "../generated/NCDValueParser_parse.c"
#include "../generated/NCDValueParser_parse.h"

struct parser_state {
    struct parser_out out;
    int error;
    void *parser;
};

static int tokenizer_output (void *user, int token, char *value, size_t value_len, size_t line, size_t line_char);
static int build_value (struct NCDConfig_list *ast, NCDValue *out);
static int build_list_value (struct NCDConfig_list *list, NCDValue *out);
static int build_map_value (struct NCDConfig_list *list, NCDValue *out);

static int tokenizer_output (void *user, int token, char *value, size_t value_len, size_t line, size_t line_char)
{
    struct parser_state *state = user;
    ASSERT(!state->out.out_of_memory)
    ASSERT(!state->out.syntax_error)
    ASSERT(!state->error)
    
    if (token == NCD_ERROR) {
        BLog(BLOG_ERROR, "line %zu, character %zu: tokenizer error", line, line_char);
        goto fail;
    }
    
    struct parser_minor minor;
    minor.str = value;
    minor.len = value_len;
    
    switch (token) {
        case NCD_EOF: {
            Parse(state->parser, 0, minor, &state->out);
        } break;
        
        case NCD_TOKEN_CURLY_OPEN: {
            Parse(state->parser, CURLY_OPEN, minor, &state->out);
        } break;
        
        case NCD_TOKEN_CURLY_CLOSE: {
            Parse(state->parser, CURLY_CLOSE, minor, &state->out);
        } break;
        
        case NCD_TOKEN_COMMA: {
            Parse(state->parser, COMMA, minor, &state->out);
        } break;
        
        case NCD_TOKEN_STRING: {
            Parse(state->parser, STRING, minor, &state->out);
        } break;
        
        case NCD_TOKEN_COLON: {
            Parse(state->parser, COLON, minor, &state->out);
        } break;
        
        case NCD_TOKEN_BRACKET_OPEN: {
            Parse(state->parser, BRACKET_OPEN, minor, &state->out);
        } break;
        
        case NCD_TOKEN_BRACKET_CLOSE: {
            Parse(state->parser, BRACKET_CLOSE, minor, &state->out);
        } break;
        
        default:
            BLog(BLOG_ERROR, "line %zu, character %zu: invalid token", line, line_char);
            free(minor.str);
            goto fail;
    }
    
    if (state->out.syntax_error) {
        BLog(BLOG_ERROR, "line %zu, character %zu: syntax error", line, line_char);
        goto fail;
    }
    
    if (state->out.out_of_memory) {
        BLog(BLOG_ERROR, "line %zu, character %zu: out of memory", line, line_char);
        goto fail;
    }
    
    return 1;
    
fail:
    state->error = 1;
    return 0;
}

static int build_value (struct NCDConfig_list *ast, NCDValue *out)
{
    switch (ast->type) {
        case NCDCONFIG_ARG_STRING: {
            if (!NCDValue_InitStringBin(out, (uint8_t *)ast->string, ast->string_len)) {
                BLog(BLOG_ERROR, "NCDValue_InitStringBin failed");
                return 0;
            }
        } break;
        
        case NCDCONFIG_ARG_LIST: {
            if (!build_list_value(ast->list, out)) {
                return 0;
            }
        } break;
        
        case NCDCONFIG_ARG_MAPLIST: {
            if (!build_map_value(ast->list, out)) {
                return 0;
            }
        } break;
        
        default: ASSERT(0);
    }
    
    return 1;
}

static int build_list_value (struct NCDConfig_list *list, NCDValue *out)
{
    NCDValue list_val;
    NCDValue_InitList(&list_val);
    
    for (struct NCDConfig_list *elem = list; elem; elem = elem->next) {
        NCDValue v;
        
        if (!build_value(elem, &v)) {
            goto fail;
        }
        
        if (!NCDValue_ListAppend(&list_val, v)) {
            BLog(BLOG_ERROR, "NCDValue_ListAppend failed");
            NCDValue_Free(&v);
            goto fail;
        }
    }
    
    *out = list_val;
    return 1;
    
fail:
    NCDValue_Free(&list_val);
    return 0;
}

static int build_map_value (struct NCDConfig_list *list, NCDValue *out)
{
    NCDValue map_val;
    NCDValue_InitMap(&map_val);
    
    for (struct NCDConfig_list *elem = list; elem; elem = elem->next->next) {
        ASSERT(elem->next)
        
        NCDValue key;
        NCDValue val;
        
        if (!build_value(elem, &key)) {
            goto fail;
        }
        
        if (!build_value(elem->next, &val)) {
            NCDValue_Free(&key);
            goto fail;
        }
        
        if (NCDValue_MapFindKey(&map_val, &key)) {
            BLog(BLOG_ERROR, "duplicate map keys");
            NCDValue_Free(&key);
            NCDValue_Free(&val);
            goto fail;
        }
        
        if (!NCDValue_MapInsert(&map_val, key, val)) {
            BLog(BLOG_ERROR, "NCDValue_MapInsert failed");
            NCDValue_Free(&key);
            NCDValue_Free(&val);
            goto fail;
        }
    }
    
    *out = map_val;
    return 1;
    
fail:
    NCDValue_Free(&map_val);
    return 0;
}

int NCDValueParser_Parse (const char *str, size_t str_len, NCDValue *out_value)
{
    int res = 0;
    
    // init parse state
    struct parser_state state;
    state.out.out_of_memory = 0;
    state.out.syntax_error = 0;
    state.out.ast_type = AST_TYPE_NONE;
    state.out.ast_string.str = NULL;
    state.out.ast_list = NULL;
    state.error = 0;
    
    // allocate parser
    if (!(state.parser = ParseAlloc(malloc))) {
        BLog(BLOG_ERROR, "ParseAlloc failed");
        goto out;
    }
    
    // tokenize and parse
    NCDConfigTokenizer_Tokenize((char *)str, str_len, tokenizer_output, &state);
    
    // check for errors
    if (state.error) {
        goto out;
    }
    
    ASSERT(state.out.ast_type == AST_TYPE_STRING || state.out.ast_type == AST_TYPE_LIST ||
           state.out.ast_type == AST_TYPE_MAP)
    
    // convert AST to value
    NCDValue val;
    switch (state.out.ast_type) {
        case AST_TYPE_STRING: {
            if (!NCDValue_InitStringBin(&val, (uint8_t *)state.out.ast_string.str, state.out.ast_string.len)) {
                BLog(BLOG_ERROR, "NCDValue_InitStringBin failed");
                goto out;
            }
        } break;
        
        case AST_TYPE_LIST: {
            if (!build_list_value(state.out.ast_list, &val)) {
                goto out;
            }
        } break;
        
        case AST_TYPE_MAP: {
            if (!build_map_value(state.out.ast_list, &val)) {
                goto out;
            }
        } break;
    }
    
    *out_value = val;
    res = 1;
    
out:
    if (state.parser) {
        ParseFree(state.parser, free);
    }
    free(state.out.ast_string.str);
    NCDConfig_free_list(state.out.ast_list);
    return res;
}

int NCDValParser_Parse (const char *str, size_t str_len, NCDValMem *mem, NCDValRef *out_value)
{
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
