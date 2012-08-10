/**
 * @file NCDInterpProcess.c
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

#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#include <misc/balloc.h>
#include <misc/split_string.h>
#include <misc/hashfun.h>
#include <misc/maxalign.h>
#include <misc/strdup.h>
#include <base/BLog.h>

#include "NCDInterpProcess.h"

#include <generated/blog_channel_ncd.h>

#include "NCDInterpProcess_trie.h"
#include <structure/CStringTrie_impl.h>

static int compute_prealloc (NCDInterpProcess *o)
{
    int size = 0;
    
    for (int i = 0; i < o->num_stmts; i++) {
        int mod = size % BMAX_ALIGN;
        int align_size = (mod == 0 ? 0 : BMAX_ALIGN - mod);
        
        if (align_size + o->stmts[i].alloc_size > INT_MAX - size) {
            return 0;
        }
        
        o->stmts[i].prealloc_offset = size + align_size;
        size += align_size + o->stmts[i].alloc_size;
    }
    
    ASSERT(size >= 0)
    
    o->prealloc_size = size;
    
    return 1;
}

static int convert_value_recurser (NCDPlaceholderDb *pdb, NCDValue *value, NCDValMem *mem, NCDValRef *out)
{
    ASSERT(pdb)
    ASSERT((NCDValue_Type(value), 1))
    ASSERT(mem)
    ASSERT(out)
    
    switch (NCDValue_Type(value)) {
        case NCDVALUE_STRING: {
            *out = NCDVal_NewStringBin(mem, (const uint8_t *)NCDValue_StringValue(value), NCDValue_StringLength(value));
            if (NCDVal_IsInvalid(*out)) {
                goto fail;
            }
        } break;
        
        case NCDVALUE_LIST: {
            *out = NCDVal_NewList(mem, NCDValue_ListCount(value));
            if (NCDVal_IsInvalid(*out)) {
                goto fail;
            }
            
            for (NCDValue *e = NCDValue_ListFirst(value); e; e = NCDValue_ListNext(value, e)) {
                NCDValRef vval;
                if (!convert_value_recurser(pdb, e, mem, &vval)) {
                    goto fail;
                }
                
                NCDVal_ListAppend(*out, vval);
            }
        } break;
        
        case NCDVALUE_MAP: {
            *out = NCDVal_NewMap(mem, NCDValue_MapCount(value));
            if (NCDVal_IsInvalid(*out)) {
                goto fail;
            }
            
            for (NCDValue *ekey = NCDValue_MapFirstKey(value); ekey; ekey = NCDValue_MapNextKey(value, ekey)) {
                NCDValue *eval = NCDValue_MapKeyValue(value, ekey);
                
                NCDValRef vkey;
                NCDValRef vval;
                if (!convert_value_recurser(pdb, ekey, mem, &vkey) ||
                    !convert_value_recurser(pdb, eval, mem, &vval)
                ) {
                    goto fail;
                }
                
                int res = NCDVal_MapInsert(*out, vkey, vval);
                ASSERT(res) // we assume different variables get different placeholder ids
            }
        } break;
        
        case NCDVALUE_VAR: {
            int plid;
            if (!NCDPlaceholderDb_AddVariable(pdb, NCDValue_VarName(value), &plid)) {
                goto fail;
            }
            
            if (NCDVAL_MINIDX + plid >= -1) {
                goto fail;
            }
            
            *out = NCDVal_NewPlaceholder(mem, plid);
        } break;
        
        default:
            goto fail;
    }
    
    return 1;
    
fail:
    return 0;
}

int NCDInterpProcess_Init (NCDInterpProcess *o, NCDBlock *block, NCDProcess *process, NCDPlaceholderDb *pdb, NCDModuleIndex *module_index, NCDMethodIndex *method_index)
{
    ASSERT(block)
    ASSERT(process)
    ASSERT(pdb)
    ASSERT(module_index)
    ASSERT(method_index)
    
    if (NCDBlock_NumStatements(block) > INT_MAX) {
        BLog(BLOG_ERROR, "too many statements");
        goto fail0;
    }
    int num_stmts = NCDBlock_NumStatements(block);
    
    if (!(o->stmts = BAllocArray(num_stmts, sizeof(o->stmts[0])))) {
        BLog(BLOG_ERROR, "BAllocArray failed");
        goto fail0;
    }
    
    if (!NCDInterpProcess__Trie_Init(&o->trie)) {
        BLog(BLOG_ERROR, "BStringTrie_Init failed");
        goto fail1;
    }
    
    o->num_stmts = 0;
    o->prealloc_size = -1;
    o->process = process;
    
    for (NCDStatement *s = NCDBlock_FirstStatement(block); s; s = NCDBlock_NextStatement(block, s)) {
        ASSERT(NCDStatement_Type(s) == NCDSTATEMENT_REG)
        struct NCDInterpProcess__stmt *e = &o->stmts[o->num_stmts];
        
        e->name = NCDStatement_Name(s);
        e->cmdname = NCDStatement_RegCmdName(s);
        e->objnames = NULL;
        e->num_objnames = 0;
        e->alloc_size = 0;
        
        NCDValMem mem;
        NCDValMem_Init(&mem);
        
        NCDValRef val;
        if (!convert_value_recurser(pdb, NCDStatement_RegArgs(s), &mem, &val)) {
            BLog(BLOG_ERROR, "convert_value_recurser failed");
            NCDValMem_Free(&mem);
            goto loop_fail0;
        }
        
        e->arg_ref = NCDVal_ToSafe(val);
        
        if (!NCDValReplaceProg_Init(&e->arg_prog, val)) {
            BLog(BLOG_ERROR, "NCDValReplaceProg_Init failed");
            NCDValMem_Free(&mem);
            goto loop_fail0;
        }
        
        if (!NCDValMem_FreeExport(&mem, &e->arg_data, &e->arg_len)) {
            BLog(BLOG_ERROR, "NCDValMem_FreeExport failed");
            NCDValMem_Free(&mem);
            goto loop_fail1;
        }
        
        if (NCDStatement_RegObjName(s)) {
            if (!(e->objnames = b_strdup(NCDStatement_RegObjName(s)))) {
                BLog(BLOG_ERROR, "b_strdup failed");
                goto loop_fail2;
            }
            e->num_objnames = split_string_inplace2(e->objnames, '.') + 1;
            
            e->binding.method_name_id = NCDMethodIndex_GetMethodNameId(method_index, NCDStatement_RegCmdName(s));
            if (e->binding.method_name_id == -1) {
                BLog(BLOG_ERROR, "NCDMethodIndex_GetMethodNameId failed");
                goto loop_fail3;
            }
        } else {
            e->binding.simple_module = NCDModuleIndex_FindModule(module_index, NCDStatement_RegCmdName(s));
        }
        
        if (e->name) {
            int next_idx = NCDInterpProcess__Trie_Get(&o->trie, e->name);
            ASSERT(next_idx >= -1)
            ASSERT(next_idx < o->num_stmts)
            
            e->trie_next = next_idx;
            
            if (!NCDInterpProcess__Trie_Set(&o->trie, e->name, o->num_stmts)) {
                BLog(BLOG_ERROR, "NCDInterpProcess__Trie_Set failed");
                goto loop_fail3;
            }
        }
        
        o->num_stmts++;
        continue;
        
    loop_fail3:
        free(e->objnames);
    loop_fail2:
        BFree(e->arg_data);
    loop_fail1:
        NCDValReplaceProg_Free(&e->arg_prog);
    loop_fail0:
        goto fail2;
    }
    
    ASSERT(o->num_stmts == num_stmts)
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail2:
    while (o->num_stmts-- > 0) {
        struct NCDInterpProcess__stmt *e = &o->stmts[o->num_stmts];
        free(e->objnames);
        BFree(e->arg_data);
        NCDValReplaceProg_Free(&e->arg_prog);
    }
    NCDInterpProcess__Trie_Free(&o->trie);
fail1:
    BFree(o->stmts);
fail0:
    return 0;
}

void NCDInterpProcess_Free (NCDInterpProcess *o)
{
    DebugObject_Free(&o->d_obj);
    
    while (o->num_stmts-- > 0) {
        struct NCDInterpProcess__stmt *e = &o->stmts[o->num_stmts];
        free(e->objnames);
        BFree(e->arg_data);
        NCDValReplaceProg_Free(&e->arg_prog);
    }
    
    NCDInterpProcess__Trie_Free(&o->trie);
    
    BFree(o->stmts);
}

int NCDInterpProcess_FindStatement (NCDInterpProcess *o, int from_index, const char *name)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(from_index >= 0)
    ASSERT(from_index <= o->num_stmts)
    ASSERT(name)
    
    int stmt_idx = NCDInterpProcess__Trie_Get(&o->trie, name);
    ASSERT(stmt_idx >= -1)
    ASSERT(stmt_idx < o->num_stmts)
    
    while (stmt_idx >= 0) {
        struct NCDInterpProcess__stmt *e = &o->stmts[stmt_idx];
        
        if (!strcmp(e->name, name) && stmt_idx < from_index) {
            return stmt_idx;
        }
        
        stmt_idx = e->trie_next;
        ASSERT(stmt_idx >= -1)
        ASSERT(stmt_idx < o->num_stmts)
    }
    
    return -1;
}

const char * NCDInterpProcess_StatementCmdName (NCDInterpProcess *o, int i)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    
    return o->stmts[i].cmdname;
}

void NCDInterpProcess_StatementObjNames (NCDInterpProcess *o, int i, const char **out_objnames, size_t *out_num_objnames)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(out_objnames)
    ASSERT(out_num_objnames)
    
    *out_objnames = o->stmts[i].objnames;
    *out_num_objnames = o->stmts[i].num_objnames;
}

const struct NCDModule * NCDInterpProcess_StatementGetSimpleModule (NCDInterpProcess *o, int i)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(!o->stmts[i].objnames)
    
    return o->stmts[i].binding.simple_module;
}

const struct NCDModule * NCDInterpProcess_StatementGetMethodModule (NCDInterpProcess *o, int i, const char *obj_type, NCDMethodIndex *method_index)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(o->stmts[i].objnames)
    ASSERT(obj_type)
    ASSERT(method_index)
    
    return NCDMethodIndex_GetMethodModule(method_index, obj_type, o->stmts[i].binding.method_name_id);
}

int NCDInterpProcess_CopyStatementArgs (NCDInterpProcess *o, int i, NCDValMem *out_valmem, NCDValRef *out_val, NCDValReplaceProg *out_prog)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(out_valmem)
    ASSERT(out_val)
    ASSERT(out_prog)
    
    struct NCDInterpProcess__stmt *e = &o->stmts[i];
    
    if (!NCDValMem_InitImport(out_valmem, e->arg_data, e->arg_len)) {
        return 0;
    }
    
    *out_val = NCDVal_FromSafe(out_valmem, e->arg_ref);
    *out_prog = e->arg_prog;
    return 1;
}

void NCDInterpProcess_StatementBumpAllocSize (NCDInterpProcess *o, int i, int alloc_size)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(alloc_size >= 0)
    
    if (alloc_size > o->stmts[i].alloc_size) {
        o->stmts[i].alloc_size = alloc_size;
        o->prealloc_size = -1;
    }
}

int NCDInterpProcess_StatementPreallocSize (NCDInterpProcess *o, int i)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    
    return o->stmts[i].alloc_size;
}

int NCDInterpProcess_PreallocSize (NCDInterpProcess *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->prealloc_size == -1 || o->prealloc_size >= 0)
    
    if (o->prealloc_size < 0 && !compute_prealloc(o)) {
        return -1;
    }
    
    return o->prealloc_size;
}

int NCDInterpProcess_StatementPreallocOffset (NCDInterpProcess *o, int i)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(o->prealloc_size >= 0)
    
    return o->stmts[i].prealloc_offset;
}

NCDProcess * NCDInterpProcess_Process (NCDInterpProcess *o)
{
    DebugObject_Access(&o->d_obj);
    
    return o->process;
}
