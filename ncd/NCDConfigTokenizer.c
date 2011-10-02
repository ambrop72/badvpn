/**
 * @file NCDConfigTokenizer.c
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

#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#include <misc/debug.h>
#include <misc/string_begins_with.h>
#include <misc/balloc.h>
#include <misc/expstring.h>
#include <base/BLog.h>

#include <ncd/NCDConfigTokenizer.h>

#include <generated/blog_channel_NCDConfigTokenizer.h>

static int is_name_char (char c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
}

static int is_name_first_char (char c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
}

static int is_space_char (char c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static int string_equals (char *str, int str_len, char *needle)
{
    return (str_len == strlen(needle) && !memcmp(str, needle, str_len));
}

void NCDConfigTokenizer_Tokenize (char *str, size_t left, NCDConfigTokenizer_output output, void *user)
{
    size_t position = 0;
    
    while (left > 0) {
        size_t l;
        int error = 0;
        int token;
        void *token_val = NULL;
        
        if (*str == '#') {
            l = 1;
            while (l < left && str[l] != '\n') {
                l++;
            }
            token = 0;
        }
        else if (l = data_begins_with(str, left, "{")) {
            token = NCD_TOKEN_CURLY_OPEN;
        }
        else if (l = data_begins_with(str, left, "}")) {
            token = NCD_TOKEN_CURLY_CLOSE;
        }
        else if (l = data_begins_with(str, left, "(")) {
            token = NCD_TOKEN_ROUND_OPEN;
        }
        else if (l = data_begins_with(str, left, ")")) {
            token = NCD_TOKEN_ROUND_CLOSE;
        }
        else if (l = data_begins_with(str, left, ";")) {
            token = NCD_TOKEN_SEMICOLON;
        }
        else if (l = data_begins_with(str, left, ".")) {
            token = NCD_TOKEN_DOT;
        }
        else if (l = data_begins_with(str, left, ",")) {
            token = NCD_TOKEN_COMMA;
        }
        else if (l = data_begins_with(str, left, "->")) {
            token = NCD_TOKEN_ARROW;
        }
        else if (is_name_first_char(*str)) {
            l = 1;
            while (l < left && is_name_char(str[l])) {
                l++;
            }
            
            // allocate buffer
            bsize_t bufsize = bsize_add(bsize_fromsize(l), bsize_fromint(1));
            char *buf;
            if (bufsize.is_overflow || !(buf = malloc(bufsize.value))) {
                BLog(BLOG_ERROR, "malloc failed");
                error = 1;
                goto out;
            }
            
            // copy and terminate
            memcpy(buf, str, l);
            buf[l] = '\0';
            
            if (!strcmp(buf, "process")) {
                token = NCD_TOKEN_PROCESS;
                free(buf);
            }
            else if (!strcmp(buf, "template")) {
                token = NCD_TOKEN_TEMPLATE;
                free(buf);
            }
            else {
                token = NCD_TOKEN_NAME;
                token_val = buf;
            }
        }
        else if (*str == '"') do {
            // init string
            ExpString estr;
            if (!ExpString_Init(&estr)) {
                BLog(BLOG_ERROR, "ExpString_Init failed");
                goto string_fail0;
            }
            
            // skip start quote
            l = 1;
            
            // decode string
            while (l < left) {
                char dec_ch;
                
                // get character
                if (str[l] == '\\') {
                    if (left - l < 2) {
                        BLog(BLOG_ERROR, "escape character found in string but nothing follows");
                        goto string_fail1;
                    }
                    
                    dec_ch = str[l + 1];
                    l += 2;
                }
                else if (str[l] == '"') {
                    break;
                }
                else {
                    dec_ch = str[l];
                    l++;
                }
                
                // string cannot contain zeros bytes
                if (dec_ch == '\0') {
                    BLog(BLOG_ERROR, "string contains zero byte");
                    goto string_fail1;
                }
                
                // append character to string
                if (!ExpString_AppendChar(&estr, dec_ch)) {
                    BLog(BLOG_ERROR, "ExpString_AppendChar failed");
                    goto string_fail1;
                }
            }
            
            // make sure ending quote was found
            if (l == left) {
                BLog(BLOG_ERROR, "missing ending quote for string");
                goto string_fail1;
            }
            
            // skip ending quote
            l++;
            
            token = NCD_TOKEN_STRING;
            token_val = ExpString_Get(&estr);
            break;
            
        string_fail1:
            ExpString_Free(&estr);
        string_fail0:
            error = 1;
        } while (0);
        else if (is_space_char(*str)) {
            token = 0;
            l = 1;
        }
        else {
            error = 1;
        }
        
    out:
        // report error
        if (error) {
            output(user, NCD_ERROR, NULL, position);
            return;
        }
        
        // output token
        if (token) {
            if (!output(user, token, token_val, position)) {
                return;
            }
        }
        
        str += l;
        left -= l;
        position += l;
    }
    
    output(user, NCD_EOF, NULL, position);
}
