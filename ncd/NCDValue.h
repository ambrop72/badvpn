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

#include <misc/debug.h>
#include <structure/LinkedList2.h>

#define NCDVALUE_STRING 1
#define NCDVALUE_LIST 2

typedef struct {
    int type;
    union {
        char *string;
        struct {
            LinkedList2 list;
            size_t list_count;
        };
    };
} NCDValue;

typedef struct {
    LinkedList2Node list_node;
    NCDValue v;
} NCDListElement;

int NCDValue_InitCopy (NCDValue *o, NCDValue *v) WARN_UNUSED;
void NCDValue_Free (NCDValue *o);
int NCDValue_Type (NCDValue *o);

int NCDValue_InitString (NCDValue *o, const char *str) WARN_UNUSED;
char * NCDValue_StringValue (NCDValue *o);

void NCDValue_InitList (NCDValue *o);
int NCDValue_ListAppend (NCDValue *o, NCDValue v) WARN_UNUSED;
int NCDValue_ListAppendList (NCDValue *o, NCDValue l) WARN_UNUSED;
size_t NCDValue_ListCount (NCDValue *o);
NCDValue * NCDValue_ListFirst (NCDValue *o);
NCDValue * NCDValue_ListNext (NCDValue *o, NCDValue *ev);
int NCDValue_ListRead (NCDValue *o, int num, ...) WARN_UNUSED;
int NCDValue_ListReadHead (NCDValue *o, int num, ...) WARN_UNUSED;

#endif
