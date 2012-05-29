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
#include <stdlib.h>

#include <misc/debug.h>
#include <base/BLog.h>
#include <ncd/NCDConfigParser.h>
#include <ncd/NCDValueGenerator.h>

int error;

static void print_indent (unsigned int indent)
{
    while (indent > 0) {
        printf("  ");
        indent--;
    }
}

static void print_value (NCDValue *v, unsigned int indent)
{
    char *str = NCDValueGenerator_Generate(v);
    if (!str) {
        DEBUG("NCDValueGenerator_Generate failed");
        exit(1);
    }
    
    print_indent(indent);
    printf("%s\n", str);
    
    free(str);
}

static void print_block (NCDBlock *block, unsigned int indent)
{
    for (NCDStatement *st = NCDBlock_FirstStatement(block); st; st = NCDBlock_NextStatement(block, st)) {
        const char *name = NCDStatement_Name(st) ? NCDStatement_Name(st) : "";
        
        switch (NCDStatement_Type(st)) {
            case NCDSTATEMENT_REG: {
                const char *objname = NCDStatement_RegObjName(st) ? NCDStatement_RegObjName(st) : "";
                const char *cmdname = NCDStatement_RegCmdName(st);
                
                print_indent(indent);
                printf("reg name=%s objname=%s cmdname=%s args:\n", name, objname, cmdname);
                
                print_value(NCDStatement_RegArgs(st), indent + 2);
            } break;
            
            case NCDSTATEMENT_IF: {
                print_indent(indent);
                printf("if name=%s\n", name);
                
                NCDIfBlock *ifb = NCDStatement_IfBlock(st);
                
                for (NCDIf *ifc = NCDIfBlock_FirstIf(ifb); ifc; ifc = NCDIfBlock_NextIf(ifb, ifc)) {
                    print_indent(indent + 2);
                    printf("if\n");
                    
                    print_value(NCDIf_Cond(ifc), indent + 4);
                    
                    print_indent(indent + 2);
                    printf("then\n");
                    
                    print_block(NCDIf_Block(ifc), indent + 4);
                }
                
                if (NCDStatement_IfElse(st)) {
                    print_indent(indent + 2);
                    printf("else\n");
                    
                    print_block(NCDStatement_IfElse(st), indent + 4);
                }
            } break;
            
            default: ASSERT(0);
        }
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
    NCDProgram prog;
    if (!NCDConfigParser_Parse(argv[1], strlen(argv[1]), &prog)) {
        DEBUG("NCDConfigParser_Parse failed");
        return 1;
    }
    
    // print
    for (NCDProcess *p = NCDProgram_FirstProcess(&prog); p; p = NCDProgram_NextProcess(&prog, p)) {
        printf("process name=%s is_template=%d\n", NCDProcess_Name(p), NCDProcess_IsTemplate(p));
        print_block(NCDProcess_Block(p), 2);
    }
    
    NCDProgram_Free(&prog);
    
    return 0;
}
