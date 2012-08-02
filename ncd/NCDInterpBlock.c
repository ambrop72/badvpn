/**
 * @file NCDInterpBlock.c
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
#include <base/BLog.h>

#include "NCDInterpBlock.h"

#include <generated/blog_channel_ncd.h>

#include "NCDInterpBlock_hash.h"
#include <structure/CHash_impl.h>

static int compute_prealloc (NCDInterpBlock *o)
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

int NCDInterpBlock_Init (NCDInterpBlock *o, NCDBlock *block, NCDProcess *process)
{
    if (NCDBlock_NumStatements(block) > INT_MAX) {
        BLog(BLOG_ERROR, "too many statements");
        goto fail0;
    }
    int num_stmts = NCDBlock_NumStatements(block);
    
    if (!(o->stmts = BAllocArray(num_stmts, sizeof(o->stmts[0])))) {
        BLog(BLOG_ERROR, "BAllocArray failed");
        goto fail0;
    }
    
    if (!NCDInterpBlock__Hash_Init(&o->hash, num_stmts)) {
        BLog(BLOG_ERROR, "NCDInterpBlock__Hash_Init failed");
        goto fail1;
    }
    
    o->num_stmts = 0;
    o->prealloc_size = -1;
    
    for (NCDStatement *s = NCDBlock_FirstStatement(block); s; s = NCDBlock_NextStatement(block, s)) {
        ASSERT(NCDStatement_Type(s) == NCDSTATEMENT_REG)
        struct NCDInterpBlock__stmt *e = &o->stmts[o->num_stmts];
        
        e->name = NCDStatement_Name(s);
        e->cmdname = NCDStatement_RegCmdName(s);
        e->objnames = NULL;
        e->alloc_size = 0;
        
        if (!NCDInterpValue_Init(&e->ivalue, NCDStatement_RegArgs(s))) {
            BLog(BLOG_ERROR, "NCDInterpValue_Init failed");
            goto loop_fail0;
        }
        
        if (NCDStatement_RegObjName(s) && !(e->objnames = split_string(NCDStatement_RegObjName(s), '.'))) {
            BLog(BLOG_ERROR, "split_string failed");
            goto loop_fail1;
        }
        
        if (e->name) {
            NCDInterpBlock__HashRef ref = NCDInterpBlock__Hash_Deref(o->stmts, o->num_stmts);
            NCDInterpBlock__Hash_InsertMulti(&o->hash, o->stmts, ref);
        }
        
        o->num_stmts++;
        continue;
        
    loop_fail1:
        NCDInterpValue_Free(&e->ivalue);
    loop_fail0:
        goto fail2;
    }
    
    ASSERT(o->num_stmts == num_stmts)
    
    o->process = process;
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail2:
    while (o->num_stmts-- > 0) {
        struct NCDInterpBlock__stmt *e = &o->stmts[o->num_stmts];
        if (e->objnames) {
            free_strings(e->objnames);
        }
        NCDInterpValue_Free(&e->ivalue);
    }
fail1:
    BFree(o->stmts);
fail0:
    return 0;
}

void NCDInterpBlock_Free (NCDInterpBlock *o)
{
    DebugObject_Free(&o->d_obj);
    
    while (o->num_stmts-- > 0) {
        struct NCDInterpBlock__stmt *e = &o->stmts[o->num_stmts];
        if (e->objnames) {
            free_strings(e->objnames);
        }
        NCDInterpValue_Free(&e->ivalue);
    }
    
    NCDInterpBlock__Hash_Free(&o->hash);
    
    BFree(o->stmts);
}

int NCDInterpBlock_FindStatement (NCDInterpBlock *o, int from_index, const char *name)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(from_index >= 0)
    ASSERT(from_index <= o->num_stmts)
    ASSERT(name)
    
    // We rely on that we get matching statements here in reverse order of insertion,
    // to properly return the greatest matching statement lesser than from_index.
    
    NCDInterpBlock__HashRef ref = NCDInterpBlock__Hash_Lookup(&o->hash, o->stmts, name);
    while (ref.link != NCDInterpBlock__HashNullLink()) {
        ASSERT(ref.link >= 0)
        ASSERT(ref.link < o->num_stmts)
        ASSERT(ref.ptr == &o->stmts[ref.link])
        ASSERT(!strcmp(ref.ptr->name, name))
        
        if (ref.link < from_index) {
            return ref.link;
        }
        
        ref = NCDInterpBlock__Hash_GetNextEqual(&o->hash, o->stmts, ref);
    }
    
    return -1;
}

const char * NCDInterpBlock_StatementCmdName (NCDInterpBlock *o, int i)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    
    return o->stmts[i].cmdname;
}

char ** NCDInterpBlock_StatementObjNames (NCDInterpBlock *o, int i)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    
    return o->stmts[i].objnames;
}

NCDInterpValue * NCDInterpBlock_StatementInterpValue (NCDInterpBlock *o, int i)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    
    return &o->stmts[i].ivalue;
}

void NCDInterpBlock_StatementBumpAllocSize (NCDInterpBlock *o, int i, int alloc_size)
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

int NCDInterpBlock_StatementPreallocSize (NCDInterpBlock *o, int i)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    
    return o->stmts[i].alloc_size;
}

int NCDInterpBlock_PreallocSize (NCDInterpBlock *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->prealloc_size == -1 || o->prealloc_size >= 0)
    
    if (o->prealloc_size < 0 && !compute_prealloc(o)) {
        return -1;
    }
    
    return o->prealloc_size;
}

int NCDInterpBlock_StatementPreallocOffset (NCDInterpBlock *o, int i)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(o->prealloc_size >= 0)
    
    return o->stmts[i].prealloc_offset;
}

NCDProcess * NCDInterpBlock_Process (NCDInterpBlock *o)
{
    DebugObject_Access(&o->d_obj);
    
    return o->process;
}
