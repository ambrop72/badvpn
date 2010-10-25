/**
 * @file BPredicate_internal.h
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
 * {@link BPredicate} expression tree definitions and functions.
 */

#ifndef BADVPN_PREDICATE_BPREDICATE_INTERNAL_H
#define BADVPN_PREDICATE_BPREDICATE_INTERNAL_H

#include <misc/debug.h>

#define NODE_CONSTANT 0
#define NODE_NEG 2
#define NODE_CONJUNCT 3
#define NODE_DISJUNCT 4
#define NODE_FUNCTION 5

struct arguments_node;

struct predicate_node {
    int type;
    union {
        struct {
            int val;
        } constant;
        struct {
            struct predicate_node *op;
        } neg;
        struct {
            struct predicate_node *op1;
            struct predicate_node *op2;
        } conjunct;
        struct {
            struct predicate_node *op1;
            struct predicate_node *op2;
        } disjunct;
        struct {
            char *name;
            struct arguments_node *args;
        } function;
    };
    int eval_value;
};

#define ARGUMENT_INVALID 0
#define ARGUMENT_PREDICATE 1
#define ARGUMENT_STRING 2

struct arguments_arg {
    int type;
    union {
        struct predicate_node *predicate;
        char *string;
    };
};

struct arguments_node {
    struct arguments_arg arg;
    struct arguments_node *next;
};

static void free_predicate_node (struct predicate_node *root);
static void free_argument (struct arguments_arg arg);
static void free_arguments_node (struct arguments_node *n);

void free_predicate_node (struct predicate_node *root)
{
    ASSERT(root)
    
    switch (root->type) {
        case NODE_CONSTANT:
            break;
        case NODE_NEG:
            free_predicate_node(root->neg.op);
            break;
        case NODE_CONJUNCT:
            free_predicate_node(root->conjunct.op1);
            free_predicate_node(root->conjunct.op2);
            break;
        case NODE_DISJUNCT:
            free_predicate_node(root->disjunct.op1);
            free_predicate_node(root->disjunct.op2);
            break;
        case NODE_FUNCTION:
            free(root->function.name);
            if (root->function.args) {
                free_arguments_node(root->function.args);
            }
            break;
        default:
            ASSERT(0)
            break;
    }
    
    free(root);
}

void free_argument (struct arguments_arg arg)
{
    switch (arg.type) {
        case ARGUMENT_INVALID:
            break;
        case ARGUMENT_PREDICATE:
            free_predicate_node(arg.predicate);
            break;
        case ARGUMENT_STRING:
            free(arg.string);
            break;
        default:
            ASSERT(0);
    }
}

void free_arguments_node (struct arguments_node *n)
{
    ASSERT(n)
    
    free_argument(n->arg);
    
    if (n->next) {
        free_arguments_node(n->next);
    }
    
    free(n);
}

#endif
