/**
 * @file BPredicate.c
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
#include <stdio.h>
#include <string.h>

#include <misc/debug.h>
#include <misc/offset.h>
#include <predicate/BPredicate_internal.h>
#include <predicate/BPredicate_parser.h>
#include <predicate/LexMemoryBufferInput.h>
#include <system/BLog.h>

#include <predicate/BPredicate.h>

#include <generated/blog_channel_BPredicate.h>

static int eval_predicate_node (BPredicate *p, struct predicate_node *root);

void yyerror (YYLTYPE *yylloc, yyscan_t scanner, struct predicate_node **result, char *str)
{
}

static int string_comparator (void *user, char *s1, char *s2)
{
    int cmp = strcmp(s1, s2);
    if (cmp < 0) {
        return -1;
    }
    if (cmp > 0) {
        return 1;
    }
    return 0;
}

static int eval_function (BPredicate *p, struct predicate_node *root)
{
    ASSERT(root->type == NODE_FUNCTION)
    
    // lookup function by name
    ASSERT(root->function.name)
    BAVLNode *tree_node;
    if (!(tree_node = BAVL_LookupExact(&p->functions_tree, root->function.name))) {
        BLog(BLOG_WARNING, "unknown function");
        return 0;
    }
    BPredicateFunction *func = UPPER_OBJECT(tree_node, BPredicateFunction, tree_node);
    
    // evaluate arguments
    struct arguments_node *arg = root->function.args;
    void *args[func->num_args];
    for (int i = 0; i < func->num_args; i++) {
        if (!arg) {
            BLog(BLOG_WARNING, "not enough arguments");
            return 0;
        }
        switch (func->args[i]) {
            case PREDICATE_TYPE_BOOL:
                if (arg->arg.type != ARGUMENT_PREDICATE) {
                    BLog(BLOG_WARNING, "expecting predicate argument");
                    return 0;
                }
                if (!eval_predicate_node(p, arg->arg.predicate)) {
                    return 0;
                }
                args[i] = &arg->arg.predicate->eval_value;
                break;
            case PREDICATE_TYPE_STRING:
                if (arg->arg.type != ARGUMENT_STRING) {
                    BLog(BLOG_WARNING, "expecting string argument");
                    return 0;
                }
                args[i] = arg->arg.string;
                break;
            default:
                ASSERT(0);
        }
        arg = arg->next;
    }
    
    if (arg) {
        BLog(BLOG_WARNING, "too many arguments");
        return 0;
    }
    
    // call callback
    #ifndef NDEBUG
    p->in_function = 1;
    #endif
    int res = func->callback(func->user, args);
    #ifndef NDEBUG
    p->in_function = 0;
    #endif
    if (res != 0 && res != 1) {
        BLog(BLOG_WARNING, "callback returned non-boolean");
        return 0;
    }
    
    root->eval_value = res;
    return 1;
}

int eval_predicate_node (BPredicate *p, struct predicate_node *root)
{
    ASSERT(root)
    
    switch (root->type) {
        case NODE_CONSTANT:
            root->eval_value = root->constant.val;
            return 1;
        case NODE_NEG:
            if (!eval_predicate_node(p, root->neg.op)) {
                return 0;
            }
            root->eval_value = !root->neg.op->eval_value;
            return 1;
        case NODE_CONJUNCT:
            if (!eval_predicate_node(p, root->conjunct.op1)) {
                return 0;
            }
            if (!root->conjunct.op1->eval_value) {
                root->eval_value = 0;
                return 1;
            }
            if (!eval_predicate_node(p, root->conjunct.op2)) {
                return 0;
            }
            if (!root->conjunct.op2->eval_value) {
                root->eval_value = 0;
                return 1;
            }
            root->eval_value = 1;
            return 1;
        case NODE_DISJUNCT:
            if (!eval_predicate_node(p, root->disjunct.op1)) {
                return 0;
            }
            if (root->disjunct.op1->eval_value) {
                root->eval_value = 1;
                return 1;
            }
            if (!eval_predicate_node(p, root->disjunct.op2)) {
                return 0;
            }
            if (root->disjunct.op2->eval_value) {
                root->eval_value = 1;
                return 1;
            }
            root->eval_value = 0;
            return 1;
        case NODE_FUNCTION:
            return eval_function(p, root);
        default:
            ASSERT(0)
            return 0;
    }
}

int BPredicate_Init (BPredicate *p, char *str)
{
    // initialize input buffer object
    LexMemoryBufferInput input;
    LexMemoryBufferInput_Init(&input, str, strlen(str));
    
    // initialize lexical analyzer
    yyscan_t scanner;
    yylex_init_extra(&input, &scanner);
    
    // parse
    struct predicate_node *root = NULL;
    int result = yyparse(scanner, &root);
    
    // free lexical analyzer
    yylex_destroy(scanner);
    
    // check for errors
    if (LexMemoryBufferInput_HasError(&input) || result != 0 || !root) {
        if (root) {
            free_predicate_node(root);
        }
        return 0;
    }
    
    // init tree
    p->root = root;
    
    // init functions tree
    BAVL_Init(&p->functions_tree, OFFSET_DIFF(BPredicateFunction, name, tree_node), (BAVL_comparator)string_comparator, NULL);
    
    // init debuggind
    #ifndef NDEBUG
    p->in_function = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&p->d_obj);
    
    return 1;
}

void BPredicate_Free (BPredicate *p)
{
    ASSERT(BAVL_IsEmpty(&p->functions_tree))
    ASSERT(!p->in_function)
    
    // free debug object
    DebugObject_Free(&p->d_obj);
    
    // free tree
    free_predicate_node(p->root);
}

int BPredicate_Eval (BPredicate *p)
{
    ASSERT(!p->in_function)
    
    if (!eval_predicate_node(p, p->root)) {
        return -1;
    }
    
    return ((struct predicate_node *)p->root)->eval_value;
}

void BPredicateFunction_Init (BPredicateFunction *o, BPredicate *p, char *name, int *args, int num_args, BPredicate_callback callback, void *user)
{
    ASSERT(strlen(name) <= PREDICATE_MAX_NAME)
    ASSERT(!BAVL_LookupExact(&p->functions_tree, name))
    ASSERT(num_args >= 0)
    ASSERT(num_args <= PREDICATE_MAX_ARGS)
    for (int i = 0; i < num_args; i++) {
        ASSERT(args[i] == PREDICATE_TYPE_BOOL || args[i] == PREDICATE_TYPE_STRING)
    }
    ASSERT(!p->in_function)
    
    // init arguments
    o->p = p;
    strcpy(o->name, name);
    memcpy(o->args, args, num_args * sizeof(int));
    o->num_args = num_args;
    o->callback = callback;
    o->user = user;
    
    // add to tree
    ASSERT_EXECUTE(BAVL_Insert(&p->functions_tree, &o->tree_node, NULL))
    
    // init debug object
    DebugObject_Init(&o->d_obj);
}

void BPredicateFunction_Free (BPredicateFunction *o)
{
    ASSERT(!o->p->in_function)
    
    BPredicate *p = o->p;
    
    // free debug object
    DebugObject_Free(&o->d_obj);
    
    // remove from tree
    BAVL_Remove(&p->functions_tree, &o->tree_node);
}
