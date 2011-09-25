/**
 * @file ncd_tokenizer_test.c
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <misc/debug.h>
#include <base/BLog.h>
#include <ncd/NCDConfigTokenizer.h>

int error;

static int tokenizer_output (void *user, int token, char *value, size_t pos)
{
    if (token == NCD_ERROR) {
        printf("error at %zd\n", pos);
        error = 1;
        return 0;
    }
    
    switch (token) {
        case NCD_EOF:
            printf("eof\n");
            break;
        case NCD_TOKEN_CURLY_OPEN:
            printf("curly_open\n");
            break;
        case NCD_TOKEN_CURLY_CLOSE:
            printf("curly_close\n");
            break;
        case NCD_TOKEN_ROUND_OPEN:
            printf("round_open\n");
            break;
        case NCD_TOKEN_ROUND_CLOSE:
            printf("round_close\n");
            break;
        case NCD_TOKEN_SEMICOLON:
            printf("semicolon\n");
            break;
        case NCD_TOKEN_DOT:
            printf("dot\n");
            break;
        case NCD_TOKEN_COMMA:
            printf("comma\n");
            break;
        case NCD_TOKEN_PROCESS:
            printf("process\n");
            break;
        case NCD_TOKEN_NAME:
            printf("name %s\n", value);
            break;
        case NCD_TOKEN_STRING:
            printf("string %s\n", value);
            break;
        default:
            ASSERT(0);
    }
    
    return 1;
}

int main (int argc, char **argv)
{
    if (argc < 1) {
        return 1;
    }
    
    if (argc != 2) {
        printf("Usage: %s <string>\n", argv[0]);
        return 1;
    }
    
    BLog_InitStdout();
    
    error = 0;
    
    NCDConfigTokenizer_Tokenize(argv[1], strlen(argv[1]), tokenizer_output, NULL);
    
    if (error) {
        return 1;
    }
    
    return 0;
}
