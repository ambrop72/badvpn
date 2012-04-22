/**
 * @file NCDValue.h
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

#ifndef BADVPN_NCD_NCDVALUE_H
#define BADVPN_NCD_NCDVALUE_H

#include <stddef.h>
#include <stdint.h>

#include <misc/debug.h>
#include <structure/LinkedList2.h>
#include <structure/BAVL.h>

#define NCDVALUE_STRING 1
#define NCDVALUE_LIST 2
#define NCDVALUE_MAP 3

typedef struct {
    int type;
    union {
        struct {
            uint8_t *string;
            size_t string_len;
        };
        struct {
            LinkedList2 list;
            size_t list_count;
        };
        struct {
            BAVL map_tree;
            size_t map_count;
        };
    };
} NCDValue;

typedef struct {
    LinkedList2Node list_node;
    NCDValue v;
} NCDListElement;

typedef struct {
    BAVLNode map_tree_node;
    NCDValue key;
    NCDValue val;
} NCDMapElement;

int NCDValue_InitCopy (NCDValue *o, NCDValue *v) WARN_UNUSED;
void NCDValue_Free (NCDValue *o);
int NCDValue_Type (NCDValue *o);

int NCDValue_InitString (NCDValue *o, const char *str) WARN_UNUSED;
int NCDValue_InitStringBin (NCDValue *o, const uint8_t *str, size_t len) WARN_UNUSED;
char * NCDValue_StringValue (NCDValue *o);
size_t NCDValue_StringLength (NCDValue *o);

void NCDValue_InitList (NCDValue *o);
int NCDValue_ListAppend (NCDValue *o, NCDValue v) WARN_UNUSED;
int NCDValue_ListAppendList (NCDValue *o, NCDValue l) WARN_UNUSED;
size_t NCDValue_ListCount (NCDValue *o);
NCDValue * NCDValue_ListFirst (NCDValue *o);
NCDValue * NCDValue_ListNext (NCDValue *o, NCDValue *ev);
int NCDValue_ListRead (NCDValue *o, int num, ...) WARN_UNUSED;
int NCDValue_ListReadHead (NCDValue *o, int num, ...) WARN_UNUSED;
NCDValue * NCDValue_ListGet (NCDValue *o, size_t pos);
NCDValue NCDValue_ListShift (NCDValue *o);
NCDValue NCDValue_ListRemove (NCDValue *o, NCDValue *ev);

void NCDValue_InitMap (NCDValue *o);
size_t NCDValue_MapCount (NCDValue *o);
NCDValue * NCDValue_MapFirstKey (NCDValue *o);
NCDValue * NCDValue_MapNextKey (NCDValue *o, NCDValue *ekey);
NCDValue * NCDValue_MapKeyValue (NCDValue *o, NCDValue *ekey);
NCDValue * NCDValue_MapFindKey (NCDValue *o, NCDValue *key);
NCDValue * NCDValue_MapInsert (NCDValue *o, NCDValue key, NCDValue val) WARN_UNUSED;
void NCDValue_MapRemove (NCDValue *o, NCDValue *ekey, NCDValue *out_key, NCDValue *out_val);
NCDValue * NCDValue_MapFindValueByString (NCDValue *o, const char *key_str);

int NCDValue_Compare (NCDValue *o, NCDValue *v);

#endif
