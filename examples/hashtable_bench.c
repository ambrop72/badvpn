/**
 * @file hashtable_bench.c
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

#include <misc/offset.h>
#include <misc/brandom.h>
#include <misc/debug.h>
#include <misc/jenkins_hash.h>
#include <structure/HashTable.h>

struct mynode {
    int used;
    int num;
    HashTableNode hash_node;
};

static int key_comparator (int *key1, int *key2)
{
    return (*key1 == *key2);
}

static int hash_function (int *key, int modulo)
{
    return (jenkins_one_at_a_time_hash((uint8_t *)key, sizeof(int)) % modulo);
}

static void print_indent (int indent)
{
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

int main (int argc, char **argv)
{
    int num_nodes;
    int num_random_delete;
    
    if (argc != 3 || (num_nodes = atoi(argv[1])) <= 0 || (num_random_delete = atoi(argv[2])) < 0) {
        fprintf(stderr, "Usage: %s <num> <numrandomdelete>\n", (argc > 0 ? argv[0] : NULL));
        return 1;
    }
    
    struct mynode *nodes = malloc(num_nodes * sizeof(*nodes));
    if (!nodes) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }
    
    int *values_ins = malloc(num_nodes * sizeof(int));
    ASSERT_FORCE(values_ins)
    
    int *values = malloc(num_random_delete * sizeof(int));
    ASSERT_FORCE(values)
    
    HashTable ht;
    if (!HashTable_Init(
        &ht,
        OFFSET_DIFF(struct mynode, num, hash_node),
        (HashTable_comparator)key_comparator,
        (HashTable_hash_function)hash_function,
        num_nodes * 2
    )) {
        fprintf(stderr, "HashTable_Init failed\n");
        return 1;
    }
    
    printf("Inserting random values...\n");
    brandom_randomize((uint8_t *)values_ins, num_nodes * sizeof(int));
    for (int i = 0; i < num_nodes; i++) {
        nodes[i].num = values_ins[i];
        if (HashTable_Insert(&ht, &nodes[i].hash_node)) {
            nodes[i].used = 1;
        } else {
            nodes[i].used = 0;
            printf("Insert collision!\n");
        }
    }
    
    printf("Removing random entries...\n");
    int removed = 0;
    brandom_randomize((uint8_t *)values, num_random_delete * sizeof(int));
    for (int i = 0; i < num_random_delete; i++) {
        int index = (((unsigned int *)values)[i] % num_nodes);
        struct mynode *node = nodes + index;
        if (node->used) {
            ASSERT_EXECUTE(HashTable_Remove(&ht, &node->num))
            node->used = 0;
            removed++;
        }
    }
    
    printf("Removed %d entries\n", removed);
    
    free(nodes);
    free(values);
    
    return 0;
}
