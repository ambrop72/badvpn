/**
 * @file NCDValueGenerator.c
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

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <misc/debug.h>
#include <misc/expstring.h>
#include <base/BLog.h>

#include <ncd/NCDValueGenerator.h>

#include <generated/blog_channel_NCDValueGenerator.h>

static int generate_value (NCDValue *value, ExpString *out_str)
{
    switch (NCDValue_Type(value)) {
        case NCDVALUE_STRING: {
            const char *str = NCDValue_StringValue(value);
            size_t len = NCDValue_StringLength(value);
            
            if (!ExpString_AppendChar(out_str, '"')) {
                BLog(BLOG_ERROR, "ExpString_AppendChar failed");
                goto fail;
            }
            
            for (size_t i = 0; i < len; i++) {
                if (str[i] == '\0') {
                    char buf[5];
                    snprintf(buf, sizeof(buf), "\\x%02"PRIx8, (uint8_t)str[i]);
                    
                    if (!ExpString_Append(out_str, buf)) {
                        BLog(BLOG_ERROR, "ExpString_Append failed");
                        goto fail;
                    }
                    
                    continue;
                }
                
                if (str[i] == '"' || str[i] == '\\') {
                    if (!ExpString_AppendChar(out_str, '\\')) {
                        BLog(BLOG_ERROR, "ExpString_AppendChar failed");
                        goto fail;
                    }
                }
                
                if (!ExpString_AppendChar(out_str, str[i])) {
                    BLog(BLOG_ERROR, "ExpString_AppendChar failed");
                    goto fail;
                }
            }
            
            if (!ExpString_AppendChar(out_str, '"')) {
                BLog(BLOG_ERROR, "ExpString_AppendChar failed");
                goto fail;
            }
        } break;
        
        case NCDVALUE_LIST: {
            if (!ExpString_AppendChar(out_str, '{')) {
                BLog(BLOG_ERROR, "ExpString_AppendChar failed");
                goto fail;
            }
            
            int is_first = 1;
            
            for (NCDValue *e = NCDValue_ListFirst(value); e; e = NCDValue_ListNext(value, e)) {
                if (!is_first) {
                    if (!ExpString_Append(out_str, ", ")) {
                        BLog(BLOG_ERROR, "ExpString_Append failed");
                        goto fail;
                    }
                }
                
                if (!generate_value(e, out_str)) {
                    goto fail;
                }
                
                is_first = 0;
            }
            
            if (!ExpString_AppendChar(out_str, '}')) {
                BLog(BLOG_ERROR, "ExpString_AppendChar failed");
                goto fail;
            }
        } break;
        
        case NCDVALUE_MAP: {
            if (!ExpString_AppendChar(out_str, '[')) {
                BLog(BLOG_ERROR, "ExpString_AppendChar failed");
                goto fail;
            }
            
            int is_first = 1;
            
            for (NCDValue *ekey = NCDValue_MapFirstKey(value); ekey; ekey = NCDValue_MapNextKey(value, ekey)) {
                NCDValue *eval = NCDValue_MapKeyValue(value, ekey);
                
                if (!is_first) {
                    if (!ExpString_Append(out_str, ", ")) {
                        BLog(BLOG_ERROR, "ExpString_Append failed");
                        goto fail;
                    }
                }
                
                if (!generate_value(ekey, out_str)) {
                    goto fail;
                }
                
                if (!ExpString_AppendChar(out_str, ':')) {
                    BLog(BLOG_ERROR, "ExpString_AppendChar failed");
                    goto fail;
                }
                
                if (!generate_value(eval, out_str)) {
                    goto fail;
                }
                
                is_first = 0;
            }
            
            if (!ExpString_AppendChar(out_str, ']')) {
                BLog(BLOG_ERROR, "ExpString_AppendChar failed");
                goto fail;
            }
        } break;
        
        case NCDVALUE_VAR: {
            if (!ExpString_Append(out_str, NCDValue_VarName(value))) {
                BLog(BLOG_ERROR, "ExpString_AppendChar failed");
                goto fail;
            }
        } break;
        
        default: ASSERT(0);
    }
    
    return 1;
    
fail:
    return 0;
}

char * NCDValueGenerator_Generate (NCDValue *value)
{
    NCDValue_Type(value);
    
    ExpString str;
    if (!ExpString_Init(&str)) {
        BLog(BLOG_ERROR, "ExpString_Init failed");
        goto fail0;
    }
    
    if (!generate_value(value, &str)) {
        goto fail1;
    }
    
    return ExpString_Get(&str);
    
fail1:
    ExpString_Free(&str);
fail0:
    return NULL;
}

int NCDValueGenerator_AppendGenerate (NCDValue *value, ExpString *str)
{
    NCDValue_Type(value);
    ASSERT(str)
    
    return generate_value(value, str);
}
