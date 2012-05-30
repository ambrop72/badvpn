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

#include <misc/balloc.h>
#include <base/BLog.h>

#include "NCDInterpBlock.h"

#include <generated/blog_channel_ncd.h>

static size_t djb2_hash (const unsigned char *str)
{
    size_t hash = 5381;
    int c;

    while (c = *str++) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return hash;
}

#include "NCDInterpBlock_hash.h"
#include <structure/CHash_impl.h>

int NCDInterpBlock_Init (NCDInterpBlock *o, NCDBlock *block)
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
    
    for (NCDStatement *s = NCDBlock_FirstStatement(block); s; s = NCDBlock_NextStatement(block, s)) {
        struct NCDInterpBlock__stmt *e = &o->stmts[o->num_stmts];
        
        e->name = NCDStatement_Name(s);
        
        if (e->name) {
            NCDInterpBlock__HashRef ref = NCDInterpBlock__Hash_Deref(o->stmts, o->num_stmts);
            NCDInterpBlock__Hash_InsertMulti(&o->hash, o->stmts, ref);
        }
        
        o->num_stmts++;
    }
    
    ASSERT(o->num_stmts == num_stmts)
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail1:
    BFree(o->stmts);
fail0:
    return 0;
}

void NCDInterpBlock_Free (NCDInterpBlock *o)
{
    DebugObject_Free(&o->d_obj);
    
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
        ASSERT(!strcmp(ref.ptr->name, name))
        if (ref.link < from_index) {
            return ref.link;
        }
        ref = NCDInterpBlock__Hash_GetNextEqual(&o->hash, o->stmts, ref);
    }
    
    return -1;
}
