/**
 * @file NCDInterpBlock.h
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

#ifndef BADVPN_NCDINTERPBLOCK_H
#define BADVPN_NCDINTERPBLOCK_H

#include <stddef.h>

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <structure/CHash.h>
#include <ncd/NCDAst.h>
#include <ncd/NCDVal.h>
#include <ncd/NCDPlaceholderDb.h>

struct NCDInterpBlock__stmt {
    const char *name;
    const char *cmdname;
    char *objnames;
    size_t num_objnames;
    char *arg_data;
    size_t arg_len;
    NCDValSafeRef arg_ref;
    NCDValReplaceProg arg_prog;
    int alloc_size;
    int prealloc_offset;
    int hash_next;
};

typedef struct NCDInterpBlock__stmt NCDInterpBlock__hashentry;
typedef const char *NCDInterpBlock__hashkey;
typedef struct NCDInterpBlock__stmt *NCDInterpBlock__hasharg;

#include "NCDInterpBlock_hash.h"
#include <structure/CHash_decl.h>

typedef struct {
    struct NCDInterpBlock__stmt *stmts;
    int num_stmts;
    int prealloc_size;
    NCDInterpBlock__Hash hash;
    NCDProcess *process;
    DebugObject d_obj;
} NCDInterpBlock;

int NCDInterpBlock_Init (NCDInterpBlock *o, NCDBlock *block, NCDProcess *process, NCDPlaceholderDb *pdb) WARN_UNUSED;
void NCDInterpBlock_Free (NCDInterpBlock *o);
int NCDInterpBlock_FindStatement (NCDInterpBlock *o, int from_index, const char *name);
const char * NCDInterpBlock_StatementCmdName (NCDInterpBlock *o, int i);
void NCDInterpBlock_StatementObjNames (NCDInterpBlock *o, int i, const char **out_objnames, size_t *out_num_objnames);
int NCDInterpBlock_CopyStatementArgs (NCDInterpBlock *o, int i, NCDValMem *out_valmem, NCDValRef *out_val, NCDValReplaceProg *out_prog) WARN_UNUSED;
void NCDInterpBlock_StatementBumpAllocSize (NCDInterpBlock *o, int i, int alloc_size);
int NCDInterpBlock_StatementPreallocSize (NCDInterpBlock *o, int i);
int NCDInterpBlock_PreallocSize (NCDInterpBlock *o);
int NCDInterpBlock_StatementPreallocOffset (NCDInterpBlock *o, int i);
NCDProcess * NCDInterpBlock_Process (NCDInterpBlock *o);

#endif
