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
#include <ncd/NCDStringIndex.h>
#include <ncd/NCDRefTarget.h>

// these are implementation details. The interface is defined below.

#define NCDVAL_FASTBUF_SIZE 60
#define NCDVAL_FIRST_SIZE 256

#define NCDVAL_MAXIDX INT_MAX
#define NCDVAL_MINIDX INT_MIN

typedef int NCDVal__idx;

typedef struct {
    char *buf;
    NCDVal__idx size;
    NCDVal__idx used;
    NCDVal__idx first_ref;
    char fastbuf[NCDVAL_FASTBUF_SIZE];
} NCDValMem;

typedef struct {
    NCDValMem *mem;
    NCDVal__idx idx;
} NCDValRef;

typedef struct {
    NCDVal__idx idx;
} NCDValSafeRef;

struct NCDVal__ref {
    NCDVal__idx next;
    NCDRefTarget *target;
};

struct NCDVal__string {
    int type;
    NCDVal__idx length;
    char data[];
};

struct NCDVal__list {
    int type;
    NCDVal__idx maxcount;
    NCDVal__idx count;
    NCDVal__idx elem_indices[];
};

struct NCDVal__mapelem {
    NCDVal__idx key_idx;
    NCDVal__idx val_idx;
    NCDVal__idx tree_child[2];
    NCDVal__idx tree_parent;
    int8_t tree_balance;
};

struct NCDVal__idstring {
    int type;
    NCD_string_id_t string_id;
    NCDStringIndex *string_index;
};

struct NCDVal__externalstring {
    int type;
    const char *data;
    size_t length;
    struct NCDVal__ref ref;
};

typedef struct NCDVal__mapelem NCDVal__maptree_entry;
typedef NCDValMem *NCDVal__maptree_arg;

#include "NCDVal_maptree.h"
#include <structure/CAvl_decl.h>

struct NCDVal__map {
    int type;
    NCDVal__idx maxcount;
    NCDVal__idx count;
    NCDVal__MapTree tree;
    struct NCDVal__mapelem elems[];
};

typedef struct {
    NCDVal__idx elemidx;
} NCDValMapElem;

#define NCDVAL_INSTR_PLACEHOLDER 0
#define NCDVAL_INSTR_REINSERT 1

struct NCDVal__instr {
    int type;
    union {
        struct {
            NCDVal__idx plid;
            NCDVal__idx plidx;
        } placeholder;
        struct {
            NCDVal__idx mapidx;
            NCDVal__idx elempos;
        } reinsert;
    };
};

typedef struct {
    struct NCDVal__instr *instrs;
    size_t num_instrs;
} NCDValReplaceProg;

typedef struct {
    char *data;
    int is_allocated;
} NCDValNullTermString;

//

#define NCDVAL_STRING 1
#define NCDVAL_LIST 2
#define NCDVAL_MAP 3
#define NCDVAL_PLACEHOLDER 4

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
 * Initializes the memory object to be a copy of an existing memory object.
 * Value references from the original may be used if they are first turned
 * to {@link NCDValSafeRef} using {@link NCDVal_ToSafe} and back to
 * {@link NCDValRef} using {@link NCDVal_FromSafe} with the new memory object
 * specified. Alternatively, {@link NCDVal_Moved} can be used.
 * Returns 1 on success and 0 on failure.
 */
int NCDValMem_InitCopy (NCDValMem *o, NCDValMem *other) WARN_UNUSED;

/**
 * Does nothing.
 * The value reference object must either point to a valid value within a valid
 * memory object, or must be an invalid reference (most functions operating on
 * {@link NCDValRef} implicitly require that).
 */
void NCDVal_Assert (NCDValRef val);

/**
 * Determines if a value reference is invalid.
 */
int NCDVal_IsInvalid (NCDValRef val);

/**
 * Determines if a value is a placeholder value.
 * The value reference must not be an invalid reference.
 */
int NCDVal_IsPlaceholder (NCDValRef val);

/**
 * Returns the type of the value reference, which must not be an invalid reference.
 * Possible values are NCDVAL_STRING, NCDVAL_LIST, NCDVAL_MAP and NCDVAL_PLACEHOLDER.
 * The placeholder type is only used internally in the interpreter for argument
 * resolution, and is never seen by modules; see {@link NCDVal_NewPlaceholder}.
 */
int NCDVal_Type (NCDValRef val);

/**
 * Returns an invalid reference.
 * An invalid reference must not be passed to any function here, except:
 *   {@link NCDVal_Assert}, {@link NCDVal_IsInvalid}, {@link NCDVal_ToSafe},
 *   {@link NCDVal_FromSafe}, {@link NCDVal_Moved}.
 */
NCDValRef NCDVal_NewInvalid (void);

/**
 * Returns a new placeholder value reference. A placeholder value is a valid value
 * containing an integer placeholder identifier.
 * This always succeeds; however, the caller must ensure the identifier is
 * non-negative and satisfies (NCDVAL_MINIDX + plid < -1).
 * 
 * The placeholder type is only used internally in the interpreter for argument
 * resolution, and is never seen by modules. Also see {@link NCDPlaceholderDb}.
 */
NCDValRef NCDVal_NewPlaceholder (NCDValMem *mem, int plid);

/**
 * Returns the indentifier of a placeholder value.
 * The value reference must point to a placeholder value.
 */
int NCDVal_PlaceholderId (NCDValRef val);

/**
 * Copies a value into the specified memory object. The source
 * must not be an invalid reference, however it may reside in any memory
 * object (including 'mem').
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
 * Determines if a value is an ID-string value. See {@link NCDVal_NewIdString}
 * for an explanation of ID-string values.
 * The value reference must not be an invalid reference.
 */
int NCDVal_IsIdString (NCDValRef val);

/**
 * Determines if a value is an external string value.
 * See {@link NCDVal_NewExternalString} for an explanation of external
 * string values.
 * The value reference must not be an invalid reference.
 */
int NCDVal_IsExternalString (NCDValRef val);

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
 * WARNING: The buffer passed must NOT be part of any value in the
 * memory object specified. In particular, you may NOT use this
 * function to copy a string that resides in the same memory object.
 */
NCDValRef NCDVal_NewString (NCDValMem *mem, const char *data);

/**
 * Builds a new string value.
 * Returns a reference to the new value, or an invalid reference
 * on out of memory.
 * WARNING: The buffer passed must NOT be part of any value in the
 * memory object specified. In particular, you may NOT use this
 * function to copy a string that resides in the same memory object.
 */
NCDValRef NCDVal_NewStringBin (NCDValMem *mem, const uint8_t *data, size_t len);

/**
 * Builds a new string value of the given length with undefined contents.
 * You can define the contents of the string later by copying to the address
 * returned by {@link NCDVal_StringData}.
 */
NCDValRef NCDVal_NewStringUninitialized (NCDValMem *mem, size_t len);

/**
 * Builds a new ID-string value.
 * Returns a reference to the new value, or an invalid reference
 * on out of memory.
 * 
 * An ID-string value is a special kind of string value which is represented
 * efficiently as a string identifier via {@link NCDStringIndex}. An ID-string
 * is also a string and is transparent for use. For example, for an ID-string,
 * {@link NCDVal_Type} still returns NCDVAL_STRING, {@link NCDVal_IsString}
 * returns 1, and {@link NCDVal_StringData} and {@link NCDVal_StringLength}
 * both work. The only way to distinguish an ID-string from a non-ID string is
 * by calling {@link NCDVal_IsIdString}.
 */
NCDValRef NCDVal_NewIdString (NCDValMem *mem, NCD_string_id_t string_id,
                              NCDStringIndex *string_index);

/**
 * Builds a new string value pointing to the given external data. A reference to
 * the external data is taken using {@link NCDRefTarget}. The data must not change
 * while the reference is being held. Like ID-strings, external strings are
 * transparent for use. An external string can be recognized using
 * {@link NCDVal_IsExternalString}.
 * 
 * Returns a reference to the new value, or an invalid reference
 * on out of memory.
 */
NCDValRef NCDVal_NewExternalString (NCDValMem *mem, const char *data, size_t len,
                                    NCDRefTarget *ref_target);

/**
 * Returns a pointer to the data of a string value.
 * WARNING: the string data may not be null-terminated. To get a null-terminated
 * version, use {@link NCDVal_StringNullTerminate}.
 * The value reference must point to a string value.
 */
const char * NCDVal_StringData (NCDValRef string);

/**
 * Returns the length of the string value.
 * The value reference must point to a string value.
 */
size_t NCDVal_StringLength (NCDValRef string);

/**
 * Produces a null-terminated version of a string value. On success, the result is
 * stored into a {@link NCDValNullTermString} structure, and the null-terminated
 * string is available via its 'data' member. This function may either simply pass
 * through the data pointer as returned by {@link NCDVal_StringData} (if the string
 * is known to be null-terminated) or produce a null-terminated dynamically allocated
 * copy.
 * On success, {@link NCDValNullTermString_Free} should be called to release any allocated
 * memory when the null-terminated string is no longer needed. This must be called before
 * the memory object is freed, because it may point to data inside the memory object.
 * It is guaranteed that *out is not modified on failure.
 * Returns 1 on success and 0 on failure.
 */
int NCDVal_StringNullTerminate (NCDValRef string, NCDValNullTermString *out) WARN_UNUSED;

/**
 * Returns a dummy {@link NCDValNullTermString} which can be freed using
 * {@link NCDValNullTermString_Free}, but need not be.
 */
NCDValNullTermString NCDValNullTermString_NewDummy (void);

/**
 * Releases any memory which was dynamically allocated by {@link NCDVal_StringNullTerminate}
 * to null-terminate a string.
 */
void NCDValNullTermString_Free (NCDValNullTermString *o);

/**
 * Returns the string ID and the string index of an ID-string.
 * The value given must be an ID-string value (which can be determined via
 * {@link NCDVal_IsIdString}). Both the \a out_string_id and \a out_string_index
 * pointers must be non-NULL.
 */
void NCDVal_IdStringGet (NCDValRef idstring, NCD_string_id_t *out_string_id,
                         NCDStringIndex **out_string_index);

/**
 * Returns the string ID of an ID-string.
 * The value given must be an ID-string value (which can be determined via
 * {@link NCDVal_IsIdString}).
 */
NCD_string_id_t NCDVal_IdStringId (NCDValRef idstring);

/**
 * Returns the string index of an ID-string.
 * The value given must be an ID-string value (which can be determined via
 * {@link NCDVal_IsIdString}).
 */
NCDStringIndex * NCDVal_IdStringStringIndex (NCDValRef idstring);

/**
 * Returns the reference target of an external string.
 * The value given must be an external string value (which can be determined
 * via {@link NCDVal_IsExternalString}).
 */
NCDRefTarget * NCDVal_ExternalStringTarget (NCDValRef externalstring);

/**
 * Determines if the string value has any null bytes in its contents.
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
 * Determines if the string value is equal to the given string represented
 * by an {@link NCDStringIndex} identifier.
 * The value reference must point to a string value.
 * NOTE: \a string_index must be equal to the string_index of every ID-string
 * that exist within this memory object.
 */
int NCDVal_StringEqualsId (NCDValRef string, NCD_string_id_t string_id,
                           NCDStringIndex *string_index);

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

/**
 * Builds a placeholder replacement program, which is a list of instructions for
 * efficiently replacing placeholders in identical values in identical memory
 * objects.
 * To actually perform replacements, make copies of the memory object of this value
 * using {@link NCDValMem_InitCopy}, then call {@link NCDValReplaceProg_Execute}
 * on the copies.
 * The value passed must be a valid value, and not a placeholder.
 * Returns 1 on success, 0 on failure.
 */
int NCDValReplaceProg_Init (NCDValReplaceProg *o, NCDValRef val);

/**
 * Frees the placeholder replacement program.
 */
void NCDValReplaceProg_Free (NCDValReplaceProg *o);

/**
 * Callback used by {@link NCDValReplaceProg_Execute} to allow the caller to produce
 * values of placeholders.
 * This function should build a new value within the memory object 'mem' (which is
 * the same as of the memory object where placeholders are being replaced).
 * On success, it should return 1, writing a valid value reference to *out.
 * On failure, it can either return 0, or return 1 but write an invalid value reference.
 * This callback must not access the memory object in any other way than building
 * new values in it; it must not modify any values that were already present at the
 * point it was called.
 */
typedef int (*NCDVal_replace_func) (void *arg, int plid, NCDValMem *mem, NCDValRef *out);

/**
 * Executes the replacement program, replacing placeholders in a value.
 * The memory object must given be identical to the memory object which was used in
 * {@link NCDValReplaceProg_Init}; see {@link NCDValMem_InitCopy}.
 * This will call the callback 'replace', which should build the values to replace
 * the placeholders.
 * Returns 1 on success and 0 on failure. On failure, the entire memory object enters
 * and inconsistent state and must be freed using {@link NCDValMem_Free} before
 * performing any other operation on it.
 * The program is passed by value instead of pointer because this appears to be faster.
 * Is is not modified in any way.
 */
int NCDValReplaceProg_Execute (NCDValReplaceProg prog, NCDValMem *mem, NCDVal_replace_func replace, void *arg);

#endif
