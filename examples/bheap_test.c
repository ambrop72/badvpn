/**
 * @file bheap_test.c
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

#include <misc/offset.h>
#include <misc/brandom.h>
#include <structure/BHeap.h>

struct mynode {
    int used;
    int num;
    BHeapNode heap_node;
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

static void print_heap_recurser (BHeapNode *node, int indent)
{
    print_indent(indent);
    
    if (!node) {
        printf("null\n");
    } else {
        struct mynode *mnode = UPPER_OBJECT(node, struct mynode, heap_node);
        printf("%d %p\n", mnode->num, node);
        print_heap_recurser(node->link[0], indent + 1);
        print_heap_recurser(node->link[1], indent + 1);
    }
}

static void print_heap (BHeap *heap)
{
    print_heap_recurser(heap->root, 0);
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
    
    int *values = malloc(num_random_delete * sizeof(int));
    ASSERT_FORCE(values)
    
    BHeap heap;
    BHeap_Init(&heap, OFFSET_DIFF(struct mynode, num, heap_node), (BHeap_comparator)int_comparator, NULL);
    
    printf("Inserting in reverse order...\n");
    for (int i = num_nodes - 1; i >= 0; i--) {
        nodes[i].used = 1;
        nodes[i].num = i;
        BHeap_Insert(&heap, &nodes[i].heap_node);
    }
    
    //print_heap(&heap);
    
    printf("Removing random entries...\n");
    brandom_randomize((uint8_t *)values, num_random_delete * sizeof(int));
    for (int i = 0; i < num_random_delete; i++) {
        int index = (((unsigned int *)values)[i] % num_nodes);
        struct mynode *node = nodes + index;
        if (node->used) {
            //printf("Removing index %d value %d\n", index, node->num);
            BHeap_Remove(&heap, &node->heap_node);
            node->used = 0;
        }
    }
    
    //print_heap(&heap);
    
    printf("Removing remaining entries...\n");
    BHeapNode *heap_node;
    while (heap_node = BHeap_GetFirst(&heap)) {
        struct mynode *node = UPPER_OBJECT(heap_node, struct mynode, heap_node);
        //printf("Removing value %d\n", node->num);
        BHeap_Remove(&heap, &node->heap_node);
        node->used = 0;
    }
    
    //print_heap(&heap);
    
    free(nodes);
    free(values);
    
    return 0;
}
