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

#include <misc/debug.h>
#include <misc/string_begins_with.h>

#include <ncdconfig/NCDConfigTokenizer.h>

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
        char dec[NCD_MAX_SIZE + 1];
        
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
            while (l < left) {
                if (!is_name_char(str[l])) {
                    break;
                }
                l++;
            }
            
            // check size
            if (l > NCD_MAX_SIZE) {
                error = 1;
                goto out;
            }
            
            // copy and terminate
            memcpy(dec, str, l);
            dec[l] = '\0';
            
            if (!strcmp(dec, "process")) {
                token = NCD_TOKEN_PROCESS;
            }
            else if (!strcmp(dec, "template")) {
                token = NCD_TOKEN_TEMPLATE;
            }
            else {
                token = NCD_TOKEN_NAME;
                token_val = dec;
            }
        }
        else if (*str == '"') {
            size_t dec_len = 0;
            
            // decode string on the fly
            l = 1;
            while (l < left) {
                char dec_ch;
                
                if (str[l] == '\\') {
                    if (!(l + 1 < left)) {
                        error = 1;
                        goto out;
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
                
                // check size
                if (dec_len == NCD_MAX_SIZE) {
                    error = 1;
                    goto out;
                }
                
                // append decoded char
                dec[dec_len] = dec_ch;
                dec_len++;
            }
            
            // make sure closing quote was found
            if (l == left) {
                error = 1;
                goto out;
            }
            
            l++;
            
            // terminate
            dec[dec_len] = '\0';
            
            token = NCD_TOKEN_STRING;
            token_val = dec;
        }
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
