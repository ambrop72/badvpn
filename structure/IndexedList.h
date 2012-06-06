/**
 * @file IndexedList.h
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
 * 
 * @section DESCRIPTION
 * 
 * A data structure similar to a list, but with efficient index-based access.
 */

#ifndef BADVPN_INDEXEDLIST_H
#define BADVPN_INDEXEDLIST_H

#include <stddef.h>
#include <stdint.h>

#include <misc/offset.h>
#include <structure/CAvl.h>

struct _IndexedList_key {
    int is_spec;
    uint64_t spec_key;
};

typedef struct IndexedList_s IndexedList;
typedef struct IndexedListNode_s IndexedListNode;

typedef struct _IndexedList_key *IndexedList__tree_key;
typedef IndexedListNode *IndexedList__tree_link;
typedef IndexedList *IndexedList__tree_arg;

#include "IndexedList_tree.h"
#include <structure/CAvl_decl.h>

struct IndexedList_s {
    IndexedList__Tree tree;
    int inserting;
    uint64_t inserting_index;
};

struct IndexedListNode_s {
    struct _IndexedList_key key;
    IndexedListNode *tree_link[2];
    IndexedListNode *tree_parent;
    int8_t tree_balance;
    uint64_t tree_count;
};

/**
 * Initializes the indexed list.
 * 
 * @param o uninitialized list object to initialize
 */
static void IndexedList_Init (IndexedList *o);

/**
 * Inserts a node into the indexed list.
 * 
 * @param o indexed list to insert into
 * @param node uninitialized node to insert
 * @param index index to insert at (starting with zero). Any existing elements
 *              at or after this index will be shifted forward, i.e. their
 *              indices will be incremented by one. Must be <=count.
 */
static void IndexedList_InsertAt (IndexedList *o, IndexedListNode *node, uint64_t index);

/**
 * Removes a nove from the indexed list.
 * 
 * @param o indexed list to remove from
 * @param node node in the list to remove
 */
static void IndexedList_Remove (IndexedList *o, IndexedListNode *node);

/**
 * Returns the number of nodes in the indexed list.
 * 
 * @param o indexed list
 * @return number of nodes
 */
static uint64_t IndexedList_Count (IndexedList *o);

/**
 * Returns the index of a node in the indexed list.
 * 
 * @param o indexed list
 * @param node node in the list to get index of
 * @return index of the node
 */
static uint64_t IndexedList_IndexOf (IndexedList *o, IndexedListNode *node);

/**
 * Returns the node at the specified index in the indexed list.
 * 
 * @param o indexed list
 * @param index index of the node to return. Must be < count.
 * @return node at the specified index
 */
static IndexedListNode * IndexedList_GetAt (IndexedList *o, uint64_t index);

static int _IndexedList_comparator (IndexedList *o, struct _IndexedList_key *k1, struct _IndexedList_key *k2)
{
    uint64_t i1;
    if (k1->is_spec) {
        i1 = k1->spec_key;
    } else {
        IndexedListNode *n1 = UPPER_OBJECT(k1, IndexedListNode, key);
        i1 = IndexedList__Tree_IndexOf(&o->tree, o, IndexedList__Tree_Deref(o, n1));
        if (o->inserting && i1 >= o->inserting_index) {
            i1++;
        }
    }
    
    uint64_t i2;
    if (k2->is_spec) {
        i2 = k2->spec_key;
    } else {
        IndexedListNode *n2 = UPPER_OBJECT(k2, IndexedListNode, key);
        i2 = IndexedList__Tree_IndexOf(&o->tree, o, IndexedList__Tree_Deref(o, n2));
        if (o->inserting && i2 >= o->inserting_index) {
            i2++;
        }
    }
    
    return (i1 > i2) - (i1 < i2);
}

#include "IndexedList_tree.h"
#include <structure/CAvl_impl.h>

static void IndexedList_Init (IndexedList *o)
{
    IndexedList__Tree_Init(&o->tree);
    o->inserting = 0;
}

static void IndexedList_InsertAt (IndexedList *o, IndexedListNode *node, uint64_t index)
{
    ASSERT(index <= IndexedList__Tree_Count(&o->tree, o))
    ASSERT(IndexedList__Tree_Count(&o->tree, o) < UINT64_MAX - 1)
    ASSERT(!o->inserting)
    
    uint64_t orig_count = IndexedList__Tree_Count(&o->tree, o);
    
    // give this node the key 'index'
    node->key.is_spec = 1;
    node->key.spec_key = index;
    
    // make all existing nodes at positions >='index' assume keys one more
    // than their positions
    o->inserting = 1;
    o->inserting_index = index;
    
    // insert new node
    int res = IndexedList__Tree_Insert(&o->tree, o, IndexedList__Tree_Deref(o, node), NULL);
    ASSERT(res)
    
    // positions have been updated by insertions, remove position
    // increments
    o->inserting = 0;
    
    // node has been inserted, have it assume index of its position
    node->key.is_spec = 0;
    
    ASSERT(IndexedList__Tree_IndexOf(&o->tree, o, IndexedList__Tree_Deref(o, node)) == index)
    ASSERT(IndexedList__Tree_Count(&o->tree, o) == orig_count + 1)
}

static void IndexedList_Remove (IndexedList *o, IndexedListNode *node)
{
    IndexedList__Tree_Remove(&o->tree, o, IndexedList__Tree_Deref(o, node));
}

static uint64_t IndexedList_Count (IndexedList *o)
{
    return IndexedList__Tree_Count(&o->tree, o);
}

static uint64_t IndexedList_IndexOf (IndexedList *o, IndexedListNode *node)
{
    return IndexedList__Tree_IndexOf(&o->tree, o, IndexedList__Tree_Deref(o, node));
}

static IndexedListNode * IndexedList_GetAt (IndexedList *o, uint64_t index)
{
    ASSERT(index <= IndexedList__Tree_Count(&o->tree, o))
    
    IndexedList__TreeNode ref = IndexedList__Tree_GetAt(&o->tree, o, index);
    ASSERT(ref.link != IndexedList__TreeNullLink)
    
    return ref.ptr;
}

#endif
