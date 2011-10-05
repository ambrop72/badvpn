/**
 * @file NCDModuleIndex.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * This file is part of BadVPN.
 * 
 * BadVPN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * BadVPN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string.h>
#include <stdlib.h>

#include <misc/offset.h>
#include <base/BLog.h>

#include "NCDModuleIndex.h"

#include <generated/blog_channel_NCDModuleIndex.h>

static int string_comparator (void *user, const char *s1, const char *s2)
{
    int cmp = strcmp(s1, s2);
    if (cmp < 0) {
        return -1;
    }
    if (cmp > 0) {
        return 1;
    }
    return 0;
}

static int string_pointer_comparator (void *user, const char **s1, const char **s2)
{
    int cmp = strcmp(*s1, *s2);
    if (cmp < 0) {
        return -1;
    }
    if (cmp > 0) {
        return 1;
    }
    return 0;
}

static struct NCDModuleIndex_module * find_module (NCDModuleIndex *o, const char *type)
{
    BAVLNode *node = BAVL_LookupExact(&o->modules_tree, (void *)type);
    if (!node) {
        return NULL;
    }
    
    struct NCDModuleIndex_module *m = UPPER_OBJECT(node, struct NCDModuleIndex_module, modules_tree_node);
    ASSERT(!strcmp(m->type, type))
    
    return m;
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

void NCDModuleIndex_Init (NCDModuleIndex *o)
{
    // init modules tree
    BAVL_Init(&o->modules_tree, OFFSET_DIFF(struct NCDModuleIndex_module, type, modules_tree_node), (BAVL_comparator)string_comparator, NULL);
    
    // init base types tree
    BAVL_Init(&o->base_types_tree, OFFSET_DIFF(struct NCDModuleIndex_base_type, base_type, base_types_tree_node), (BAVL_comparator)string_pointer_comparator, NULL);
    
    DebugObject_Init(&o->d_obj);
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
    
    // free modules
    while (!BAVL_IsEmpty(&o->modules_tree)) {
        struct NCDModuleIndex_module *m = UPPER_OBJECT(BAVL_GetFirst(&o->modules_tree), struct NCDModuleIndex_module, modules_tree_node);
        BAVL_Remove(&o->modules_tree, &m->modules_tree_node);
        free(m);
    }
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
            BLog(BLOG_ERROR, "module type '%s' is too long", nm->type);
            goto loop_fail0;
        }
        
        struct NCDModuleIndex_module *m = malloc(sizeof(*m));
        if (!m) {
            BLog(BLOG_ERROR, "malloc failed");
            goto loop_fail0;
        }
        
        strcpy(m->type, nm->type);
        m->module = nm;
        ASSERT_EXECUTE(BAVL_Insert(&o->modules_tree, &m->modules_tree_node, NULL))
        
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
        
        continue;
        
    loop_fail1:
        BAVL_Remove(&o->modules_tree, &m->modules_tree_node);
        free(m);
    loop_fail0:
        return 0;
    }
    
    return 1;
}

const struct NCDModule * NCDModuleIndex_FindModule (NCDModuleIndex *o, const char *type)
{
    DebugObject_Access(&o->d_obj);
    
    struct NCDModuleIndex_module *m = find_module(o, type);
    if (!m) {
        return NULL;
    }
    
    return m->module;
}
