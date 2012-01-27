/**
 * @file linkedlist2_example.c
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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include <structure/LinkedList2.h>

struct elem
{
    int i;
    LinkedList2Node list_node;
};

void printnode (LinkedList2Node *node)
{
    if (!node) {
        printf("(null) ");
    } else {
        struct elem *e = (struct elem *)((uint8_t *)node-offsetof(struct elem, list_node));
        printf("%d ", e->i);
    }
}

void printall (LinkedList2 *list)
{
    printf("List: ");
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, list);
    LinkedList2Node *node;
    while (node = LinkedList2Iterator_Next(&it)) {
        printnode(node);
    }
    printf("\n");
}

void removeall (LinkedList2 *list)
{
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, list);
    LinkedList2Node *node;
    while (node = LinkedList2Iterator_Next(&it)) {
        LinkedList2_Remove(list, node);
    }
}

int main ()
{
    struct elem elems[10];

    LinkedList2 list;
    LinkedList2_Init(&list);

    int i;
    for (i=0; i<10; i++) {
        elems[i].i = i;
        LinkedList2_Append(&list, &elems[i].list_node);
    }

    printall(&list);
    
    LinkedList2Iterator it1;
    LinkedList2Iterator it2;
    LinkedList2Iterator it3;
    LinkedList2Iterator it4;
    
    LinkedList2Iterator_InitForward(&it1, &list);
    LinkedList2Iterator_InitForward(&it2, &list);
    LinkedList2Iterator_InitBackward(&it3, &list);
    LinkedList2Iterator_InitBackward(&it4, &list);
    
    LinkedList2_Remove(&list, &elems[0].list_node);
    LinkedList2_Remove(&list, &elems[1].list_node);
    LinkedList2_Remove(&list, &elems[2].list_node);
    LinkedList2_Remove(&list, &elems[3].list_node);
    
    LinkedList2_Remove(&list, &elems[9].list_node);
    LinkedList2_Remove(&list, &elems[8].list_node);
    LinkedList2_Remove(&list, &elems[7].list_node);
    LinkedList2_Remove(&list, &elems[6].list_node);
    
    LinkedList2Node *node1;
    LinkedList2Node *node2;
    LinkedList2Node *node3;
    LinkedList2Node *node4;
    
    node1 = LinkedList2Iterator_Next(&it1);
    node2 = LinkedList2Iterator_Next(&it2);
    printnode(node1);
    printnode(node2);
    printf("\n");
    
    node3 = LinkedList2Iterator_Next(&it3);
    node4 = LinkedList2Iterator_Next(&it4);
    printnode(node3);
    printnode(node4);
    printf("\n");
    
    printall(&list);
    
    node1 = LinkedList2Iterator_Next(&it1);
    printnode(node1);
    printf("\n");
    
    node3 = LinkedList2Iterator_Next(&it3);
    printnode(node3);
    printf("\n");
    
    printall(&list);
    
    LinkedList2_Prepend(&list, &elems[3].list_node);
    LinkedList2_Append(&list, &elems[6].list_node);
    
    printall(&list);
    
    node1 = LinkedList2Iterator_Next(&it1);
    node2 = LinkedList2Iterator_Next(&it2);
    printnode(node1);
    printnode(node2);
    printf("\n");
    
    node3 = LinkedList2Iterator_Next(&it3);
    node4 = LinkedList2Iterator_Next(&it4);
    printnode(node3);
    printnode(node4);
    printf("\n");
    
    node1 = LinkedList2Iterator_Next(&it1);
    node2 = LinkedList2Iterator_Next(&it2);
    printnode(node1);
    printnode(node2);
    printf("\n");
    
    node3 = LinkedList2Iterator_Next(&it3);
    node4 = LinkedList2Iterator_Next(&it4);
    printnode(node3);
    printnode(node4);
    printf("\n");
    
    return 0;
}
