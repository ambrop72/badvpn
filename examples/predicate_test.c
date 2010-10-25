/**
 * @file predicate_test.c
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
#include <string.h>

#include <predicate/BPredicate.h>
#include <system/BLog.h>

static int func_hello (void *user, void **args)
{
    return 1;
}

static int func_neg (void *user, void **args)
{
    int arg = *((int *)args[0]);
    
    return !arg;
}

static int func_conj (void *user, void **args)
{
    int arg1 = *((int *)args[0]);
    int arg2 = *((int *)args[1]);
    
    return (arg1 && arg2);
}

static int func_strcmp (void *user, void **args)
{
    char *arg1 = args[0];
    char *arg2 = args[1];
    
    return (!strcmp(arg1, arg2));
}

static int func_error (void *user, void **args)
{
    return -1;
}

int main (int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <predicate>\n", argv[0]);
        return 1;
    }
    
    // init logger
    BLog_InitStdout();
    
    // init predicate
    BPredicate pr;
    if (!BPredicate_Init(&pr, argv[1])) {
        fprintf(stderr, "BPredicate_Init failed\n");
        return 1;
    }
    
    // init functions
    BPredicateFunction f_hello;
    BPredicateFunction_Init(&f_hello, &pr, "hello", NULL, 0, func_hello, NULL);
    BPredicateFunction f_neg;
    BPredicateFunction_Init(&f_neg, &pr, "neg", (int []){PREDICATE_TYPE_BOOL}, 1, func_neg, NULL);
    BPredicateFunction f_conj;
    BPredicateFunction_Init(&f_conj, &pr, "conj", (int []){PREDICATE_TYPE_BOOL, PREDICATE_TYPE_BOOL}, 2, func_conj, NULL);
    BPredicateFunction f_strcmp;
    BPredicateFunction_Init(&f_strcmp, &pr, "strcmp", (int []){PREDICATE_TYPE_STRING, PREDICATE_TYPE_STRING}, 2, func_strcmp, NULL);
    BPredicateFunction f_error;
    BPredicateFunction_Init(&f_error, &pr, "error", NULL, 0, func_error, NULL);
    
    // evaluate
    int result = BPredicate_Eval(&pr);
    printf("%d\n", result);
    
    // free functions
    BPredicateFunction_Free(&f_hello);
    BPredicateFunction_Free(&f_neg);
    BPredicateFunction_Free(&f_conj);
    BPredicateFunction_Free(&f_strcmp);
    BPredicateFunction_Free(&f_error);
    
    // free predicate
    BPredicate_Free(&pr);
    
    return 0;
}
