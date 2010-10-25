/**
 * @file hashtable_example.c
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

#include <stddef.h>

#include <misc/debug.h>
#include <misc/jenkins_hash.h>
#include <structure/HashTable.h>

int key_comparator (int *key1, int *key2)
{
    return (*key1 == *key2);
}

int hash_function (int *key, int modulo)
{
    return (jenkins_one_at_a_time_hash((uint8_t *)key, sizeof(int)) % modulo);
}

struct entry {
    HashTableNode node;
    int value;
};

int main ()
{
    HashTable table;
    if (HashTable_Init(
        &table,
        (offsetof(struct entry, value) - offsetof(struct entry, node)),
        (HashTable_comparator)key_comparator,
        (HashTable_hash_function)hash_function,
        20
    ) != 1) {
        return 1;
    }
    
    struct entry entries[10];
    
    // insert entries
    int i;
    for (i=0; i<10; i++) {
        struct entry *entry = &entries[i];
        // must initialize value before inserting
        entry->value = i;
        // insert
        int res = HashTable_Insert(&table, &entry->node);
        ASSERT(res == 1)
    }
    
    // lookup entries
    for (i=0; i<10; i++) {
        HashTableNode *node;
        int res = HashTable_Lookup(&table, &i, &node);
        ASSERT(res == 1)
        struct entry *entry = (struct entry *)((uint8_t *)node - offsetof(struct entry, node));
        ASSERT(entry == &entries[i])
    }
    
    // remove entries
    for (i=0; i<10; i++) {
        int res = HashTable_Remove(&table, &i);
        ASSERT(res == 1)
    }
    
    // remove entries again
    for (i=0; i<10; i++) {
        int res = HashTable_Remove(&table, &i);
        ASSERT(res == 0)
    }
    
    HashTable_Free(&table);
    
    return 0;
}
