/**
 * @file NCDModuleIndex.c
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

#include <string.h>
#include <stdlib.h>

#include <misc/offset.h>
#include <misc/balloc.h>
#include <misc/hashfun.h>
#include <misc/compare.h>
#include <base/BLog.h>

#include "NCDModuleIndex.h"

#include <generated/blog_channel_NCDModuleIndex.h>

#include "NCDModuleIndex_mhash.h"
#include <structure/CHash_impl.h>

static int string_pointer_comparator (void *user, const char **s1, const char **s2)
{
    int cmp = strcmp(*s1, *s2);
    return B_COMPARE(cmp, 0);
}

static struct NCDModuleIndex_module * find_module (NCDModuleIndex *o, const char *type)
{
    NCDModuleIndex__MHashRef ref = NCDModuleIndex__MHash_Lookup(&o->modules_hash, o->modules, type);
    if (ref.link == NCDModuleIndex__MHashNullLink()) {
        return NULL;
    }
    
    ASSERT(!strcmp(ref.ptr->type, type))
    
    return ref.ptr;
}

static struct NCDModuleIndex_base_type * find_base_type (NCDModuleIndex *o, const char *base_type)
{
    BAVLNode *node = BAVL_LookupExact(&o->base_types_tree, &base_type);
    if (!node) {
        return NULL;
    }
    
    struct NCDModuleIndex_base_type *bt = UPPER_OBJECT(node, struct NCDModuleIndex_base_type, base_types_tree_node);
    ASSERT(!strcmp(bt->base_type, base_type))
    
    return bt;
}

int NCDModuleIndex_Init (NCDModuleIndex *o)
{
    // allocate modules array
    if (!(o->modules = BAllocArray(NCDMODULEINDEX_MAX_MODULES, sizeof(o->modules[0])))) {
        BLog(BLOG_ERROR, "BAllocArray failed");
        goto fail0;
    }
    
    // set zero modules
    o->num_modules = 0;
    
    // init modules hash
    if (!NCDModuleIndex__MHash_Init(&o->modules_hash, NCDMODULEINDEX_MODULES_HASH_SIZE)) {
        BLog(BLOG_ERROR, "NCDModuleIndex__MHash_Init failed");
        goto fail1;
    }
    
    // init base types tree
    BAVL_Init(&o->base_types_tree, OFFSET_DIFF(struct NCDModuleIndex_base_type, base_type, base_types_tree_node), (BAVL_comparator)string_pointer_comparator, NULL);
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail1:
    BFree(o->modules);
fail0:
    return 0;
}

void NCDModuleIndex_Free (NCDModuleIndex *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free base types
    while (!BAVL_IsEmpty(&o->base_types_tree)) {
        struct NCDModuleIndex_base_type *bt = UPPER_OBJECT(BAVL_GetFirst(&o->base_types_tree), struct NCDModuleIndex_base_type, base_types_tree_node);
        BAVL_Remove(&o->base_types_tree, &bt->base_types_tree_node);
        free(bt);
    }
    
    // free modules hash
    NCDModuleIndex__MHash_Free(&o->modules_hash);
    
    // free modules array
    BFree(o->modules);
}

int NCDModuleIndex_AddGroup (NCDModuleIndex *o, const struct NCDModuleGroup *group)
{
    DebugObject_Access(&o->d_obj);
    
    for (const struct NCDModule *nm = group->modules; nm->type; nm++) {
        if (find_module(o, nm->type)) {
            BLog(BLOG_ERROR, "module type '%s' already exists", nm->type);
            goto loop_fail0;
        }
        
        if (strlen(nm->type) > NCDMODULEINDEX_MAX_TYPE_LEN) {
            BLog(BLOG_ERROR, "module type '%s' is too long (dump NCDMODULEINDEX_MAX_TYPE_LEN)", nm->type);
            goto loop_fail0;
        }
        
        if (o->num_modules == NCDMODULEINDEX_MAX_MODULES) {
            BLog(BLOG_ERROR, "too many modules (bump NCDMODULEINDEX_MAX_MODULES)");
            goto loop_fail0;
        }
        
        struct NCDModuleIndex_module *m = &o->modules[o->num_modules];
        
        strcpy(m->type, nm->type);
        m->module = nm;
        
        NCDModuleIndex__MHashRef ref = {m, o->num_modules};
        int res = NCDModuleIndex__MHash_Insert(&o->modules_hash, o->modules, ref, NULL);
        ASSERT(res)
        
        const char *base_type = (nm->base_type ? nm->base_type : nm->type);
        
        struct NCDModuleIndex_base_type *bt = find_base_type(o, base_type);
        if (bt) {
            if (bt->group != group) {
                BLog(BLOG_ERROR, "module base type '%s' already exists in another module group", base_type);
                goto loop_fail1;
            }
        } else {
            if (!(bt = malloc(sizeof(*bt)))) {
                BLog(BLOG_ERROR, "malloc failed");
                goto loop_fail1;
            }
            
            bt->base_type = base_type;
            bt->group = group;
            ASSERT_EXECUTE(BAVL_Insert(&o->base_types_tree, &bt->base_types_tree_node, NULL))
        }
        
        o->num_modules++;
        continue;
        
    loop_fail1:
        NCDModuleIndex__MHash_Remove(&o->modules_hash, o->modules, ref);
    loop_fail0:
        return 0;
    }
    
    return 1;
}

const struct NCDModule * NCDModuleIndex_FindModule (NCDModuleIndex *o, const char *type)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(type)
    
    struct NCDModuleIndex_module *m = find_module(o, type);
    if (!m) {
        return NULL;
    }
    
    return m->module;
}
