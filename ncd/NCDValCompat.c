/**
 * @file NCDValCompat.c
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

#include "NCDValCompat.h"

int NCDValCompat_ValueToVal (NCDValue *value, NCDValMem *mem, NCDValRef *out)
{
    ASSERT((NCDValue_Type(value), 1))
    ASSERT(mem)
    ASSERT(out)
    
    switch (NCDValue_Type(value)) {
        case NCDVALUE_STRING: {
            *out = NCDVal_NewStringBin(mem, (const uint8_t *)NCDValue_StringValue(value), NCDValue_StringLength(value));
            if (NCDVal_IsInvalid(*out)) {
                goto fail;
            }
        } break;
        
        case NCDVALUE_LIST: {
            *out = NCDVal_NewList(mem, NCDValue_ListCount(value));
            if (NCDVal_IsInvalid(*out)) {
                goto fail;
            }
            
            for (NCDValue *e = NCDValue_ListFirst(value); e; e = NCDValue_ListNext(value, e)) {
                NCDValRef vval;
                if (!NCDValCompat_ValueToVal(e, mem, &vval)) {
                    goto fail;
                }
                
                NCDVal_ListAppend(*out, vval);
            }
        } break;
        
        case NCDVALUE_MAP: {
            *out = NCDVal_NewMap(mem, NCDValue_MapCount(value));
            if (NCDVal_IsInvalid(*out)) {
                goto fail;
            }
            
            for (NCDValue *ekey = NCDValue_MapFirstKey(value); ekey; ekey = NCDValue_MapNextKey(value, ekey)) {
                NCDValue *eval = NCDValue_MapKeyValue(value, ekey);
                
                NCDValRef vkey;
                NCDValRef vval;
                if (!NCDValCompat_ValueToVal(ekey, mem, &vkey) || !NCDValCompat_ValueToVal(eval, mem, &vval)) {
                    goto fail;
                }
                
                int res = NCDVal_MapInsert(*out, vkey, vval);
                ASSERT(res)
            }
        } break;
        
        default:
            goto fail;
    }
    
    return 1;
    
fail:
    return 0;
}

int NCDValCompat_ValToValue (NCDValRef value, NCDValue *out)
{
    ASSERT(!NCDVal_IsInvalid(value))
    ASSERT(out)
    
    switch (NCDVal_Type(value)) {
        case NCDVAL_STRING: {
            if (!NCDValue_InitStringBin(out, (const uint8_t *)NCDVal_StringValue(value), NCDVal_StringLength(value))) {
                goto fail0;
            }
        } break;
        
        case NCDVAL_LIST: {
            NCDValue_InitList(out);
            
            size_t count = NCDVal_ListCount(value);
            
            for (size_t j = 0; j < count; j++) {
                NCDValRef velem = NCDVal_ListGet(value, j);
                
                NCDValue elem;
                if (!NCDValCompat_ValToValue(velem, &elem)) {
                    goto fail1;
                }
                
                if (!NCDValue_ListAppend(out, elem)) {
                    NCDValue_Free(&elem);
                    goto fail1;
                }
            }
        } break;
        
        case NCDVAL_MAP: {
            NCDValue_InitMap(out);
            
            for (NCDValMapElem e = NCDVal_MapFirst(value); !NCDVal_MapElemInvalid(e); e = NCDVal_MapNext(value, e)) {
                NCDValRef vkey = NCDVal_MapElemKey(value, e);
                NCDValRef vval = NCDVal_MapElemVal(value, e);
                
                NCDValue key;
                if (!NCDValCompat_ValToValue(vkey, &key)) {
                    goto fail1;
                }
                
                NCDValue val;
                if (!NCDValCompat_ValToValue(vval, &val)) {
                    NCDValue_Free(&key);
                    goto fail1;
                }
                
                if (!NCDValue_MapInsert(out, key, val)) {
                    NCDValue_Free(&key);
                    NCDValue_Free(&val);
                    goto fail1;
                }
            }
        } break;
        
        default:
            ASSERT(0);
            return 0;
    }
    
    return 1;
    
fail1:
    NCDValue_Free(out);
fail0:
    return 0;
}
