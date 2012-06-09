/**
 * @file NCDVal.h
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

#ifndef BADVPN_NCDVAL_H
#define BADVPN_NCDVAL_H

#include <stddef.h>
#include <stdint.h>

#include <misc/debug.h>
#include <structure/CAvl.h>

#define NCDVAL_FASTBUF_SIZE 64
#define NCDVAL_FIRST_SIZE 256

#define NCDVAL_MAXIDX INT_MAX

typedef int NCDVal__idx;

typedef struct {
    char *buf;
    NCDVal__idx size;
    NCDVal__idx used;
    char fastbuf[NCDVAL_FASTBUF_SIZE];
} NCDValMem;

typedef struct {
    NCDValMem *mem;
    NCDVal__idx idx;
} NCDValRef;

typedef struct {
    NCDVal__idx idx;
} NCDValSafeRef;

struct NCDVal__string {
    uint8_t type;
    NCDVal__idx length;
    char data[];
};

struct NCDVal__list {
    uint8_t type;
    NCDVal__idx maxcount;
    NCDVal__idx count;
    NCDVal__idx elem_indices[];
};

struct NCDVal__mapelem {
    NCDVal__idx key_idx;
    NCDVal__idx val_idx;
    NCDVal__idx tree_link[2];
    NCDVal__idx tree_parent;
    int8_t tree_balance;
};

typedef struct NCDVal__mapelem NCDVal__maptree_entry;
typedef NCDValMem *NCDVal__maptree_arg;

#include "NCDVal_maptree.h"
#include <structure/CAvl_decl.h>

struct NCDVal__map {
    uint8_t type;
    NCDVal__idx maxcount;
    NCDVal__idx count;
    NCDVal__MapTree tree;
    struct NCDVal__mapelem elems[];
};

typedef struct {
    NCDVal__idx elemidx;
} NCDValMapElem;

//

#define NCDVAL_STRING 1
#define NCDVAL_LIST 2
#define NCDVAL_MAP 3

void NCDValMem_Init (NCDValMem *o);
void NCDValMem_Free (NCDValMem *o);

void NCDVal_Assert (NCDValRef val);
int NCDVal_IsInvalid (NCDValRef val);
int NCDVal_Type (NCDValRef val);
NCDValRef NCDVal_NewInvalid (void);
NCDValRef NCDVal_NewCopy (NCDValMem *mem, NCDValRef val);
int NCDVal_Compare (NCDValRef val1, NCDValRef val2);

NCDValSafeRef NCDVal_ToSafe (NCDValRef val);
NCDValRef NCDVal_FromSafe (NCDValMem *mem, NCDValSafeRef sval);
NCDValRef NCDVal_Moved (NCDValMem *mem, NCDValRef val);

int NCDVal_IsString (NCDValRef val);
int NCDVal_IsStringNoNulls (NCDValRef val);
NCDValRef NCDVal_NewString (NCDValMem *mem, const char *data);
NCDValRef NCDVal_NewStringBin (NCDValMem *mem, const uint8_t *data, size_t len);
const char * NCDVal_StringValue (NCDValRef string);
size_t NCDVal_StringLength (NCDValRef string);
int NCDVal_StringHasNulls (NCDValRef string);
int NCDVal_StringEquals (NCDValRef string, const char *data);

int NCDVal_IsList (NCDValRef val);
NCDValRef NCDVal_NewList (NCDValMem *mem, size_t maxcount);
void NCDVal_ListAppend (NCDValRef list, NCDValRef elem);
size_t NCDVal_ListCount (NCDValRef list);
size_t NCDVal_ListMaxCount (NCDValRef list);
NCDValRef NCDVal_ListGet (NCDValRef list, size_t pos);
int NCDVal_ListRead (NCDValRef list, int num, ...);
int NCDVal_ListReadHead (NCDValRef list, int num, ...);

int NCDVal_IsMap (NCDValRef val);
NCDValRef NCDVal_NewMap (NCDValMem *mem, size_t maxcount);
int NCDVal_MapInsert (NCDValRef map, NCDValRef key, NCDValRef val);
size_t NCDVal_MapCount (NCDValRef map);
size_t NCDVal_MapMaxCount (NCDValRef map);
int NCDVal_MapElemInvalid (NCDValMapElem me);
NCDValMapElem NCDVal_MapFirst (NCDValRef map);
NCDValMapElem NCDVal_MapNext (NCDValRef map, NCDValMapElem me);
NCDValRef NCDVal_MapElemKey (NCDValRef map, NCDValMapElem me);
NCDValRef NCDVal_MapElemVal (NCDValRef map, NCDValMapElem me);
NCDValMapElem NCDVal_MapFindKey (NCDValRef map, NCDValRef key);

#endif
