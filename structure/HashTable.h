/**
 * @file HashTable.h
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
 * Hash table using separate chaining.
 */

#ifndef BADVPN_STRUCTURE_HASHTABLE_H
#define BADVPN_STRUCTURE_HASHTABLE_H

#include <stdint.h>
#include <stddef.h>

#include <misc/debug.h>
#include <system/DebugObject.h>

/**
 * Handler function used to compare values.
 * For any two values, the comparator must always return the same result.
 * Values are obtained like that:
 *   - The value of a node in the table, or a node that is being inserted is:
 *     (uint8_t *)node + offset.
 *   - The value being looked up is the same as given to the lookup function.
 *
 * @param val1 first value
 * @param val2 second value
 * @return 1 if values are equal, 0 if not
 */
typedef int (*HashTable_comparator) (void *val1, void *val2);

/**
 * Handler function used to compute the hash of a value, modulo some number.
 * The value is obtained the same way as in {@link HashTable_comparator}.
 *
 * @param val value whose hash function is to be computed
 * @param modulo an integer modulo which the hash must be taken before returning it
 * @return hash of value modulo parameter
 */
typedef int (*HashTable_hash_function) (void *val, int modulo);

struct HashTableNode_t;

/**
 * Hash table using separate chaining.
 */
typedef struct {
    DebugObject d_obj;
    int offset;
    HashTable_comparator comparator;
    HashTable_hash_function hash_function;
    int num_buckets;
    struct HashTableNode_t **buckets;
    #ifndef NDEBUG
    int in_handler;
    #endif
} HashTable;

/**
 * Hash table node.
 */
typedef struct HashTableNode_t {
    struct HashTableNode_t *next;
} HashTableNode;

/**
 * Initializes the hash table.
 *
 * @param t the object
 * @param offset offset of a value from its node
 * @param comparator value comparator handler function
 * @param hash_function value hash function handler function
 * @param size number of buckets in the hash table. Must be >0.
 * @return 1 on success, 0 on failure
 */
static int HashTable_Init (HashTable *t, int offset, HashTable_comparator comparator, HashTable_hash_function hash_function, int size) WARN_UNUSED;

/**
 * Frees the hash table.
 * Must not be called from handler functions.
 *
 * @param t the object
 */
static void HashTable_Free (HashTable *t);

/**
 * Inserts a node into the hash table.
 * Must not be called from handler functions.
 *
 * @param t the object
 * @param node uninitialized node to insert. Must have a valid value (its value
 *             may be passed to the comparator or hash function during insertion).
 * @return 1 on success, 0 if an element with an equal value is already in the table
 */
static int HashTable_Insert (HashTable *t, HashTableNode *node);

/**
 * Removes a node from the table by value.
 * The node must be in the hash table.
 * Must not be called from handler functions.
 *
 * @param t the object
 * @param val value of the node to be removed.
 * @return 1 on success, 0 if there is no node with the given value
 */
static int HashTable_Remove (HashTable *t, void *val);

/**
 * Looks up the node of the value given.
 * Must not be called from handler functions.
 *
 * @param t the object
 * @param val value to look up
 * @param node if not NULL, will be set to the node pointer on success
 * @return 1 on success, 0 if there is no node with the given value
 */
static int HashTable_Lookup (HashTable *t, void *val, HashTableNode **node);

static int _HashTable_compare_values (HashTable *t, void *v1, void *v2)
{
    #ifndef NDEBUG
    t->in_handler = 1;
    #endif
    
    int res = t->comparator(v1, v2);
    
    #ifndef NDEBUG
    t->in_handler = 0;
    #endif
    
    ASSERT(res == 0 || res == 1)
    
    return res;
}

static int _HashTable_compute_hash (HashTable *t, void *v)
{
    #ifndef NDEBUG
    t->in_handler = 1;
    #endif
    
    int res = t->hash_function(v, t->num_buckets);
    
    #ifndef NDEBUG
    t->in_handler = 0;
    #endif
    
    ASSERT(res >= 0)
    ASSERT(res < t->num_buckets)
    
    return res;
}

int HashTable_Init (HashTable *t, int offset, HashTable_comparator comparator, HashTable_hash_function hash_function, int size)
{
    ASSERT(size > 0)
    
    // init arguments
    t->offset = offset;
    t->comparator = comparator;
    t->hash_function = hash_function;
    t->num_buckets = size;
    
    // allocate buckets
    t->buckets = (HashTableNode **)malloc(t->num_buckets * sizeof(HashTableNode *));
    if (!t->buckets) {
        return 0;
    }
    
    // zero buckets
    int i;
    for (i = 0; i < t->num_buckets; i++) {
        t->buckets[i] = NULL;
    }
    
    // init debugging
    #ifndef NDEBUG
    t->in_handler = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&t->d_obj);
    
    return 1;
}

void HashTable_Free (HashTable *t)
{
    // free debug object
    DebugObject_Free(&t->d_obj);
    
    ASSERT(!t->in_handler)
    
    // free buckets
    free(t->buckets);
}

int HashTable_Insert (HashTable *t, HashTableNode *node)
{
    ASSERT(!t->in_handler)
    
    // obtain value
    void *val = (uint8_t *)node + t->offset;
    
    // obtain bucket index
    int index = _HashTable_compute_hash(t, val);
    
    // look for existing entries with an equal value
    HashTableNode *cur = t->buckets[index];
    while (cur) {
        void *cur_val = (uint8_t *)cur + t->offset;
        if (_HashTable_compare_values(t, cur_val, val)) {
            return 0;
        }
        cur = cur->next;
    }
    
    // prepend to linked list
    node->next = t->buckets[index];
    t->buckets[index] = node;
    
    return 1;
}

int HashTable_Remove (HashTable *t, void *val)
{
    ASSERT(!t->in_handler)
    
    // obtain bucket index
    int index = _HashTable_compute_hash(t, val);
    
    // find node with an equal value
    HashTableNode *prev = NULL;
    HashTableNode *cur = t->buckets[index];
    while (cur) {
        void *cur_val = (uint8_t *)cur + t->offset;
        if (_HashTable_compare_values(t, cur_val, val)) {
            // remove node from lined list
            if (prev) {
                prev->next = cur->next;
            } else {
                t->buckets[index] = cur->next;
            }
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    
    return 0;
}

int HashTable_Lookup (HashTable *t, void *val, HashTableNode **node)
{
    ASSERT(!t->in_handler)
    
    int index = _HashTable_compute_hash(t, val);
    
    // find node with an equal value
    HashTableNode *cur = t->buckets[index];
    while (cur) {
        void *cur_val = (uint8_t *)cur + t->offset;
        if (_HashTable_compare_values(t, cur_val, val)) {
            if (node) {
                *node = cur;
            }
            return 1;
        }
        cur = cur->next;
    }
    
    return 0;
}

#endif
