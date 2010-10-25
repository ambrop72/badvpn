/**
 * @file BPredicate_parser.h
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
 * {@link BPredicate} parser definitions.
 */

#ifndef BADVPN_PREDICATE_BPREDICATE_PARSER_H
#define BADVPN_PREDICATE_BPREDICATE_PARSER_H

#include <predicate/BPredicate_internal.h>

#include <generated/bison_BPredicate.h>
#include <generated/flex_BPredicate.h>

// implemented in BPredicate.c
void yyerror (YYLTYPE *yylloc, yyscan_t scanner, struct predicate_node **result, char *str);

// implemented in parser
int yyparse (void *scanner, struct predicate_node **result);

#endif
