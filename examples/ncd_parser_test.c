/**
 * @file ncd_tokenizer_test.c
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
#include <stdio.h>
#include <string.h>

#include <misc/debug.h>
#include <base/BLog.h>
#include <ncd/NCDConfigParser.h>

int error;

static void print_indent (unsigned int indent)
{
    while (indent > 0) {
        printf("  ");
        indent--;
    }
}

static void print_list (struct NCDConfig_list *l, unsigned int indent)
{
    while (l) {
        print_indent(indent);
        switch (l->type) {
            case NCDCONFIG_ARG_STRING: {
                printf("string: %s\n", l->string);
            } break;
            case NCDCONFIG_ARG_VAR: {
                printf("var: ");
                struct NCDConfig_strings *n = l->var;
                printf("%s", n->value);
                n = n->next;
                while (n) {
                    printf(".%s", n->value);
                    n = n->next;
                }
                printf("\n");
            } break;
            case NCDCONFIG_ARG_LIST: {
                printf("list\n");
                print_list(l->list, indent + 1);
            } break;
            default:
                ASSERT(0);
        }
        l = l->next;
    }
}

int main (int argc, char **argv)
{
    if (argc < 1) {
        return 1;
    }
    
    if (argc != 2) {
        printf("Usage: %s <string>\n", argv[0]);
        return 1;
    }
    
    BLog_InitStdout();
    
    // parse
    struct NCDConfig_processes *ast;
    if (!NCDConfigParser_Parse(argv[1], strlen(argv[1]), &ast)) {
        DEBUG("NCDConfigParser_Parse failed");
        return 1;
    }
    
    // print
    struct NCDConfig_processes *iface = ast;
    while (iface) {
        printf("process %s\n", iface->name);
        
        struct NCDConfig_statements *st = iface->statements;
        while (st) {
            struct NCDConfig_strings *name = st->names;
            ASSERT(name)
            printf("  %s", name->value);
            name = name->next;
            
            while (name) {
                printf(".%s", name->value);
                name = name->next;
            }
            
            printf("\n");
            
            print_list(st->args, 2);
            
            st = st->next;
        }
        
        iface = iface->next;
    }
    
    NCDConfig_free_processes(ast);
    
    return 0;
}
