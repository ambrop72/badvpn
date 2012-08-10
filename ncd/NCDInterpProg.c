/**
 * @file NCDInterpProg.c
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
#include <misc/hashfun.h>
#include <base/BLog.h>

#include "NCDInterpProg.h"

#include <generated/blog_channel_ncd.h>

#include "NCDInterpProg_hash.h"
#include <structure/CHash_impl.h>

int NCDInterpProg_Init (NCDInterpProg *o, NCDProgram *prog, NCDPlaceholderDb *pdb, NCDModuleIndex *module_index, NCDMethodIndex *method_index)
{
    ASSERT(prog)
    ASSERT(pdb)
    ASSERT(module_index)
    ASSERT(method_index)
    
    if (NCDProgram_NumProcesses(prog) > INT_MAX) {
        BLog(BLOG_ERROR, "too many processes");
        goto fail0;
    }
    int num_procs = NCDProgram_NumProcesses(prog);
    
    if (!(o->procs = BAllocArray(num_procs, sizeof(o->procs[0])))) {
        BLog(BLOG_ERROR, "BAllocArray failed");
        goto fail0;
    }
    
    if (!NCDInterpProg__Hash_Init(&o->hash, num_procs)) {
        BLog(BLOG_ERROR, "NCDInterpProg__Hash_Init failed");
        goto fail1;
    }
    
    o->num_procs = 0;
    
    for (NCDProcess *p = NCDProgram_FirstProcess(prog); p; p = NCDProgram_NextProcess(prog, p)) {
        struct NCDInterpProg__process *e = &o->procs[o->num_procs];
        
        e->name = NCDProcess_Name(p);
        e->proc = p;
        
        if (!NCDInterpProcess_Init(&e->iprocess, p, pdb, module_index, method_index)) {
            BLog(BLOG_ERROR, "NCDInterpProcess_Init failed");
            goto fail2;
        }
        
        NCDInterpProg__HashRef ref = {e, o->num_procs};
        if (!NCDInterpProg__Hash_Insert(&o->hash, o->procs, ref, NULL)) {
            BLog(BLOG_ERROR, "duplicate process or template name: %s", e->name);
            NCDInterpProcess_Free(&e->iprocess);
            goto fail2;
        }
        
        o->num_procs++;
    }
    
    ASSERT(o->num_procs == num_procs)
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail2:
    while (o->num_procs-- > 0) {
        NCDInterpProcess_Free(&o->procs[o->num_procs].iprocess);
    }
    NCDInterpProg__Hash_Free(&o->hash);
fail1:
    BFree(o->procs);
fail0:
    return 0;
}

void NCDInterpProg_Free (NCDInterpProg *o)
{
    DebugObject_Free(&o->d_obj);
    
    while (o->num_procs-- > 0) {
        NCDInterpProcess_Free(&o->procs[o->num_procs].iprocess);
    }
    
    NCDInterpProg__Hash_Free(&o->hash);
    
    BFree(o->procs);
}

NCDInterpProcess * NCDInterpProg_FindProcess (NCDInterpProg *o, const char *name)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(name)
    
    NCDInterpProg__HashRef ref = NCDInterpProg__Hash_Lookup(&o->hash, o->procs, name);
    if (ref.link == NCDInterpProg__HashNullLink()) {
        return NULL;
    }
    
    ASSERT(!strcmp(ref.ptr->name, name))
    
    return &ref.ptr->iprocess;
}
