/**
 * @file NCDValue.h
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

#ifndef BADVPN_NCD_NCDVALUE_H
#define BADVPN_NCD_NCDVALUE_H

#include <stddef.h>

#include <structure/LinkedList2.h>

#define NCDVALUE_STRING 1
#define NCDVALUE_LIST 2

typedef struct {
    int type;
    union {
        char *string;
        LinkedList2 list;
    };
} NCDValue;

typedef struct {
    LinkedList2Node list_node;
    NCDValue v;
} NCDListElement;

int NCDValue_InitCopy (NCDValue *o, NCDValue *v);
void NCDValue_Free (NCDValue *o);
int NCDValue_Type (NCDValue *o);

int NCDValue_InitString (NCDValue *o, char *str);
char * NCDValue_StringValue (NCDValue *o);

void NCDValue_InitList (NCDValue *o);
int NCDValue_ListAppend (NCDValue *o, NCDValue v);
size_t NCDValue_ListCount (NCDValue *o);
NCDValue * NCDValue_ListFirst (NCDValue *o);
NCDValue * NCDValue_ListNext (NCDValue *o, NCDValue *ev);
int NCDValue_ListRead (NCDValue *o, int num, ...);
int NCDValue_ListReadHead (NCDValue *o, int num, ...);

#endif
