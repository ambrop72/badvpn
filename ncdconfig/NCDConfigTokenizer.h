/**
 * @file NCDConfigTokenizer.h
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

#ifndef BADVPN_NCDCONFIG_NCDCONFIGTOKENIZER_H
#define BADVPN_NCDCONFIG_NCDCONFIGTOKENIZER_H

#define NCD_ERROR -1
#define NCD_EOF 0
#define NCD_TOKEN_CURLY_OPEN 1
#define NCD_TOKEN_CURLY_CLOSE 2
#define NCD_TOKEN_ROUND_OPEN 3
#define NCD_TOKEN_ROUND_CLOSE 4
#define NCD_TOKEN_SEMICOLON 5
#define NCD_TOKEN_DOT 6
#define NCD_TOKEN_COMMA 7
#define NCD_TOKEN_PROCESS 8
#define NCD_TOKEN_NAME 9
#define NCD_TOKEN_STRING 10

#define NCD_MAX_SIZE 128

typedef int (*NCDConfigTokenizer_output) (void *user, int token, char *value, size_t position);

void NCDConfigTokenizer_Tokenize (char *str, size_t str_len, NCDConfigTokenizer_output output, void *user);

#endif
