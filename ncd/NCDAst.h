/**
 * @file NCDAst.h
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

#ifndef BADVPN_NCDAST_H
#define BADVPN_NCDAST_H

#include <stdint.h>
#include <stddef.h>

#include <misc/debug.h>
#include <structure/LinkedList1.h>
#include <structure/SAvl.h>

typedef struct NCDValue_s NCDValue;
typedef struct NCDProgram_s NCDProgram;
typedef struct NCDProcess_s NCDProcess;
typedef struct NCDBlock_s NCDBlock;
typedef struct NCDStatement_s NCDStatement;
typedef struct NCDIfBlock_s NCDIfBlock;
typedef struct NCDIf_s NCDIf;

struct NCDValue__map_element;
typedef NCDValue *NCDValue__maptree_key;

#include "NCDAst_maptree.h"
#include <structure/SAvl_decl.h>

struct NCDValue_s {
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
};

struct NCDProgram_s {
    LinkedList1 processes_list;
    size_t num_processes;
};

struct NCDBlock_s {
    LinkedList1 statements_list;
    size_t count;
};

struct NCDProcess_s {
    int is_template;
    char *name;
    NCDBlock block;
};

struct NCDIfBlock_s {
    LinkedList1 ifs_list;
};

struct NCDStatement_s {
    int type;
    char *name;
    union {
        struct {
            char *objname;
            char *cmdname;
            NCDValue args;
        } reg;
        struct {
            NCDIfBlock ifblock;
            int have_else;
            NCDBlock else_block;
        } ifc;
        struct {
            NCDValue collection;
            char *name1;
            char *name2;
            NCDBlock block;
            int is_grabbed;
        } foreach;
    };
};

struct NCDIf_s {
    NCDValue cond;
    NCDBlock block;
};

struct ProgramProcess {
    LinkedList1Node processes_list_node;
    NCDProcess p;
};

struct BlockStatement {
    LinkedList1Node statements_list_node;
    NCDStatement s;
};

struct IfBlockIf {
    LinkedList1Node ifs_list_node;
    NCDIf ifc;
};

//

#define NCDVALUE_STRING 1
#define NCDVALUE_LIST 2
#define NCDVALUE_MAP 3
#define NCDVALUE_VAR 4

#define NCDSTATEMENT_REG 1
#define NCDSTATEMENT_IF 2
#define NCDSTATEMENT_FOREACH 3

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

void NCDProgram_Init (NCDProgram *o);
void NCDProgram_Free (NCDProgram *o);
NCDProcess * NCDProgram_PrependProcess (NCDProgram *o, NCDProcess p) WARN_UNUSED;
NCDProcess * NCDProgram_FirstProcess (NCDProgram *o);
NCDProcess * NCDProgram_NextProcess (NCDProgram *o, NCDProcess *ep);
size_t NCDProgram_NumProcesses (NCDProgram *o);

int NCDProcess_Init (NCDProcess *o, int is_template, const char *name, NCDBlock block) WARN_UNUSED;
void NCDProcess_Free (NCDProcess *o);
int NCDProcess_IsTemplate (NCDProcess *o);
const char * NCDProcess_Name (NCDProcess *o);
NCDBlock * NCDProcess_Block (NCDProcess *o);

void NCDBlock_Init (NCDBlock *o);
void NCDBlock_Free (NCDBlock *o);
int NCDBlock_PrependStatement (NCDBlock *o, NCDStatement s) WARN_UNUSED;
int NCDBlock_InsertStatementAfter (NCDBlock *o, NCDStatement *after, NCDStatement s) WARN_UNUSED;
NCDStatement * NCDBlock_ReplaceStatement (NCDBlock *o, NCDStatement *es, NCDStatement s);
NCDStatement * NCDBlock_FirstStatement (NCDBlock *o);
NCDStatement * NCDBlock_NextStatement (NCDBlock *o, NCDStatement *es);
size_t NCDBlock_NumStatements (NCDBlock *o);

int NCDStatement_InitReg (NCDStatement *o, const char *name, const char *objname, const char *cmdname, NCDValue args) WARN_UNUSED;
int NCDStatement_InitIf (NCDStatement *o, const char *name, NCDIfBlock ifblock) WARN_UNUSED;
int NCDStatement_InitForeach (NCDStatement *o, const char *name, NCDValue collection, const char *name1, const char *name2, NCDBlock block) WARN_UNUSED;
void NCDStatement_Free (NCDStatement *o);
int NCDStatement_Type (NCDStatement *o);
const char * NCDStatement_Name (NCDStatement *o);
const char * NCDStatement_RegObjName (NCDStatement *o);
const char * NCDStatement_RegCmdName (NCDStatement *o);
NCDValue * NCDStatement_RegArgs (NCDStatement *o);
NCDIfBlock * NCDStatement_IfBlock (NCDStatement *o);
void NCDStatement_IfAddElse (NCDStatement *o, NCDBlock else_block);
NCDBlock * NCDStatement_IfElse (NCDStatement *o);
NCDBlock NCDStatement_IfGrabElse (NCDStatement *o);
NCDValue * NCDStatement_ForeachCollection (NCDStatement *o);
const char * NCDStatement_ForeachName1 (NCDStatement *o);
const char * NCDStatement_ForeachName2 (NCDStatement *o);
NCDBlock * NCDStatement_ForeachBlock (NCDStatement *o);
void NCDStatement_ForeachGrab (NCDStatement *o, NCDValue *out_collection, NCDBlock *out_block);

void NCDIfBlock_Init (NCDIfBlock *o);
void NCDIfBlock_Free (NCDIfBlock *o);
int NCDIfBlock_PrependIf (NCDIfBlock *o, NCDIf ifc) WARN_UNUSED;
NCDIf * NCDIfBlock_FirstIf (NCDIfBlock *o);
NCDIf * NCDIfBlock_NextIf (NCDIfBlock *o, NCDIf *ei);
NCDIf NCDIfBlock_GrabIf (NCDIfBlock *o, NCDIf *ei);

void NCDIf_Init (NCDIf *o, NCDValue cond, NCDBlock block);
void NCDIf_Free (NCDIf *o);
void NCDIf_FreeGrab (NCDIf *o, NCDValue *out_cond, NCDBlock *out_block);
NCDValue * NCDIf_Cond (NCDIf *o);
NCDBlock * NCDIf_Block (NCDIf *o);

#endif
