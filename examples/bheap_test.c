/**
 * @file bheap_test.c
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
 */

#include <misc/offset.h>
#include <misc/balloc.h>
#include <misc/compare.h>
#include <structure/BHeap.h>
#include <security/BRandom.h>

struct mynode {
    int used;
    int num;
    BHeapNode heap_node;
};

static int int_comparator (void *user, int *val1, int *val2)
{
    return B_COMPARE(*val1, *val2);
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
    
    struct mynode *nodes = (struct mynode *)BAllocArray(num_nodes, sizeof(*nodes));
    ASSERT_FORCE(nodes)
    
    int *values = (int *)BAllocArray(num_random_delete, sizeof(int));
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
    BRandom_randomize((uint8_t *)values, num_random_delete * sizeof(int));
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
    
    BFree(values);
    BFree(nodes);
    
    return 0;
}
