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
#include <structure/LinkedList1.h>
#include <structure/SAvl.h>

#define NCDVALUE_STRING 1
#define NCDVALUE_LIST 2
#define NCDVALUE_MAP 3
#define NCDVALUE_VAR 4

struct NCDValue_s;
struct NCDMapElement_s;

typedef struct NCDValue_s *NCDValue__maptree_key;

#include "NCDValue_maptree.h"
#include <structure/SAvl_decl.h>

/**
 * Holds an NCD "value", which is used in the NCD programming when passing arguments to
 * statements, among other uses.
 * 
 * Each value is of one of the following three types:
 * - String (NCDVALUE_STRING); holds an array of arbitrary bytes, of any size.
 * - List (NCDVALUE_LIST); holds an ordered set of any number of values (by recursive
 *   definition).
 * - Map (NCDVALUE_MAP); holds a set of (key, value) pairs, where both 'key' and 'value'
 *   are values (by recursive definition), and 'key' is unique.
 * 
 * A valid NCDValue structure may be copied freely, which results in multiple valid NCDValue
 * structures holding the same value. When one of those is freed (or passed to a function
 * which proceeds to take ownership of the value), all the structures become invalid.
 * Similarly, if the value is modified via one structure, the others become invalid.
 */
typedef struct NCDValue_s {
    int type;
    union {
        struct {
            uint8_t *string;
            size_t string_len;
        };
        struct {
            LinkedList1 list;
            size_t list_count;
        };
        struct {
            NCDValue__MapTree map_tree;
            size_t map_count;
        };
        struct {
            char *var_name;
        };
    };
} NCDValue;

typedef struct {
    LinkedList1Node list_node;
    NCDValue v;
} NCDListElement;

typedef struct NCDMapElement_s {
    NCDValue__MapTreeNode tree_node;
    NCDValue key;
    NCDValue val;
} NCDMapElement;

/**
 * Initializes a value by copying an existing value.
 * 
 * @param o value structure to initialize
 * @param v an existing value to copy
 * @return 1 on success, 0 on failure
 */
int NCDValue_InitCopy (NCDValue *o, NCDValue *v) WARN_UNUSED;

/**
 * Frees a value.
 * 
 * @param o value to free
 */
void NCDValue_Free (NCDValue *o);

/**
 * Returns the type of a value.
 * 
 * @param o the value
 * @return type of value; one of NCDVALUE_STRING, NCDVALUE_LIST and NCDVALUE_MAP.
 */
int NCDValue_Type (NCDValue *o);

/**
 * Checks if the value is a string value.
 * 
 * @param o the value
 * @return 1 if string, 0 if not
 */
int NCDValue_IsString (NCDValue *o);

/**
 * Checks if the value is a string value and does not contain
 * any null bytes.
 * 
 * @param o the value
 * @return 1 if string with no nulls, 0 if not
 */
int NCDValue_IsStringNoNulls (NCDValue *o);

/**
 * Initializes a string value from a null-terminated string.
 * This function can only be used to create string values which do
 * not contain any null bytes. To create a string which may contain
 * null bytes, use {@link NCDValue_InitStringBin}.
 * 
 * @param o value structure to initialize
 * @param str null-terminated string
 * @return 1 on success, 0 on failure
 */
int NCDValue_InitString (NCDValue *o, const char *str) WARN_UNUSED;

/**
 * Initializes a string value from a byte array.
 * 
 * @param o value structure to initialize
 * @param str byte array
 * @param len number of bytes in byte array
 * @return 1 on success, 0 on failure
 */
int NCDValue_InitStringBin (NCDValue *o, const uint8_t *str, size_t len) WARN_UNUSED;

/**
 * Returns the pointer to the bytes of a string value. The string is always
 * null-terminated (but it itself contain null bytes).
 * 
 * @param o string value
 * @return pointer to null-terminated array of bytes
 */
char * NCDValue_StringValue (NCDValue *o);

/**
 * Returns the length of the string (excuding the internal null termination,
 * but including any null bytes in the data).
 * 
 * @param o string value
 * @return length of string
 */
size_t NCDValue_StringLength (NCDValue *o);

/**
 * Checks whether a string contains no null bytes in its data, i.e. strlen(str)==length.
 * 
 * @param o string value
 * @return 1 if no null, 0 if nulls
 */
int NCDValue_StringHasNoNulls (NCDValue *o);

/**
 * Checks whether a string contains any null bytes in its data, i.e. strlen(str) < length.
 * 
 * @param o string value
 * @return 1 if nulls, 0 if no nulls
 */
int NCDValue_StringHasNulls (NCDValue *o);

/**
 * Checks whether the string value is equal to the given null-terminated string.
 * Note that this is not equivalent to strcmp()==0, because the string value may
 * 
 * @param o string value
 * @param str null-terminated string to compare against
 * @return 1 if equal, 0 if not
 */
int NCDValue_StringEquals (NCDValue *o, const char *str);

/**
 * Checks if the value is a list value.
 * 
 * @param o the value
 * @return 1 if list, 0 if not
 */
int NCDValue_IsList (NCDValue *o);

/**
 * Initializes an empty list value.
 * 
 * @param o value structure to initialize
 */
void NCDValue_InitList (NCDValue *o);

/**
 * Appends a value to the end of a list.
 * On success, the value that was passed for insertion must be assumed freed;
 * on failure, it is unaffected.
 * 
 * @param o list value
 * @param v value to append
 * @return 1 on success, 0 on failure
 */
int NCDValue_ListAppend (NCDValue *o, NCDValue v) WARN_UNUSED;

/**
 * Prepends a value to the beginning of a list.
 * On success, the value that was passed for insertion must be assumed freed;
 * on failure, it is unaffected.
 * 
 * @param o list value
 * @param v value to prepend
 * @return 1 on success, 0 on failure
 */
int NCDValue_ListPrepend (NCDValue *o, NCDValue v) WARN_UNUSED;

/**
 * Appends values from a list to the end of a list.
 * On success, the list value that was passed with elements for insertion must be
 * assumed freed; on failure, it is unaffected.
 * 
 * @param o list value
 * @param l list value whose elements to append
 * @return 1 on success, 0 on failure
 */
int NCDValue_ListAppendList (NCDValue *o, NCDValue l) WARN_UNUSED;

/**
 * Returns the number of elements in a list.
 * 
 * @param o list value
 * @return number of elements
 */
size_t NCDValue_ListCount (NCDValue *o);

/**
 * Returns a pointer to the first elements in a list, or NULL if there are no
 * elements.
 * 
 * @param o list value
 * @return pointer to first value, or NULL
 */
NCDValue * NCDValue_ListFirst (NCDValue *o);

/**
 * Given a pointer to an existing element in a list, returns a pointer to the
 * element that follows it, or NULL if it is the last.
 * Note that the element pointer must point to a value that is really in the list
 * right now, and not just equal.
 * 
 * @param o list value
 * @param ev pointer to an existing element in the list
 * @return pointer to next value, or NULL
 */
NCDValue * NCDValue_ListNext (NCDValue *o, NCDValue *ev);

/**
 * Attempts to retrieve pointers to elements from a list.
 * Pass exactly 'num' extra NCDValue ** arguments. If the list has exactly
 * 'num' elements, this function succeeds, and returns pointers to them via the
 * passed variable arguments; if not, it fails.
 * 
 * @param o list value
 * @param num number of values to read. Must be >=0, and exactly that many
 *            variable arguments of type NCDValue ** must follow, all non-NULL.
 * @return 1 on succees, 0 on failure
 */
int NCDValue_ListRead (NCDValue *o, int num, ...) WARN_UNUSED;

/**
 * Like {@link NCDValue_ListRead}, but the list only needs to have >= 'num' values,
 * instead of exactly 'num'.
 */
int NCDValue_ListReadHead (NCDValue *o, int num, ...) WARN_UNUSED;

/**
 * Returns a pointer to the element of the list at the given position.
 * This performs a linear search from the beginning.
 * 
 * @param o list value
 * @param pos index of element to retrieve; must be < length.
 */
NCDValue * NCDValue_ListGet (NCDValue *o, size_t pos);

/**
 * Removes the first element from a list and returns it.
 * The caller takes ownership of the removed value and is responsible for freeing
 * it.
 * The list must have at least one element.
 * 
 * @param o list value
 * @return value that was the first on the list
 */
NCDValue NCDValue_ListShift (NCDValue *o);

/**
 * Removes an element from a list and returns it.
 * The caller takes ownership of the removed value and is responsible for freeing
 * it; the passed element pointer becomes invalid.
 * Note that the element pointer must point to a value that is really in the list
 * right now, and not just equal.
 * 
 * @param o list value
 * @param ev pointer to element of list to remove
 * @return value that was just removed
 */
NCDValue NCDValue_ListRemove (NCDValue *o, NCDValue *ev);

/**
 * Checks if the value is a map value.
 * 
 * @param o the value
 * @return 1 if map, 0 if not
 */
int NCDValue_IsMap (NCDValue *o);

/**
 * Initializes an empty map value.
 * 
 * @param o value structure to initialize
 */
void NCDValue_InitMap (NCDValue *o);

/**
 * Returns the number of entries in a map.
 * 
 * @param o map value
 * @return number of entries
 */
size_t NCDValue_MapCount (NCDValue *o);

/**
 * Returns the pointer to the first key in the map, or NULL if
 * the map is empty.
 * The keys are ordered according to {@link NCDValue_Compare}.
 * 
 * @param o map value
 * @return pointer to first key, or NULL
 */
NCDValue * NCDValue_MapFirstKey (NCDValue *o);

/**
 * Given a pointer to an existing key in a map, returns a pointer to the
 * key that follows it, or NULL if this is the last key.
 * Note that the key pointer must point to a value that is really a key in the map
 * right now, and not just equal to some key.
 * 
 * @param o map value
 * @param ekey pointer to an existing key in the map
 * @return pointer to next key, or NULL
 */
NCDValue * NCDValue_MapNextKey (NCDValue *o, NCDValue *ekey);

/**
 * Given a pointer to an existing key in a map, returns a pointer to the
 * value associated with it.
 * Note that the key pointer must point to a value that is really a key in the
 * map right now, and not just equal.
 * 
 * @param o map value
 * @param ekey pointer to an existing key in the map
 * @return pointer to the associated value
 */
NCDValue * NCDValue_MapKeyValue (NCDValue *o, NCDValue *ekey);

/**
 * Looks for a key in a map that is equal to the given key.
 * 
 * @param o map value
 * @param key key to look for
 * @return pointer to the key in the map, or NULL if not found
 */
NCDValue * NCDValue_MapFindKey (NCDValue *o, NCDValue *key);

/**
 * Inserts a (key, value) entry into the map.
 * The map must not already contain a key equal to the provided key.
 * On success, the key and value that were passed for insertion must be assumed freed;
 * on failure, they are unaffected.
 * 
 * @param o map value
 * @param key key to insert
 * @param val value to insert
 * @return pointer to the newly inserted key in the map, or NULL if insertion failed.
 */
NCDValue * NCDValue_MapInsert (NCDValue *o, NCDValue key, NCDValue val) WARN_UNUSED;

/**
 * Removes an entry from the map and returns the key and value that were just removed.
 * The entry to remove is specified by a pointer to an existing key in the map.
 * The caller takes ownership of the removed key and value value and is responsible for
 * freeing them; the passed key pointer becomes invalid.
 * Note that the key pointer must point to a value that is really a key in the map
 * right now, and not just equal to some key.
 * 
 * @param o map value
 * @param ekey pointer to an existing key in the map whose entry to remove
 * @param out_key the key of the removed entry will be returned here; must not be NULL.
 * @param out_val the value of the removed entry will be returned here; must not be NULL.
 */
void NCDValue_MapRemove (NCDValue *o, NCDValue *ekey, NCDValue *out_key, NCDValue *out_val);

/**
 * Looks for an entry in the map with a string key equal to the given null-terminated
 * string.
 * If such key exists, it returns a pointer to its associated value; if not, it returns
 * NULL.
 * NOTE: this returns a pointer to the value, not the key, unlike
 *       {@link NCDValue_MapFindKey}.
 * 
 * @param o map value
 * @param key_str null-terminated string specifying the key to look for
 * @return pointer to value, or NULL if there is no such key
 */
NCDValue * NCDValue_MapFindValueByString (NCDValue *o, const char *key_str);

/**
 * Checks if the value is a variable value.
 * 
 * @param o the value
 * @return 1 if variable, 0 if not
 */
int NCDValue_IsVar (NCDValue *o);

/**
 * Initializes a variable value.
 * WARNING: variable values are only used internally by NCD as part of
 * the AST, and must never be used as statement or template arguments
 * during program execution.
 * 
 * @param o value structure to initialize
 * @param var_name name of the variable
 * @return 1 on success, 0 on failure
 */
int NCDValue_InitVar (NCDValue *o, const char *var_name) WARN_UNUSED;

/**
 * Returns the name of the variable.
 * 
 * @param o variable value
 * @return variable name
 */
const char * NCDValue_VarName (NCDValue *o);

/**
 * Compares a value with another value.
 * This function defines a total order on the set of all possible values.
 * 
 * @param o first value
 * @param v second value
 * @return -1 if 'o' is lesser than 'v', 0 if equal, 1 if greater
 */
int NCDValue_Compare (NCDValue *o, NCDValue *v);

#endif
