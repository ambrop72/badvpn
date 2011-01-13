/**
 * @file LinkedList1.h
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
 * Simple doubly linked list.
 */

#ifndef BADVPN_STRUCTURE_LINKEDLIST1_H
#define BADVPN_STRUCTURE_LINKEDLIST1_H

#include <stddef.h>

#include <misc/debug.h>

/**
 * Linked list node.
 */
typedef struct LinkedList1Node_t
{
    struct LinkedList1Node_t *p;
    struct LinkedList1Node_t *n;
} LinkedList1Node;

/**
 * Simple doubly linked list.
 */
typedef struct
{
    LinkedList1Node *first;
    LinkedList1Node *last;
} LinkedList1;

/**
 * Initializes the linked list.
 * 
 * @param list list to initialize
 */
static void LinkedList1_Init (LinkedList1 *list);

/**
 * Determines if the list is empty.
 * 
 * @param list the list
 * @return 1 if empty, 0 if not
 */
static int LinkedList1_IsEmpty (LinkedList1 *list);

/**
 * Returns the first node of the list.
 * 
 * @param list the list
 * @return first node of the list, or NULL if the list is empty
 */
static LinkedList1Node * LinkedList1_GetFirst (LinkedList1 *list);

/**
 * Returns the last node of the list.
 * 
 * @param list the list
 * @return last node of the list, or NULL if the list is empty
 */
static LinkedList1Node * LinkedList1_GetLast (LinkedList1 *list);

/**
 * Inserts a node to the beginning of the list.
 * 
 * @param list the list
 * @param node uninitialized node to insert
 */
static void LinkedList1_Prepend (LinkedList1 *list, LinkedList1Node *node);

/**
 * Inserts a node to the end of the list.
 * 
 * @param list the list
 * @param node uninitialized node to insert
 */
static void LinkedList1_Append (LinkedList1 *list, LinkedList1Node *node);

/**
 * Inserts a node before a given node.
 * 
 * @param list the list
 * @param node uninitialized node to insert
 * @param target node in the list to insert before
 */
static void LinkedList1_InsertBefore (LinkedList1 *list, LinkedList1Node *node, LinkedList1Node *target);

/**
 * Inserts a node after a given node.
 * 
 * @param list the list
 * @param node uninitialized node to insert
 * @param target node in the list to insert after
 */
static void LinkedList1_InsertAfter (LinkedList1 *list, LinkedList1Node *node, LinkedList1Node *target);

/**
 * Removes a node from the list.
 * 
 * @param list the list
 * @param node node to remove
 */
static void LinkedList1_Remove (LinkedList1 *list, LinkedList1Node *node);

/**
 * Returns the next node of a given node.
 * 
 * @param node reference node
 * @return next node, or NULL if none
 */
static LinkedList1Node * LinkedList1Node_Next (LinkedList1Node *node);

/**
 * Returns the previous node of a given node.
 * 
 * @param node reference node
 * @return previous node, or NULL if none
 */
static LinkedList1Node * LinkedList1Node_Prev (LinkedList1Node *node);

void LinkedList1_Init (LinkedList1 *list)
{
    list->first = NULL;
    list->last = NULL;
}

int LinkedList1_IsEmpty (LinkedList1 *list)
{
    return (!list->first);
}

LinkedList1Node * LinkedList1_GetFirst (LinkedList1 *list)
{
    return (list->first);
}

LinkedList1Node * LinkedList1_GetLast (LinkedList1 *list)
{
    return (list->last);
}

void LinkedList1_Prepend (LinkedList1 *list, LinkedList1Node *node)
{
    node->p = NULL;
    node->n = list->first;
    if (list->first) {
        list->first->p = node;
    } else {
        list->last = node;
    }
    list->first = node;
}

void LinkedList1_Append (LinkedList1 *list, LinkedList1Node *node)
{
    node->p = list->last;
    node->n = NULL;
    if (list->last) {
        list->last->n = node;
    } else {
        list->first = node;
    }
    list->last = node;
}

void LinkedList1_InsertBefore (LinkedList1 *list, LinkedList1Node *node, LinkedList1Node *target)
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

void LinkedList1_InsertAfter (LinkedList1 *list, LinkedList1Node *node, LinkedList1Node *target)
{
    node->p = target;
    node->n = target->n;
    if (target->n) {
        target->n->p = node;
    } else {
        list->last = node;
    }
    target->n = node;
}

void LinkedList1_Remove (LinkedList1 *list, LinkedList1Node *node)
{
    // remove from list
    if (node->p) {
        node->p->n = node->n;
    } else {
        list->first = node->n;
    }
    if (node->n) {
        node->n->p = node->p;
    } else {
        list->last = node->p;
    }
}

LinkedList1Node * LinkedList1Node_Next (LinkedList1Node *node)
{
    return node->n;
}

LinkedList1Node * LinkedList1Node_Prev (LinkedList1Node *node)
{
    return node->p;
}

#endif
