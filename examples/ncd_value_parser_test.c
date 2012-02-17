/**
 * @file ncd_value_parser_test.c
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
#include <ncd/NCDValueParser.h>

static void print_indent (unsigned int indent)
{
    while (indent > 0) {
        printf("  ");
        indent--;
    }
}

static void print_value (NCDValue *val, unsigned int indent)
{
    switch (NCDValue_Type(val)) {
        case NCDVALUE_STRING: {
            print_indent(indent);
            printf("string: '%s'\n", NCDValue_StringValue(val));
        } break;
        case NCDVALUE_LIST: {
            print_indent(indent);
            printf("list:\n");
            
            for (NCDValue *e = NCDValue_ListFirst(val); e; e = NCDValue_ListNext(val, e)) {
                print_value(e, indent + 1);
            }
        } break;
        default: ASSERT(0);
    }
}

int main (int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s <string>\n", (argc > 0 ? argv[0] : ""));
        return 1;
    }
    
    BLog_InitStdout();
    
    // parse
    NCDValue val;
    if (!NCDValueParser_Parse(argv[1], strlen(argv[1]), &val)) {
        DEBUG("NCDConfigParser_Parse failed");
        return 1;
    }
    
    // print
    print_value(&val, 0);
    
    NCDValue_Free(&val);
    
    return 0;
}
