/**
 * @file LinkedList0.h
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
 * Very simple doubly linked list, with only a 'first' pointer an no 'last'
 * pointer.
 */

#ifndef BADVPN_STRUCTURE_LINKEDLIST0_H
#define BADVPN_STRUCTURE_LINKEDLIST0_H

#include <stddef.h>

#include <misc/debug.h>

/**
 * Linked list node.
 */
typedef struct LinkedList0Node_t
{
    struct LinkedList0Node_t *p;
    struct LinkedList0Node_t *n;
} LinkedList0Node;

/**
 * Simple doubly linked list.
 */
typedef struct
{
    LinkedList0Node *first;
} LinkedList0;

/**
 * Initializes the linked list.
 * 
 * @param list list to initialize
 */
static void LinkedList0_Init (LinkedList0 *list);

/**
 * Determines if the list is empty.
 * 
 * @param list the list
 * @return 1 if empty, 0 if not
 */
static int LinkedList0_IsEmpty (LinkedList0 *list);

/**
 * Returns the first node of the list.
 * 
 * @param list the list
 * @return first node of the list, or NULL if the list is empty
 */
static LinkedList0Node * LinkedList0_GetFirst (LinkedList0 *list);

/**
 * Inserts a node to the beginning of the list.
 * 
 * @param list the list
 * @param node uninitialized node to insert
 */
static void LinkedList0_Prepend (LinkedList0 *list, LinkedList0Node *node);

/**
 * Inserts a node before a given node.
 * 
 * @param list the list
 * @param node uninitialized node to insert
 * @param target node in the list to insert before
 */
static void LinkedList0_InsertBefore (LinkedList0 *list, LinkedList0Node *node, LinkedList0Node *target);

/**
 * Inserts a node after a given node.
 * 
 * @param list the list
 * @param node uninitialized node to insert
 * @param target node in the list to insert after
 */
static void LinkedList0_InsertAfter (LinkedList0 *list, LinkedList0Node *node, LinkedList0Node *target);

/**
 * Removes a node from the list.
 * 
 * @param list the list
 * @param node node to remove
 */
static void LinkedList0_Remove (LinkedList0 *list, LinkedList0Node *node);

/**
 * Returns the next node of a given node.
 * 
 * @param node reference node
 * @return next node, or NULL if none
 */
static LinkedList0Node * LinkedList0Node_Next (LinkedList0Node *node);

/**
 * Returns the previous node of a given node.
 * 
 * @param node reference node
 * @return previous node, or NULL if none
 */
static LinkedList0Node * LinkedList0Node_Prev (LinkedList0Node *node);

void LinkedList0_Init (LinkedList0 *list)
{
    list->first = NULL;
}

int LinkedList0_IsEmpty (LinkedList0 *list)
{
    return (!list->first);
}

LinkedList0Node * LinkedList0_GetFirst (LinkedList0 *list)
{
    return (list->first);
}

void LinkedList0_Prepend (LinkedList0 *list, LinkedList0Node *node)
{
    node->p = NULL;
    node->n = list->first;
    if (list->first) {
        list->first->p = node;
    }
    list->first = node;
}

void LinkedList0_InsertBefore (LinkedList0 *list, LinkedList0Node *node, LinkedList0Node *target)
{
    node->p = target->p;
    node->n = target;
    if (target->p) {
        target->p->n = node;
    } else {
        list->first = node;
    }
    target->p = node;
}

void LinkedList0_InsertAfter (LinkedList0 *list, LinkedList0Node *node, LinkedList0Node *target)
{
    node->p = target;
    node->n = target->n;
    if (target->n) {
        target->n->p = node;
    }
    target->n = node;
}

void LinkedList0_Remove (LinkedList0 *list, LinkedList0Node *node)
{
    // remove from list
    if (node->p) {
        node->p->n = node->n;
    } else {
        list->first = node->n;
    }
    if (node->n) {
        node->n->p = node->p;
    }
}

LinkedList0Node * LinkedList0Node_Next (LinkedList0Node *node)
{
    return node->n;
}

LinkedList0Node * LinkedList0Node_Prev (LinkedList0Node *node)
{
    return node->p;
}

#endif
