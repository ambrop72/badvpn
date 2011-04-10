/**
 * @file NCDConfigParser.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <misc/debug.h>
#include <ncdconfig/NCDConfigTokenizer.h>

#include <ncdconfig/NCDConfigParser.h>

#include "../generated/NCDConfigParser_parse.c"
#include "../generated/NCDConfigParser_parse.h"

struct parser_state {
    struct parser_out out;
    int error;
    void *parser;
};

static int tokenizer_output (void *user, int token, char *value, size_t position)
{
    struct parser_state *state = (struct parser_state *)user;
    ASSERT(!state->out.out_of_memory)
    ASSERT(!state->out.syntax_error)
    ASSERT(!state->error)
    
    if (token == NCD_ERROR) {
        DEBUG("tokenizer error at %zu", position);
        state->error = 1;
        return 0;
    }
    
    switch (token) {
        case NCD_EOF: {
            Parse(state->parser, 0, NULL, &state->out);
        } break;
        
        case NCD_TOKEN_CURLY_OPEN: {
            Parse(state->parser, CURLY_OPEN, NULL, &state->out);
        } break;
        
        case NCD_TOKEN_CURLY_CLOSE: {
            Parse(state->parser, CURLY_CLOSE, NULL, &state->out);
        } break;
        
        case NCD_TOKEN_ROUND_OPEN: {
            Parse(state->parser, ROUND_OPEN, NULL, &state->out);
        } break;
        
        case NCD_TOKEN_ROUND_CLOSE: {
            Parse(state->parser, ROUND_CLOSE, NULL, &state->out);
        } break;
        
        case NCD_TOKEN_SEMICOLON: {
            Parse(state->parser, SEMICOLON, NULL, &state->out);
        } break;
        
        case NCD_TOKEN_DOT: {
            Parse(state->parser, DOT, NULL, &state->out);
        } break;
        
        case NCD_TOKEN_COMMA: {
            Parse(state->parser, COMMA, NULL, &state->out);
        } break;
        
        case NCD_TOKEN_ARROW: {
            Parse(state->parser, ARROW, NULL, &state->out);
        } break;
        
        case NCD_TOKEN_PROCESS: {
            Parse(state->parser, PROCESS, NULL, &state->out);
        } break;
        
        case NCD_TOKEN_TEMPLATE: {
            Parse(state->parser, TEMPLATE, NULL, &state->out);
        } break;
        
        case NCD_TOKEN_NAME: {
            char *v = malloc(strlen(value) + 1);
            if (!v) {
                state->out.out_of_memory = 1;
                break;
            }
            strcpy(v, value);
            Parse(state->parser, NAME, v, &state->out);
        } break;
        
        case NCD_TOKEN_STRING: {
            char *v = malloc(strlen(value) + 1);
            if (!v) {
                state->out.out_of_memory = 1;
                break;
            }
            strcpy(v, value);
            Parse(state->parser, STRING, v, &state->out);
        } break;
        
        default:
            ASSERT(0);
    }
    
    // if we got syntax error, stop parsing
    if (state->out.syntax_error) {
        DEBUG("syntax error at %zu", position);
        state->error = 1;
        return 0;
    }
    
    if (state->out.out_of_memory) {
        DEBUG("out of memory at %zu", position);
        state->error = 1;
        return 0;
    }
    
    return 1;
}

int NCDConfigParser_Parse (char *config, size_t config_len, struct NCDConfig_interfaces **out_ast)
{
    struct parser_state state;
    
    state.out.out_of_memory = 0;
    state.out.syntax_error = 0;
    state.out.ast = NULL;
    state.error = 0;
    
    if (!(state.parser = ParseAlloc(malloc))) {
        DEBUG("ParseAlloc failed");
        return 0;
    }
    
    // tokenize and parse
    NCDConfigTokenizer_Tokenize(config, config_len, tokenizer_output, &state);
    
    if (state.error) {
        ParseFree(state.parser, free);
        NCDConfig_free_interfaces(state.out.ast);
        return 0;
    }
    
    ParseFree(state.parser, free);
    
    *out_ast = state.out.ast;
    return 1;
}
