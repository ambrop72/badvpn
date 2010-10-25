/**
 * @file BHeap.h
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
 * 
 * @section DESCRIPTION
 * 
 * Binary heap.
 */

#ifndef BADVPN_STRUCTURE_BHEAP_H
#define BADVPN_STRUCTURE_BHEAP_H

//#define BHEAP_DEBUG

#include <stdint.h>
#include <stddef.h>

#include <misc/debug.h>

/**
 * Handler function called by heap algorithms to compare two values.
 * For any two values, the comparator must always return the same result.
 * The <= relation defined by the comparator must be a total order.
 * Values are obtained like that:
 *   - The value of a node in the heap, or a node that is being inserted is:
 *     (uint8_t *)node + offset.
 *   - The value being looked up is the same as given to the lookup function.
 * 
 * @param user as in {@link BHeap_Init}
 * @param val1 first value
 * @param val2 second value
 * @return -1 if val1 < val2, 0 if val1 = val2, 1 if val1 > val2
 */
typedef int (*BHeap_comparator) (void *user, void *val1, void *val2);

struct BHeapNode;

/**
 * Binary heap.
 */
typedef struct {
    int offset;
    BHeap_comparator comparator;
    void *user;
    struct BHeapNode *root;
    struct BHeapNode *last;
    #ifndef NDEBUG
    int in_handler;
    #endif
} BHeap;

/**
 * Binary heap node.
 */
typedef struct BHeapNode {
    struct BHeapNode *parent;
    struct BHeapNode *link[2];
} BHeapNode;

/**
 * Initializes the heap.
 * 
 * @param o heap to initialize
 * @param offset offset of a value from its node
 * @param comparator value comparator handler function
 * @param user value to pass to comparator
 */
static void BHeap_Init (BHeap *h, int offset, BHeap_comparator comparator, void *user);

/**
 * Inserts a node into the heap.
 * Must not be called from comparator.
 * 
 * @param o the heap
 * @param node uninitialized node to insert. Must have a valid value (its value
 *             may be passed to the comparator during insertion).
 */
static void BHeap_Insert (BHeap *h, BHeapNode *node);

/**
 * Removes a node from the heap.
 * Must not be called from comparator.
 * 
 * @param o the heap
 * @param node node to remove
 */
static void BHeap_Remove (BHeap *h, BHeapNode *node);

/**
 * Returns one of the smallest nodes in the heap.
 * Must not be called from comparator.
 * 
 * @param o the heap
 * @return one of the smallest nodes in the heap, or NULL if there
 *         are no nodes
 */
static BHeapNode * BHeap_GetFirst (BHeap *h);

static void * _BHeap_node_value (BHeap *o, BHeapNode *n)
{
    return ((uint8_t *)n + o->offset);
}

static int _BHeap_compare_values (BHeap *o, void *v1, void *v2)
{
    #ifndef NDEBUG
    o->in_handler = 1;
    #endif
    
    int res = o->comparator(o->user, v1, v2);
    
    #ifndef NDEBUG
    o->in_handler = 0;
    #endif
    
    ASSERT(res == -1 || res == 0 || res == 1)
    
    return res;
}

static int _BHeap_compare_nodes (BHeap *o, BHeapNode *n1, BHeapNode *n2)
{
    return _BHeap_compare_values(o, _BHeap_node_value(o, n1), _BHeap_node_value(o, n2));
}

#ifdef BHEAP_DEBUG
#define BHEAP_ASSERT(_h) _BHeap_assert(_h);
#else
#define BHEAP_ASSERT(_h)
#endif

#ifdef BHEAP_DEBUG

struct _BHeap_assert_data {
    int state;
    int level;
    BHeapNode *prev_leaf;
};

#define BHEAP_ASSERT_STATE_NODEPTH 1
#define BHEAP_ASSERT_STATE_LOWEST 2
#define BHEAP_ASSERT_STATE_LOWESTEND 3

static void _BHeap_assert_recurser (BHeap *h, BHeapNode *n, struct _BHeap_assert_data *ad, int level)
{
    if (!n->link[0] && !n->link[1]) {
        if (ad->state == BHEAP_ASSERT_STATE_NODEPTH) {
            // leftmost none, remember depth
            ad->state = BHEAP_ASSERT_STATE_LOWEST;
            ad->level = level;
        }
    } else {
        // drop down
        if (n->link[0]) {
            ASSERT(_BHeap_compare_nodes(h, n, n->link[0]) <= 0)
            ASSERT(n->link[0]->parent == n)
            _BHeap_assert_recurser(h, n->link[0], ad, level + 1);
        }
        if (n->link[1]) {
            ASSERT(_BHeap_compare_nodes(h, n, n->link[1]) <= 0)
            ASSERT(n->link[1]->parent == n)
            _BHeap_assert_recurser(h, n->link[1], ad, level + 1);
        }
    }
    
    ASSERT(ad->state == BHEAP_ASSERT_STATE_LOWEST || ad->state == BHEAP_ASSERT_STATE_LOWESTEND)
    
    if (level < ad->level - 1) {
        ASSERT(n->link[0] && n->link[1])
    }
    else if (level == ad->level - 1) {
        switch (ad->state) {
            case BHEAP_ASSERT_STATE_LOWEST:
                if (!n->link[0]) {
                    ad->state = BHEAP_ASSERT_STATE_LOWESTEND;
                    ASSERT(!n->link[1])
                    ASSERT(ad->prev_leaf == h->last)
                } else {
                    if (!n->link[1]) {
                        ad->state = BHEAP_ASSERT_STATE_LOWESTEND;
                        ASSERT(ad->prev_leaf == h->last)
                    }
                }
                break;
            case BHEAP_ASSERT_STATE_LOWESTEND:
                ASSERT(!n->link[0] && !n->link[1])
                break;
        }
    }
    else if (level == ad->level) {
        ASSERT(ad->state == BHEAP_ASSERT_STATE_LOWEST)
        ASSERT(!n->link[0] && !n->link[1])
        ad->prev_leaf = n;
    }
    else {
        ASSERT(0)
    }
}

static void _BHeap_assert (BHeap *h)
{
    struct _BHeap_assert_data ad;
    ad.state = BHEAP_ASSERT_STATE_NODEPTH;
    ad.prev_leaf = NULL;
    
    if (h->root) {
        ASSERT(h->last)
        ASSERT(!h->root->parent)
        _BHeap_assert_recurser(h, h->root, &ad, 0);
        if (ad.state == BHEAP_ASSERT_STATE_LOWEST) {
            ASSERT(ad.prev_leaf == h->last)
        }
    } else {
        ASSERT(!h->last)
    }
}

#endif

static void _BHeap_move_one_up (BHeap *h, BHeapNode *n)
{
    ASSERT(n->parent)
    
    BHeapNode *p = n->parent;
    
    if (p->parent) {
        p->parent->link[p == p->parent->link[1]] = n;
    } else {
        h->root = n;
    }
    n->parent = p->parent;
    
    int nside = (n == p->link[1]);
    BHeapNode *c = p->link[!nside];
    
    p->link[0] = n->link[0];
    if (p->link[0]) {
        p->link[0]->parent = p;
    }
    
    p->link[1] = n->link[1];
    if (p->link[1]) {
        p->link[1]->parent = p;
    }
    
    n->link[nside] = p;
    p->parent = n;
    
    n->link[!nside] = c;
    if (c) {
        c->parent = n;
    }
    
    if (n == h->last) {
        h->last = p;
    }
}

static void _BHeap_replace_node (BHeap *h, BHeapNode *d, BHeapNode *s)
{
    if (d->parent) {
        d->parent->link[d == d->parent->link[1]] = s;
    } else {
        h->root = s;
    }
    s->parent = d->parent;
    
    s->link[0] = d->link[0];
    if (s->link[0]) {
        s->link[0]->parent = s;
    }
    
    s->link[1] = d->link[1];
    if (s->link[1]) {
        s->link[1]->parent = s;
    }
}

void BHeap_Init (BHeap *h, int offset, BHeap_comparator comparator, void *user)
{
    h->offset = offset;
    h->comparator = comparator;
    h->user = user;
    h->root = NULL;
    h->last = NULL;
    
    #ifndef NDEBUG
    h->in_handler = 0;
    #endif
    
    BHEAP_ASSERT(h)
}

void BHeap_Insert (BHeap *h, BHeapNode *node)
{
    ASSERT(!h->in_handler)
    
    if (!h->root) {
        // insert to root
        h->root = node;
        h->last = node;
        node->parent = NULL;
        node->link[0] = NULL;
        node->link[1] = NULL;
        
        BHEAP_ASSERT(h)
        return;
    }
    
    // find the node to insert to
    
    // start with current last node and walk up left as much as possible.
    // That is, keep replacing the current node with the parent as long as it
    // exists and the current node is its right child.
    BHeapNode *cur = h->last;
    while (cur->parent && cur == cur->parent->link[1]) {
        cur = cur->parent;
    }
    
    if (cur->parent) {
        if (cur->parent->link[1]) {
            // have parent and parent has right child. Attach the new node
            // to the leftmost node of the parent's right subtree.
            cur = cur->parent->link[1];
            while (cur->link[0]) {
                cur = cur->link[0];
            }
        } else {
            // have parent, but parent has no right child. This can
            // only happen when the last node is a right child. So
            // attach the new node to its parent.
            cur = cur->parent;
        }
    } else {
        // have no parent, attach the new node to a new level. We're at the
        // root, so drop down left to obtain the node where we'll attach
        // the new node.
        while (cur->link[0]) {
            cur = cur->link[0];
        }
    }
    
    ASSERT((!cur->link[0] && !cur->link[1]) || (cur->link[0] && !cur->link[1]))
    
    // attach new node
    // the new node becomes the new last node
    h->last = node;
    cur->link[!!cur->link[0]] = node;
    node->parent = cur;
    node->link[0] = NULL;
    node->link[1] = NULL;
    
    // restore heap property
    while (node->parent && _BHeap_compare_nodes(h, node->parent, node) > 0) {
        _BHeap_move_one_up(h, node);
    }
    
    BHEAP_ASSERT(h)
}

void BHeap_Remove (BHeap *h, BHeapNode *node)
{
    ASSERT(!h->in_handler)
    
    if (!node->parent && !node->link[0] && !node->link[1]) {
        h->root = NULL;
        h->last = NULL;
        
        BHEAP_ASSERT(h)
        return;
    }
    
    // locate the node before the last node
    BHeapNode *cur = h->last;
    while (cur->parent && cur == cur->parent->link[0]) {
        cur = cur->parent;
    }
    if (cur->parent) {
        ASSERT(cur->parent->link[0])
        cur = cur->parent->link[0];
        while (cur->link[1]) {
            cur = cur->link[1];
        }
    } else {
        while (cur->link[1]) {
            cur = cur->link[1];
        }
    }
    
    // disconnect last
    ASSERT(h->last->parent)
    h->last->parent->link[h->last == h->last->parent->link[1]] = NULL;
    
    if (node == h->last) {
        // deleting last; set new last
        h->last = cur;
    } else {
        // not deleting last; move last to node's place
        BHeapNode *srcnode = h->last;
        _BHeap_replace_node(h, node, srcnode);
        // set new last unless node=cur; in this case it stays the same
        if (node != cur) {
            h->last = cur;
        }
        
        // restore heap property
        if (srcnode->parent && _BHeap_compare_nodes(h, srcnode, srcnode->parent) < 0) {
            do {
                _BHeap_move_one_up(h, srcnode);
            } while (srcnode->parent && _BHeap_compare_nodes(h, srcnode, srcnode->parent) < 0);
        } else {
            while (srcnode->link[0] || srcnode->link[1]) {
                int side = (srcnode->link[1] && (_BHeap_compare_nodes(h, srcnode->link[0], srcnode->link[1]) >= 0));
                if (_BHeap_compare_nodes(h, srcnode, srcnode->link[side]) > 0) {
                    _BHeap_move_one_up(h, srcnode->link[side]);
                } else {
                    break;
                }
            }
        }
    }
    
    BHEAP_ASSERT(h)
}

BHeapNode * BHeap_GetFirst (BHeap *h)
{
    ASSERT(!h->in_handler)
    
    return h->root;
}

#endif
