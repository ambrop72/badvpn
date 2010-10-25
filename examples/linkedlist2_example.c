/**
 * @file linkedlist2_example.c
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
