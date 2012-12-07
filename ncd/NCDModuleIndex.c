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
#include <misc/substring.h>
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

static int add_method (char *type, const struct NCDModule *module, NCDMethodIndex *method_index, int *out_method_id)
{
    ASSERT(type)
    ASSERT(module)
    ASSERT(method_index)
    ASSERT(out_method_id)
    
    const char search[] = "::";
    size_t search_len = sizeof(search) - 1;
    
    size_t table[sizeof(search) - 1];
    build_substring_backtrack_table_reverse(search, search_len, table);
    
    size_t pos;
    if (!find_substring_reverse(type, strlen(type), search, search_len, table, &pos)) {
        *out_method_id = -1;
        return 1;
    }
    
    ASSERT(pos >= 0)
    ASSERT(pos <= strlen(type) - search_len)
    ASSERT(!memcmp(type + pos, search, search_len))
    
    char save = type[pos];
    type[pos] = '\0';
    int method_id = NCDMethodIndex_AddMethod(method_index, type, type + pos + search_len, module);
    type[pos] = save;
    
    if (method_id < 0) {
        BLog(BLOG_ERROR, "NCDMethodIndex_AddMethod failed");
        return 0;
    }
    
    *out_method_id = method_id;
    return 1;
}

int NCDModuleIndex_Init (NCDModuleIndex *o, NCDStringIndex *string_index)
{
    ASSERT(string_index)
    
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
    
    // init groups list
    LinkedList0_Init(&o->groups_list);
    
    // init method index
    if (!NCDMethodIndex_Init(&o->method_index, string_index)) {
        BLog(BLOG_ERROR, "NCDMethodIndex_Init failed");
        goto fail2;
    }
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail2:
    NCDModuleIndex__MHash_Free(&o->modules_hash);
fail1:
    BFree(o->modules);
fail0:
    return 0;
}

void NCDModuleIndex_Free (NCDModuleIndex *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free groups
    LinkedList0Node *ln;
    while (ln = LinkedList0_GetFirst(&o->groups_list)) {
        struct NCDModuleIndex_group *ig = UPPER_OBJECT(ln, struct NCDModuleIndex_group, groups_list_node);
        if (ig->group->func_globalfree) {
            ig->group->func_globalfree();
        }
        LinkedList0_Remove(&o->groups_list, &ig->groups_list_node);
        BFree(ig);
    }
    
    // free base types
    while (!BAVL_IsEmpty(&o->base_types_tree)) {
        struct NCDModuleIndex_base_type *bt = UPPER_OBJECT(BAVL_GetFirst(&o->base_types_tree), struct NCDModuleIndex_base_type, base_types_tree_node);
        BAVL_Remove(&o->base_types_tree, &bt->base_types_tree_node);
        free(bt);
    }
    
    // free method index
    NCDMethodIndex_Free(&o->method_index);
    
    // free modules hash
    NCDModuleIndex__MHash_Free(&o->modules_hash);
    
    // free modules array
    BFree(o->modules);
}

int NCDModuleIndex_AddGroup (NCDModuleIndex *o, const struct NCDModuleGroup *group, const struct NCDModuleInst_iparams *iparams, NCDStringIndex *string_index)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(group)
    ASSERT(iparams)
    ASSERT(string_index)
    
    struct NCDModuleIndex_group *ig = BAlloc(sizeof(*ig));
    if (!ig) {
        BLog(BLOG_ERROR, "BAlloc failed");
        goto fail0;
    }
    
    ig->group = group;
    LinkedList0_Prepend(&o->groups_list, &ig->groups_list_node);
    
    if (group->strings) {
        if (!NCDStringIndex_GetRequests(string_index, group->strings)) {
            BLog(BLOG_ERROR, "NCDStringIndex_GetRequests failed");
            goto fail1;
        }
    }
    
    if (group->func_globalinit) {
        if (!group->func_globalinit(iparams)) {
            BLog(BLOG_ERROR, "func_globalinit failed");
            goto fail1;
        }
    }
    
    int num_inited_modules = 0;
    
    for (struct NCDModule *nm = group->modules; nm->type; nm++) {
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
        
        const char *base_type = (nm->base_type ? nm->base_type : nm->type);
        ASSERT(base_type)
        
        nm->base_type_id = NCDStringIndex_Get(string_index, base_type);
        if (nm->base_type_id < 0) {
            BLog(BLOG_ERROR, "NCDStringIndex_Get failed");
            goto loop_fail0;
        }
        
        struct NCDModuleIndex_module *m = &o->modules[o->num_modules];
        
        strcpy(m->type, nm->type);
        m->module = nm;
        
        if (!add_method(m->type, nm, &o->method_index, &m->method_id)) {
            BLog(BLOG_ERROR, "failed to add method to method index");
            goto loop_fail0;
        }
        
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
        
        NCDModuleIndex__MHashRef ref = {m, o->num_modules};
        int res = NCDModuleIndex__MHash_Insert(&o->modules_hash, o->modules, ref, NULL);
        ASSERT_EXECUTE(res)
        
        o->num_modules++;
        num_inited_modules++;
        continue;
        
    loop_fail1:
        if (m->method_id >= 0) {
            NCDMethodIndex_RemoveMethod(&o->method_index, m->method_id);
        }
    loop_fail0:
        goto fail2;
    }
    
    return 1;
    
fail2:
    while (num_inited_modules-- > 0) {
        o->num_modules--;
        struct NCDModule *nm = group->modules + num_inited_modules;
        struct NCDModuleIndex_module *m = &o->modules[o->num_modules];
        
        NCDModuleIndex__MHashRef ref = {m, o->num_modules};
        NCDModuleIndex__MHash_Remove(&o->modules_hash, o->modules, ref);
        
        struct NCDModuleIndex_base_type *bt = find_base_type(o, (nm->base_type ? nm->base_type : nm->type));
        if (bt) {
            ASSERT(bt->group == group)
            BAVL_Remove(&o->base_types_tree, &bt->base_types_tree_node);
            free(bt);
        }
        
        if (m->method_id >= 0) {
            NCDMethodIndex_RemoveMethod(&o->method_index, m->method_id);
        }
    }
    if (group->func_globalfree) {
        group->func_globalfree();
    }
fail1:
    LinkedList0_Remove(&o->groups_list, &ig->groups_list_node);
    BFree(ig);
fail0:
    return 0;
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

int NCDModuleIndex_GetMethodNameId (NCDModuleIndex *o, const char *method_name)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(method_name)
    
    return NCDMethodIndex_GetMethodNameId(&o->method_index, method_name);
}

const struct NCDModule * NCDModuleIndex_GetMethodModule (NCDModuleIndex *o, NCD_string_id_t obj_type, int method_name_id)
{
    DebugObject_Access(&o->d_obj);
    
    return NCDMethodIndex_GetMethodModule(&o->method_index, obj_type, method_name_id);
}
