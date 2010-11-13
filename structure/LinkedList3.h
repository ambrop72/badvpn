/**
 * @file LinkedList3.h
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
 * Doubly linked list that support multiple iterations and removing
 * aritrary elements during iteration, without a central object to
 * represent the list.
 */

#ifndef BADVPN_STRUCTURE_LINKEDLIST3_H
#define BADVPN_STRUCTURE_LINKEDLIST3_H

#include <stddef.h>

#include <misc/debug.h>

struct _LinkedList3Iterator;

typedef struct _LinkedList3Node {
    struct _LinkedList3Node *p;
    struct _LinkedList3Node *n;
    struct _LinkedList3Iterator *it;
} LinkedList3Node;

typedef struct _LinkedList3Iterator {
    int dir;
    struct _LinkedList3Node *e;
    struct _LinkedList3Iterator *pi;
    struct _LinkedList3Iterator *ni;
} LinkedList3Iterator;

static void LinkedList3Node_InitLonely (LinkedList3Node *node);
static void LinkedList3Node_InitAfter (LinkedList3Node *node, LinkedList3Node *ref);
static void LinkedList3Node_InitBefore (LinkedList3Node *node, LinkedList3Node *ref);
static void LinkedList3Node_Free (LinkedList3Node *node);
static int LinkedList3Node_IsLonely (LinkedList3Node *node);
static LinkedList3Node * LinkedList3Node_PrevOrNext (LinkedList3Node *node);
static LinkedList3Node * LinkedList3Node_NextOrPrev (LinkedList3Node *node);
static LinkedList3Node * LinkedList3Node_Prev (LinkedList3Node *node);
static LinkedList3Node * LinkedList3Node_Next (LinkedList3Node *node);
static LinkedList3Node * LinkedList3Node_First (LinkedList3Node *node);
static LinkedList3Node * LinkedList3Node_Last (LinkedList3Node *node);

static void LinkedList3Iterator_Init (LinkedList3Iterator *it, LinkedList3Node *e, int dir);
static void LinkedList3Iterator_Free (LinkedList3Iterator *it);
static LinkedList3Node * LinkedList3Iterator_Next (LinkedList3Iterator *it);

void LinkedList3Node_InitLonely (LinkedList3Node *node)
{
    node->p = NULL;
    node->n = NULL;
    node->it = NULL;
}

void LinkedList3Node_InitAfter (LinkedList3Node *node, LinkedList3Node *ref)
{
    ASSERT(ref)
    
    node->p = ref;
    node->n = ref->n;
    ref->n = node;
    if (node->n) {
        node->n->p = node;
    }
    node->it = NULL;
}

void LinkedList3Node_InitBefore (LinkedList3Node *node, LinkedList3Node *ref)
{
    ASSERT(ref)
    
    node->n = ref;
    node->p = ref->p;
    ref->p = node;
    if (node->p) {
        node->p->n = node;
    }
    node->it = NULL;
}

void LinkedList3Node_Free (LinkedList3Node *node)
{
    // jump iterators
    while (node->it) {
        LinkedList3Iterator_Next(node->it);
    }
    
    if (node->p) {
        node->p->n = node->n;
    }
    if (node->n) {
        node->n->p = node->p;
    }
}

int LinkedList3Node_IsLonely (LinkedList3Node *node)
{
    return (!node->p && !node->n);
}

LinkedList3Node * LinkedList3Node_PrevOrNext (LinkedList3Node *node)
{
    if (node->p) {
        return node->p;
    }
    if (node->n) {
        return node->n;
    }
    return NULL;
}

LinkedList3Node * LinkedList3Node_NextOrPrev (LinkedList3Node *node)
{
    if (node->n) {
        return node->n;
    }
    if (node->p) {
        return node->p;
    }
    return NULL;
}

LinkedList3Node * LinkedList3Node_Prev (LinkedList3Node *node)
{
    return node->p;
}

LinkedList3Node * LinkedList3Node_Next (LinkedList3Node *node)
{
    return node->n;
}

LinkedList3Node * LinkedList3Node_First (LinkedList3Node *node)
{
    while (node->p) {
        node = node->p;
    }
    
    return node;
}

LinkedList3Node * LinkedList3Node_Last (LinkedList3Node *node)
{
    while (node->n) {
        node = node->n;
    }
    
    return node;
}

void LinkedList3Iterator_Init (LinkedList3Iterator *it, LinkedList3Node *e, int dir)
{
    ASSERT(dir == -1 || dir == 1)
    
    it->dir = dir;
    it->e = e;
    
    if (e) {
        // link into node's iterator list
        it->pi = NULL;
        it->ni = e->it;
        if (e->it) {
            e->it->pi = it;
        }
        e->it = it;
    }
}

void LinkedList3Iterator_Free (LinkedList3Iterator *it)
{
    if (it->e) {
        // remove from node's iterator list
        if (it->ni) {
            it->ni->pi = it->pi;
        }
        if (it->pi) {
            it->pi->ni = it->ni;
        } else {
            it->e->it = it->ni;
        }
    }
}

LinkedList3Node * LinkedList3Iterator_Next (LinkedList3Iterator *it)
{
    // remember original entry
    LinkedList3Node *orig = it->e;
    
    // jump to next entry
    if (it->e) {
        // get next entry
        LinkedList3Node *next;
        switch (it->dir) {
            case 1:
                next = it->e->n;
                break;
            case -1:
                next = it->e->p;
                break;
            default:
                ASSERT(0);
        }
        // destroy interator
        LinkedList3Iterator_Free(it);
        // re-initialize at next entry
        LinkedList3Iterator_Init(it, next, it->dir);
    }
    
    // return original entry
    return orig;
}

#endif
