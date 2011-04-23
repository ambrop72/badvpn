/**
 * @file BStringMap.c
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

#include <stdlib.h>
#include <string.h>

#include <misc/offset.h>

#include <stringmap/BStringMap.h>

static int string_comparator (void *unused, char **str1, char **str2)
{
    int c = strcmp(*str1, *str2);
    if (c < 0) {
        return -1;
    }
    if (c > 0) {
        return 1;
    }
    return 0;
}

static void free_entry (BStringMap *o, struct BStringMap_entry *e)
{
    BAVL_Remove(&o->tree, &e->tree_node);
    free(e->value);
    free(e->key);
    free(e);
}

void BStringMap_Init (BStringMap *o)
{
    // init tree
    BAVL_Init(&o->tree, OFFSET_DIFF(struct BStringMap_entry, key, tree_node), (BAVL_comparator)string_comparator, NULL);
    
    DebugObject_Init(&o->d_obj);
}

int BStringMap_InitCopy (BStringMap *o, const BStringMap *src)
{
    BStringMap_Init(o);
    
    const char *key = BStringMap_First(src);
    while (key) {
        if (!BStringMap_Set(o, key, BStringMap_Get(src, key))) {
            goto fail1;
        }
        key = BStringMap_Next(src, key);
    }
    
    return 1;
    
fail1:
    BStringMap_Free(o);
    return 0;
}

void BStringMap_Free (BStringMap *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free entries
    BAVLNode *tree_node;
    while (tree_node = BAVL_GetFirst(&o->tree)) {
        struct BStringMap_entry *e = UPPER_OBJECT(tree_node, struct BStringMap_entry, tree_node);
        free_entry(o, e);
    }
}

const char * BStringMap_Get (const BStringMap *o, const char *key)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(key)
    
    // lookup
    BAVLNode *tree_node = BAVL_LookupExact(&o->tree, &key);
    if (!tree_node) {
        return NULL;
    }
    struct BStringMap_entry *e = UPPER_OBJECT(tree_node, struct BStringMap_entry, tree_node);
    
    return e->value;
}

int BStringMap_Set (BStringMap *o, const char *key, const char *value)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(key)
    ASSERT(value)
    
    // alloc entry
    struct BStringMap_entry *e = malloc(sizeof(*e));
    if (!e) {
        goto fail0;
    }
    
    // alloc and set key
    if (!(e->key = malloc(strlen(key) + 1))) {
        goto fail1;
    }
    strcpy(e->key, key);
    
    // alloc and set value
    if (!(e->value = malloc(strlen(value) + 1))) {
        goto fail2;
    }
    strcpy(e->value, value);
    
    // try inserting to tree
    BAVLNode *ex_tree_node;
    if (!BAVL_Insert(&o->tree, &e->tree_node, &ex_tree_node)) {
        // remove existing entry
        struct BStringMap_entry *ex_e = UPPER_OBJECT(ex_tree_node, struct BStringMap_entry, tree_node);
        free_entry(o, ex_e);
        
        // insert new node
        ASSERT_EXECUTE(BAVL_Insert(&o->tree, &e->tree_node, NULL))
    }
    
    return 1;
    
fail2:
    free(e->key);
fail1:
    free(e);
fail0:
    return 0;
}

void BStringMap_Unset (BStringMap *o, const char *key)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(key)
    
    // lookup
    BAVLNode *tree_node = BAVL_LookupExact(&o->tree, &key);
    if (!tree_node) {
        return;
    }
    struct BStringMap_entry *e = UPPER_OBJECT(tree_node, struct BStringMap_entry, tree_node);
    
    // remove
    free_entry(o, e);
}

const char * BStringMap_First (const BStringMap *o)
{
    DebugObject_Access(&o->d_obj);
    
    // get first
    BAVLNode *tree_node = BAVL_GetFirst(&o->tree);
    if (!tree_node) {
        return NULL;
    }
    struct BStringMap_entry *e = UPPER_OBJECT(tree_node, struct BStringMap_entry, tree_node);
    
    return e->key;
}

const char * BStringMap_Next (const BStringMap *o, const char *key)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(key)
    ASSERT(BAVL_LookupExact(&o->tree, &key))
    
    // get entry
    struct BStringMap_entry *e = UPPER_OBJECT(BAVL_LookupExact(&o->tree, &key), struct BStringMap_entry, tree_node);
    
    // get next
    BAVLNode *tree_node = BAVL_GetNext(&o->tree, &e->tree_node);
    if (!tree_node) {
        return NULL;
    }
    struct BStringMap_entry *next_e = UPPER_OBJECT(tree_node, struct BStringMap_entry, tree_node);
    
    return next_e->key;
}
