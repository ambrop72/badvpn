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

// these are implementation details. The interface is defined below.

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

/**
 * Initializes a value memory object.
 * A value memory object holds memory for value structures. Values within
 * the memory are referenced using {@link NCDValRef} objects, which point
 * to values within memory objects.
 * 
 * Values may be added to a memory object using functions such as
 * {@link NCDVal_NewString}, {@link NCDVal_NewList} and {@link NCDVal_NewMap},
 * and {@link NCDVal_NewCopy}, which return references to the new values within
 * the memory object.
 * 
 * It is not possible to remove values from the memory object, or modify existing
 * values other than adding elements to pre-allocated slots in lists and maps.
 * Once a value is added, it will consume memory as long as its memory object
 * exists. This is by design - this code is intended and optimized for constructing
 * and passing around values, not for operating on them in place. In fact, al
 * values within a memory object are stored in a single memory buffer, as an
 * embedded data structure with relativepointers. For example, map values use an
 * embedded AVL tree.
 */
void NCDValMem_Init (NCDValMem *o);

/**
 * Frees a value memory object.
 * All values within the memory object cease to exist, and any {@link NCDValRef}
 * object pointing to them must no longer be used.
 */
void NCDValMem_Free (NCDValMem *o);

/**
 * Does nothing.
 * The value reference object must either point to a valid value within a valid
 * memory object, or must be an invalid reference (all functions operating on
 * {@link NCDValRef} implicitly require that).
 */
void NCDVal_Assert (NCDValRef val);

/**
 * Determines if a value reference is invalid.
 */
int NCDVal_IsInvalid (NCDValRef val);

/**
 * Returns the type of the value reference, which must not be an invalid reference.
 * Possible values are NCDVAL_STRING, NCDVAL_LIST and NCDVAL_MAP.
 */
int NCDVal_Type (NCDValRef val);

/**
 * Returns an invalid reference.
 */
NCDValRef NCDVal_NewInvalid (void);

/**
 * Copies a value into the specified memory object. The source
 * must not be an invalid reference, but may reside in a different
 * memory object.
 * Returns a reference to the copied value. On out of memory, returns
 * an invalid reference.
 */
NCDValRef NCDVal_NewCopy (NCDValMem *mem, NCDValRef val);

/**
 * Compares two values, both of which must not be invalid references.
 * Returns -1, 0 or 1.
 */
int NCDVal_Compare (NCDValRef val1, NCDValRef val2);

/**
 * Converts a value reference to a safe referece format, which remains valid
 * if the memory object is moved (safe references do not contain a pointer
 * to the memory object, unlike {@link NCDValRef} references).
 */
NCDValSafeRef NCDVal_ToSafe (NCDValRef val);

/**
 * Converts a safe value reference to a normal value reference.
 * This should be used to recover references from safe references
 * after the memory object is moved.
 */
NCDValRef NCDVal_FromSafe (NCDValMem *mem, NCDValSafeRef sval);

/**
 * Fixes a value reference after its memory object was moved.
 */
NCDValRef NCDVal_Moved (NCDValMem *mem, NCDValRef val);

/**
 * Determines if a value is a string value.
 * The value reference must not be an invalid reference.
 */
int NCDVal_IsString (NCDValRef val);

/**
 * Determines if a value is a string value which has no null bytes.
 * The value reference must not be an invalid reference.
 */
int NCDVal_IsStringNoNulls (NCDValRef val);

/**
 * Builds a new string value from a null-terminated array of bytes.
 * Equivalent to NCDVal_NewStringBin(mem, data, strlen(data)).
 * Returns a reference to the new value, or an invalid reference
 * on out of memory.
 */
NCDValRef NCDVal_NewString (NCDValMem *mem, const char *data);

/**
 * Builds a new string value.
 * Returns a reference to the new value, or an invalid reference
 * on out of memory.
 */
NCDValRef NCDVal_NewStringBin (NCDValMem *mem, const uint8_t *data, size_t len);

/**
 * Returns a pointer to the data of a string value. An extra null byte
 * is always appended to the actual contents of the string.
 * The value reference must point to a string value.
 */
const char * NCDVal_StringValue (NCDValRef string);

/**
 * Returns the length of the string value, excluding the automatically
 * appended null byte.
 * The value reference must point to a string value.
 */
size_t NCDVal_StringLength (NCDValRef string);

/**
 * Determines if the string value has any null bytes in its contents,
 * i.e. that length > strlen().
 * The value reference must point to a string value.
 */
int NCDVal_StringHasNulls (NCDValRef string);

/**
 * Determines if the string value is equal to the given null-terminated
 * string.
 * The value reference must point to a string value.
 */
int NCDVal_StringEquals (NCDValRef string, const char *data);

/**
 * Determines if a value is a list value.
 * The value reference must not be an invalid reference.
 */
int NCDVal_IsList (NCDValRef val);

/**
 * Builds a new list value. The 'maxcount' argument specifies how
 * many element slots to preallocate. Not more than that many
 * elements may be appended to the list using {@link NCDVal_ListAppend}.
 * Returns a reference to the new value, or an invalid reference
 * on out of memory.
 */
NCDValRef NCDVal_NewList (NCDValMem *mem, size_t maxcount);

/**
 * Appends a value to to the list value.
 * The 'list' reference must point to a list value, and the
 * 'elem' reference must be non-invalid and point to a value within
 * the same memory object as the list.
 * Inserting a value into a list does not in any way change it;
 * internally, the list only points to it.
 */
void NCDVal_ListAppend (NCDValRef list, NCDValRef elem);

/**
 * Returns the number of elements in a list value, i.e. the number
 * of times {@link NCDVal_ListAppend} was called.
 * The 'list' reference must point to a list value.
 */
size_t NCDVal_ListCount (NCDValRef list);

/**
 * Returns the maximum number of elements a list value may contain,
 * i.e. the 'maxcount' argument to {@link NCDVal_NewList}.
 * The 'list' reference must point to a list value.
 */
size_t NCDVal_ListMaxCount (NCDValRef list);

/**
 * Returns a reference to the value at the given position 'pos' in a list,
 * starting with zero.
 * The 'list' reference must point to a list value.
 * The position 'pos' must refer to an existing element, i.e.
 * pos < NCDVal_ListCount().
 */
NCDValRef NCDVal_ListGet (NCDValRef list, size_t pos);

/**
 * Returns references to elements within a list by writing them
 * via (NCDValRef *) variable arguments.
 * If 'num' == NCDVal_ListCount(), succeeds, returing 1 and writing 'num'
 * references, as mentioned.
 * If 'num' != NCDVal_ListCount(), fails, returning 0, without writing any
 * references
 */
int NCDVal_ListRead (NCDValRef list, int num, ...);

/**
 * Like {@link NCDVal_ListRead}, but the list can contain more than 'num'
 * elements.
 */
int NCDVal_ListReadHead (NCDValRef list, int num, ...);

/**
 * Determines if a value is a map value.
 * The value reference must not be an invalid reference.
 */
int NCDVal_IsMap (NCDValRef val);

/**
 * Builds a new map value. The 'maxcount' argument specifies how
 * many entry slots to preallocate. Not more than that many
 * entries may be inserted to the map using {@link NCDVal_MapInsert}.
 * Returns a reference to the new value, or an invalid reference
 * on out of memory.
 */
NCDValRef NCDVal_NewMap (NCDValMem *mem, size_t maxcount);

/**
 * Inserts an entry to the map value.
 * The 'map' reference must point to a map value, and the
 * 'key' and 'val' references must be non-invalid and point to values within
 * the same memory object as the map.
 * Inserting an entry does not in any way change the 'key'and 'val';
 * internally, the map only points to it.
 * You must not modify the key after inserting it into a map. This is because
 * the map builds an embedded AVL tree of entries indexed by keys.
 * If 'key' does not exist in the map, succeeds, returning 1.
 * If 'key' already exists in the map, fails, returning 0.
 */
int NCDVal_MapInsert (NCDValRef map, NCDValRef key, NCDValRef val);

/**
 * Returns the number of entries in a map value, i.e. the number
 * of times {@link NCDVal_MapInsert} was called successfully.
 * The 'map' reference must point to a map value.
 */
size_t NCDVal_MapCount (NCDValRef map);

/**
 * Returns the maximum number of entries a map value may contain,
 * i.e. the 'maxcount' argument to {@link NCDVal_NewMap}.
 * The 'map' reference must point to a map value.
 */
size_t NCDVal_MapMaxCount (NCDValRef map);

/**
 * Determines if a map entry reference is invalid. This is used in combination
 * with the map iteration functions to detect the end of iteration.
 */
int NCDVal_MapElemInvalid (NCDValMapElem me);

/**
 * Returns a reference to the first entry in a map, with respect to some
 * arbitrary order.
 * If the map is empty, returns an invalid map entry reference.
 */
NCDValMapElem NCDVal_MapFirst (NCDValRef map);

/**
 * Returns a reference to the entry in a map that follows the entry referenced
 * by 'me', with respect to some arbitrary order.
 * The 'me' argument must be a non-invalid reference to an entry in the map.
 * If 'me' is the last entry, returns an invalid map entry reference.
 */
NCDValMapElem NCDVal_MapNext (NCDValRef map, NCDValMapElem me);

/**
 * Like {@link NCDVal_MapFirst}, but with respect to the order defined by
 * {@link NCDVal_Compare}.
 * Ordered iteration is slower and should only be used when needed.
 */
NCDValMapElem NCDVal_MapOrderedFirst (NCDValRef map);

/**
 * Like {@link NCDVal_MapNext}, but with respect to the order defined by
 * {@link NCDVal_Compare}.
 * Ordered iteration is slower and should only be used when needed.
 */
NCDValMapElem NCDVal_MapOrderedNext (NCDValRef map, NCDValMapElem me);

/**
 * Returns a reference to the key of the map entry referenced by 'me'.
 * The 'me' argument must be a non-invalid reference to an entry in the map.
 */
NCDValRef NCDVal_MapElemKey (NCDValRef map, NCDValMapElem me);

/**
 * Returns a reference to the value of the map entry referenced by 'me'.
 * The 'me' argument must be a non-invalid reference to an entry in the map.
 */
NCDValRef NCDVal_MapElemVal (NCDValRef map, NCDValMapElem me);

/**
 * Looks for a key in the map. The 'key' reference must be a non-invalid
 * value reference, and may point to a value in a different memory object
 * than the map.
 * If the key exists in the map, returns a reference to the corresponding
 * map entry.
 * If the key does not exist, returns an invalid map entry reference.
 */
NCDValMapElem NCDVal_MapFindKey (NCDValRef map, NCDValRef key);

#endif
