/**
 * @file ncd_tokenizer_test.c
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
            case NCDCONFIG_ARG_MAPLIST: {
                printf("maplist\n");
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
