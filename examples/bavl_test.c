/**
 * @file bavl_test.c
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
#include <misc/debug.h>
#include <misc/balloc.h>
#include <structure/BAVL.h>
#include <security/BRandom.h>

struct mynode {
    int used;
    int num;
    BAVLNode avl_node;
};

static int int_comparator (void *user, int *val1, int *val2)
{
    if (*val1 < *val2) {
        return -1;
    }
    if (*val1 > *val2) {
        return 1;
    }
    return 0;
}

static void print_indent (int indent)
{
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

static void print_avl_recurser (BAVLNode *node, int indent)
{
    print_indent(indent);
    
    if (!node) {
        printf("null\n");
    } else {
        struct mynode *mnode = UPPER_OBJECT(node, struct mynode, avl_node);
        printf("(%d) %d %p\n", node->balance, mnode->num, node);
        print_avl_recurser(node->link[0], indent + 1);
        print_avl_recurser(node->link[1], indent + 1);
    }
}

static void print_avl (BAVL *tree)
{
    print_avl_recurser(tree->root, 0);
}

int main (int argc, char **argv)
{
    int num_nodes;
    int num_random_delete;
    
    if (argc != 3 || (num_nodes = atoi(argv[1])) <= 0 || (num_random_delete = atoi(argv[2])) < 0) {
        fprintf(stderr, "Usage: %s <num> <numrandomdelete>\n", (argc > 0 ? argv[0] : NULL));
        return 1;
    }
    
    struct mynode *nodes = BAllocArray(num_nodes, sizeof(*nodes));
    ASSERT_FORCE(nodes)
    
    int *values_ins = BAllocArray(num_nodes, sizeof(int));
    ASSERT_FORCE(values_ins)
    
    int *values = BAllocArray(num_random_delete, sizeof(int));
    ASSERT_FORCE(values)
    
    BAVL avl;
    BAVL_Init(&avl, OFFSET_DIFF(struct mynode, num, avl_node), (BAVL_comparator)int_comparator, NULL);
    
    /*
    printf("Inserting in reverse order...\n");
    for (int i = num_nodes - 1; i >= 0; i--) {
        nodes[i].used = 1;
        nodes[i].num = i;
        int res = BAVL_Insert(&avl, &nodes[i].avl_node);
        ASSERT(res == 1)
    }
    */
    
    printf("Inserting random values...\n");
    BRandom_randomize((uint8_t *)values_ins, num_nodes * sizeof(int));
    for (int i = 0; i < num_nodes; i++) {
        nodes[i].num = values_ins[i];
        if (BAVL_Insert(&avl, &nodes[i].avl_node, NULL)) {
            nodes[i].used = 1;
        } else {
            nodes[i].used = 0;
            printf("Insert collision!\n");
        }
    }
    
    printf("Removing random entries...\n");
    int removed = 0;
    BRandom_randomize((uint8_t *)values, num_random_delete * sizeof(int));
    for (int i = 0; i < num_random_delete; i++) {
        int index = (((unsigned int *)values)[i] % num_nodes);
        struct mynode *node = nodes + index;
        if (node->used) {
            BAVL_Remove(&avl, &node->avl_node);
            node->used = 0;
            removed++;
        }
    }
    
    printf("Removed %d entries\n", removed);
    
    BFree(nodes);
    BFree(values_ins);
    BFree(values);
    
    return 0;
}
