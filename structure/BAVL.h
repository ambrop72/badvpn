/**
 * @file BAVL.h
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
 * AVL tree.
 */

#ifndef BADVPN_STRUCTURE_BAVL_H
#define BADVPN_STRUCTURE_BAVL_H

//#define BAVL_DEBUG

#include <stdint.h>
#include <stddef.h>

#include <misc/debug.h>

/**
 * Handler function called by tree algorithms to compare two values.
 * For any two values, the comparator must always return the same result.
 * The <= relation defined by the comparator must be a total order.
 * Values are obtained like that:
 *   - The value of a node in the tree, or a node that is being inserted is:
 *     (uint8_t *)node + offset.
 *   - The value being looked up is the same as given to the lookup function.
 * 
 * @param user as in {@link BAVL_Init}
 * @param val1 first value
 * @param val2 second value
 * @return -1 if val1 < val2, 0 if val1 = val2, 1 if val1 > val2
 */
typedef int (*BAVL_comparator) (void *user, void *val1, void *val2);

struct BAVLNode;

/**
 * AVL tree.
 */
typedef struct {
    int offset;
    BAVL_comparator comparator;
    void *user;
    struct BAVLNode *root;
    #ifndef NDEBUG
    int in_handler;
    #endif
} BAVL;

/**
 * AVL tree node.
 */
typedef struct BAVLNode {
    struct BAVLNode *parent;
    struct BAVLNode *link[2];
    int balance;
} BAVLNode;

/**
 * Initializes the tree.
 * 
 * @param o tree to initialize
 * @param offset offset of a value from its node
 * @param comparator value comparator handler function
 * @param user value to pass to comparator
 */
static void BAVL_Init (BAVL *o, int offset, BAVL_comparator comparator, void *user);

/**
 * Inserts a node into the tree.
 * Must not be called from comparator.
 * 
 * @param o the tree
 * @param node uninitialized node to insert. Must have a valid value (its value
 *             may be passed to the comparator during insertion).
 * @param ref if not NULL, will return (regardless if insertion succeeded):
 *              - the greatest node lesser than the inserted value, or (not in order)
 *              - the smallest node greater than the inserted value, or
 *              - NULL meaning there were no nodes in the tree.
 * @param 1 on success, 0 if an element with an equal value is already in the tree
 */
static int BAVL_Insert (BAVL *o, BAVLNode *node, BAVLNode **ref);

/**
 * Removes a node from the tree.
 * Must not be called from comparator.
 * 
 * @param o the tree
 * @param node node to remove
 */
static void BAVL_Remove (BAVL *o, BAVLNode *node);

/**
 * Checks if the tree is empty.
 * Must not be called from comparator.
 * 
 * @param o the tree
 * @return 1 if empty, 0 if not
 */
static int BAVL_IsEmpty (BAVL *o);

/**
 * Looks for a value in the tree.
 * Must not be called from comparator.
 * 
 * @param o the tree
 * @param val value to look for
 * @return If a node is in the thee with an equal value, that node.
 *         Else if the tree is not empty:
 *           - the greatest node lesser than the given value, or (not in order)
 *           - the smallest node greater than the given value.
 *         NULL if the tree is empty.
 */
static BAVLNode * BAVL_Lookup (BAVL *o, void *val);

/**
 * Looks for a value in the tree.
 * Must not be called from comparator.
 * 
 * @param o the tree
 * @param val value to look for
 * @return If a node is in the thee with an equal value, that node.
 *         Else NULL.
 */
static BAVLNode * BAVL_LookupExact (BAVL *o, void *val);

#define BAVL_MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))
#define BAVL_OPTNEG(_a, _neg) ((_neg) ? -(_a) : (_a))

static void * _BAVL_node_value (BAVL *o, BAVLNode *n)
{
    return ((uint8_t *)n + o->offset);
}

static int _BAVL_compare_values (BAVL *o, void *v1, void *v2)
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

static int _BAVL_compare_nodes (BAVL *o, BAVLNode *n1, BAVLNode *n2)
{
    return _BAVL_compare_values(o, _BAVL_node_value(o, n1), _BAVL_node_value(o, n2));
}

#ifdef BAVL_DEBUG
#define BAVL_ASSERT(_h) _BAVL_assert(_h);
#else
#define BAVL_ASSERT(_h)
#endif

#ifdef BAVL_DEBUG

static int _BAVL_assert_recurser (BAVL *o, BAVLNode *n)
{
    ASSERT(n->balance >= -1 && n->balance <= 1)
    
    int height_left = 0;
    int height_right = 0;
    
    // check left subtree
    if (n->link[0]) {
        // check parent link
        ASSERT(n->link[0]->parent == n)
        // check binary search tree
        ASSERT(_BAVL_compare_nodes(o, n->link[0], n) == -1)
        // recursively calculate height
        height_left = _BAVL_assert_recurser(o, n->link[0]);
    }
    
    // check right subtree
    if (n->link[1]) {
        // check parent link
        ASSERT(n->link[1]->parent == n)
        // check binary search tree
        ASSERT(_BAVL_compare_nodes(o, n->link[1], n) == 1)
        // recursively calculate height
        height_right = _BAVL_assert_recurser(o, n->link[1]);
    }
    
    // check balance factor
    ASSERT(n->balance == height_right - height_left)
    
    return (BAVL_MAX(height_left, height_right) + 1);
}

static void _BAVL_assert (BAVL *o)
{
    if (o->root) {
        ASSERT(!o->root->parent)
        _BAVL_assert_recurser(o, o->root);
    }
}

#endif

static void _BAVL_rotate (BAVL *tree, BAVLNode *r, int dir)
{
    BAVLNode *nr = r->link[!dir];
    
    r->link[!dir] = nr->link[dir];
    if (r->link[!dir]) {
        r->link[!dir]->parent = r;
    }
    nr->link[dir] = r;
    nr->parent = r->parent;
    if (nr->parent) {
        nr->parent->link[r == r->parent->link[1]] = nr;
    } else {
        tree->root = nr;
    }
    r->parent = nr;
}

static BAVLNode * _BAVL_subtree_max (BAVLNode *n)
{
    ASSERT(n)
    while (n->link[1]) {
        n = n->link[1];
    }
    return n;
}

static void _BAVL_replace_subtree (BAVL *tree, BAVLNode *dest, BAVLNode *n)
{
    ASSERT(dest)
    
    if (dest->parent) {
        dest->parent->link[dest == dest->parent->link[1]] = n;
    } else {
        tree->root = n;
    }
    if (n) {
        n->parent = dest->parent;
    }
}

static void _BAVL_swap_nodes (BAVL *tree, BAVLNode *n1, BAVLNode *n2)
{
    if (n2->parent == n1 || n1->parent == n2) {
        // when the nodes are directly connected we need special handling
        // make sure n1 is above n2
        if (n1->parent == n2) {
            BAVLNode *t = n1;
            n1 = n2;
            n2 = t;
        }
        
        int side = (n2 == n1->link[1]);
        BAVLNode *c = n1->link[!side];
        
        if (n1->link[0] = n2->link[0]) {
            n1->link[0]->parent = n1;
        }
        if (n1->link[1] = n2->link[1]) {
            n1->link[1]->parent = n1;
        }
        
        if (n2->parent = n1->parent) {
            n2->parent->link[n1 == n1->parent->link[1]] = n2;
        } else {
            tree->root = n2;
        }
        
        n2->link[side] = n1;
        n1->parent = n2;
        if (n2->link[!side] = c) {
            c->parent = n2;
        }
    } else {
        BAVLNode *temp;
        
        // swap parents
        temp = n1->parent;
        if (n1->parent = n2->parent) {
            n1->parent->link[n2 == n2->parent->link[1]] = n1;
        } else {
            tree->root = n1;
        }
        if (n2->parent = temp) {
            n2->parent->link[n1 == temp->link[1]] = n2;
        } else {
            tree->root = n2;
        }
        
        // swap left children
        temp = n1->link[0];
        if (n1->link[0] = n2->link[0]) {
            n1->link[0]->parent = n1;
        }
        if (n2->link[0] = temp) {
            n2->link[0]->parent = n2;
        }
        
        // swap right children
        temp = n1->link[1];
        if (n1->link[1] = n2->link[1]) {
            n1->link[1]->parent = n1;
        }
        if (n2->link[1] = temp) {
            n2->link[1]->parent = n2;
        }
    }
    
    // swap balance factors
    int b = n1->balance;
    n1->balance = n2->balance;
    n2->balance = b;
}

static void _BAVL_rebalance (BAVL *o, BAVLNode *node, int side, int deltac)
{
    ASSERT(side == 0 || side == 1)
    ASSERT(deltac >= -1 && deltac <= 1)
    ASSERT(node->balance >= -1 && node->balance <= 1)
    
    // if no subtree changed its height, no more rebalancing is needed
    if (deltac == 0) {
        return;
    }
    
    // calculate how much our height changed
    int delta = BAVL_MAX(deltac, BAVL_OPTNEG(node->balance, side)) - BAVL_MAX(0, BAVL_OPTNEG(node->balance, side));
    ASSERT(delta >= -1 && delta <= 1)
    
    // update our balance factor
    node->balance -= BAVL_OPTNEG(deltac, side);
    
    BAVLNode *child;
    BAVLNode *gchild;
    
    // perform transformations if the balance factor is wrong
    if (node->balance == 2 || node->balance == -2) {
        int bside;
        int bsidef;
        if (node->balance == 2) {
            bside = 1;
            bsidef = 1;
        } else {
            bside = 0;
            bsidef = -1;
        }
        
        ASSERT(node->link[bside])
        child = node->link[bside];
        switch (child->balance * bsidef) {
            case 1:
                _BAVL_rotate(o, node, !bside);
                node->balance = 0;
                child->balance = 0;
                node = child;
                delta -= 1;
                break;
            case 0:
                _BAVL_rotate(o, node, !bside);
                node->balance = 1 * bsidef;
                child->balance = -1 * bsidef;
                node = child;
                break;
            case -1:
                ASSERT(child->link[!bside])
                gchild = child->link[!bside];
                _BAVL_rotate(o, child, bside);
                _BAVL_rotate(o, node, !bside);
                node->balance = -BAVL_MAX(0, gchild->balance * bsidef) * bsidef;
                child->balance = BAVL_MAX(0, -gchild->balance * bsidef) * bsidef;
                gchild->balance = 0;
                node = gchild;
                delta -= 1;
                break;
            default:
                ASSERT(0);
        }
    }
    
    ASSERT(delta >= -1 && delta <= 1)
    // Transformations above preserve this. Proof:
    //     - if a child subtree gained 1 height and rebalancing was needed,
    //       it was the heavier subtree. Then delta was was originally 1, because
    //       the heaviest subtree gained one height. If the transformation reduces
    //       delta by one, it becomes 0.
    //     - if a child subtree lost 1 height and rebalancing was needed, it
    //       was the lighter subtree. Then delta was originally 0, because
    //       the height of the heaviest subtree was unchanged. If the transformation
    //       reduces delta by one, it becomes -1.
    
    if (node->parent) {
        _BAVL_rebalance(o, node->parent, node == node->parent->link[1], delta);
    }
}

void BAVL_Init (BAVL *o, int offset, BAVL_comparator comparator, void *user)
{
    o->offset = offset;
    o->comparator = comparator;
    o->user = user;
    o->root = NULL;
    
    #ifndef NDEBUG
    o->in_handler = 0;
    #endif
    
    BAVL_ASSERT(o)
}

int BAVL_Insert (BAVL *o, BAVLNode *node, BAVLNode **ref)
{
    ASSERT(!o->in_handler)
    
    // insert to root?
    if (!o->root) {
        o->root = node;
        node->parent = NULL;
        node->link[0] = NULL;
        node->link[1] = NULL;
        node->balance = 0;
        
        BAVL_ASSERT(o)
        
        if (ref) {
            *ref = NULL;
        }
        return 1;
    }
    
    // find node to insert to
    BAVLNode *c = o->root;
    int side;
    while (1) {
        // compare
        int comp = _BAVL_compare_nodes(o, node, c);
        
        // have we found a node that compares equal?
        if (comp == 0) {
            if (ref) {
                *ref = c;
            }
            return 0;
        }
        
        side = (comp == 1);
        
        // have we reached a leaf?
        if (!c->link[side]) {
            break;
        }
        
        c = c->link[side];
    }
    
    // insert
    c->link[side] = node;
    node->parent = c;
    node->link[0] = NULL;
    node->link[1] = NULL;
    node->balance = 0;
    
    // rebalance
    _BAVL_rebalance(o, c, side, 1);
    
    BAVL_ASSERT(o)
    
    if (ref) {
        *ref = c;
    }
    return 1;
}

void BAVL_Remove (BAVL *o, BAVLNode *node)
{
    ASSERT(!o->in_handler)
    
    // if we have both subtrees, swap the node and the largest node
    // in the left subtree, so we have at most one subtree
    if (node->link[0] && node->link[1]) {
        // find the largest node in the left subtree
        BAVLNode *max = _BAVL_subtree_max(node->link[0]);
        // swap the nodes
        _BAVL_swap_nodes(o, node, max);
    }
    
    // have at most one child now
    ASSERT(!(node->link[0] && node->link[1]))
    
    BAVLNode *parent = node->parent;
    BAVLNode *child = (node->link[0] ? node->link[0] : node->link[1]);
    
    if (parent) {
        // remember on which side node is
        int side = (node == parent->link[1]);
        // replace node with child
        _BAVL_replace_subtree(o, node, child);
        // rebalance
        _BAVL_rebalance(o, parent, side, -1);
    } else {
        // replace node with child
        _BAVL_replace_subtree(o, node, child);
    }
    
    BAVL_ASSERT(o)
}

int BAVL_IsEmpty (BAVL *o)
{
    ASSERT(!o->in_handler)
    
    return (!o->root);
}

BAVLNode * BAVL_Lookup (BAVL *o, void *val)
{
    ASSERT(!o->in_handler)
    
    if (!o->root) {
        return NULL;
    }
    
    BAVLNode *c = o->root;
    while (1) {
        // compare
        int comp = _BAVL_compare_values(o, val, _BAVL_node_value(o, c));
        
        // have we found a node that compares equal?
        if (comp == 0) {
            return c;
        }
        
        int side = (comp == 1);
        
        // have we reached a leaf?
        if (!c->link[side]) {
            return c;
        }
        
        c = c->link[side];
    }
}

BAVLNode * BAVL_LookupExact (BAVL *o, void *val)
{
    ASSERT(!o->in_handler)
    
    if (!o->root) {
        return NULL;
    }
    
    BAVLNode *c = o->root;
    while (1) {
        // compare
        int comp = _BAVL_compare_values(o, val, _BAVL_node_value(o, c));
        
        // have we found a node that compares equal?
        if (comp == 0) {
            return c;
        }
        
        int side = (comp == 1);
        
        // have we reached a leaf?
        if (!c->link[side]) {
            return NULL;
        }
        
        c = c->link[side];
    }
}

#endif
