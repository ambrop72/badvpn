/**
 * @file LinkedList4.h
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
 * Doubly linked list without a central object to represent the list.
 * Like {@link LinkedList3}, but without iterators.
 */

#ifndef BADVPN_STRUCTURE_LINKEDLIST4_H
#define BADVPN_STRUCTURE_LINKEDLIST4_H

#include <stddef.h>
#include <stdint.h>

#include <misc/debug.h>

/**
 * Linked list node.
 */
typedef struct _LinkedList4Node {
    struct _LinkedList4Node *p;
    struct _LinkedList4Node *n;
} LinkedList4Node;

/**
 * Initializes a list node to form a new list consisting of a
 * single node.
 * 
 * @param node list node structure to initialize. The node must remain
 *        available until it is freed with {@link LinkedList4Node_Free},
 *        or the list is no longer required.
 */
static void LinkedList4Node_InitLonely (LinkedList4Node *node);

/**
 * Initializes a list node to go after an existing node.
 * 
 * @param node list node structure to initialize. The node must remain
 *        available until it is freed with {@link LinkedList4Node_Free},
 *        or the list is no longer required.
 * @param ref existing list node
 */
static void LinkedList4Node_InitAfter (LinkedList4Node *node, LinkedList4Node *ref);

/**
 * Initializes a list node to go before an existing node.
 * 
 * @param node list node structure to initialize. The node must remain
 *        available until it is freed with {@link LinkedList4Node_Free},
 *        or the list is no longer required.
 * @param ref existing list node
 */
static void LinkedList4Node_InitBefore (LinkedList4Node *node, LinkedList4Node *ref);

/**
 * Frees a list node, removing it a list (if there were other nodes
 * in the list).
 * 
 * @param node list node to free
 */
static void LinkedList4Node_Free (LinkedList4Node *node);

/**
 * Determines if a list node is a single node in a list.
 * 
 * @param node list node
 * @return 1 if the node ia a single node, 0 if not
 */
static int LinkedList4Node_IsLonely (LinkedList4Node *node);

/**
 * Returnes the node preceding this node (if there is one),
 * the node following this node (if there is one), or NULL,
 * respectively.
 * 
 * @param node list node
 * @return neighbour node or NULL if none
 */
static LinkedList4Node * LinkedList4Node_PrevOrNext (LinkedList4Node *node);

/**
 * Returnes the node following this node (if there is one),
 * the node preceding this node (if there is one), or NULL,
 * respectively.
 * 
 * @param node list node
 * @return neighbour node or NULL if none
 */
static LinkedList4Node * LinkedList4Node_NextOrPrev (LinkedList4Node *node);

/**
 * Returns the node preceding this node, or NULL if there is none.
 * 
 * @param node list node
 * @return left neighbour, or NULL if none
 */
static LinkedList4Node * LinkedList4Node_Prev (LinkedList4Node *node);

/**
 * Returns the node following this node, or NULL if there is none.
 * 
 * @param node list node
 * @return right neighbour, or NULL if none
 */
static LinkedList4Node * LinkedList4Node_Next (LinkedList4Node *node);

/**
 * Returns the first node in the list which this node is part of.
 * It is found by iterating the list from this node to the beginning.
 * 
 * @param node list node
 * @return first node in the list
 */
static LinkedList4Node * LinkedList4Node_First (LinkedList4Node *node);

/**
 * Returns the last node in the list which this node is part of.
 * It is found by iterating the list from this node to the end.
 * 
 * @param node list node
 * @return last node in the list
 */
static LinkedList4Node * LinkedList4Node_Last (LinkedList4Node *node);

void LinkedList4Node_InitLonely (LinkedList4Node *node)
{
    node->p = NULL;
    node->n = NULL;
}

void LinkedList4Node_InitAfter (LinkedList4Node *node, LinkedList4Node *ref)
{
    ASSERT(ref)
    
    node->p = ref;
    node->n = ref->n;
    ref->n = node;
    if (node->n) {
        node->n->p = node;
    }
}

void LinkedList4Node_InitBefore (LinkedList4Node *node, LinkedList4Node *ref)
{
    ASSERT(ref)
    
    node->n = ref;
    node->p = ref->p;
    ref->p = node;
    if (node->p) {
        node->p->n = node;
    }
}

void LinkedList4Node_Free (LinkedList4Node *node)
{
    if (node->p) {
        node->p->n = node->n;
    }
    if (node->n) {
        node->n->p = node->p;
    }
}

int LinkedList4Node_IsLonely (LinkedList4Node *node)
{
    return (!node->p && !node->n);
}

LinkedList4Node * LinkedList4Node_PrevOrNext (LinkedList4Node *node)
{
    if (node->p) {
        return node->p;
    }
    if (node->n) {
        return node->n;
    }
    return NULL;
}

LinkedList4Node * LinkedList4Node_NextOrPrev (LinkedList4Node *node)
{
    if (node->n) {
        return node->n;
    }
    if (node->p) {
        return node->p;
    }
    return NULL;
}

LinkedList4Node * LinkedList4Node_Prev (LinkedList4Node *node)
{
    return node->p;
}

LinkedList4Node * LinkedList4Node_Next (LinkedList4Node *node)
{
    return node->n;
}

LinkedList4Node * LinkedList4Node_First (LinkedList4Node *node)
{
    while (node->p) {
        node = node->p;
    }
    
    return node;
}

LinkedList4Node * LinkedList4Node_Last (LinkedList4Node *node)
{
    while (node->n) {
        node = node->n;
    }
    
    return node;
}

#endif
