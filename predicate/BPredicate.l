/**
 * @file BPredicate.l
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
 * 
 * @section DESCRIPTION
 * 
 * {@link BPredicate} lexer file.
 */

%{

#include <string.h>
#include <stdlib.h>

#include <predicate/LexMemoryBufferInput.h>
#include <predicate/BPredicate_internal.h>

#include <generated/bison_BPredicate.h>

#define YY_EXTRA_TYPE LexMemoryBufferInput *

#define YY_INPUT(buffer, res, max_size) \
    int bytes_read = LexMemoryBufferInput_Read(yyget_extra(yyscanner), buffer, max_size); \
    res = (bytes_read == 0 ? YY_NULL : bytes_read);

%}

%option reentrant stack noyywrap bison-bridge bison-locations

%%
\(              return SPAR;
\)              return EPAR;
,               return COMMA;
AND             return AND;
OR              return OR;
NOT             return NOT;
true            return CONSTANT_TRUE;
false           return CONSTANT_FALSE;
[a-zA-Z0-9_]+   {
                    int l = strlen(yytext);
                    char *p = malloc(l + 1);
                    if (p) {
                        memcpy(p, yytext, l);
                        p[l] = '\0';
                    }
                    yylval->text = p;
                    return NAME;
                }
\"[^\"]*\"      {
                    int l = strlen(yytext);
                    char *p = malloc(l - 1);
                    if (p) {
                        memcpy(p, yytext + 1, l - 2);
                        p[l - 2] = '\0';
                    }
                    yylval->text = p;
                    return STRING;
                }
[ \t\n]+        ;
.               LexMemoryBufferInput_SetError(yyget_extra(yyscanner)); return 0; // remember failure and report EOF
%%
