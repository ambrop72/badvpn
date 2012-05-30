/**
 * @file NCDInterpValue.c
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

#include <misc/offset.h>
#include <misc/split_string.h>
#include <base/BLog.h>

#include "NCDInterpValue.h"

#include <generated/blog_channel_ncd.h>

static int NCDInterpValue_InitString (NCDInterpValue *o, const char *string, size_t len);
static int NCDInterpValue_InitVar (NCDInterpValue *o, const char *name);
static void NCDInterpValue_InitList (NCDInterpValue *o);
static int NCDInterpValue_ListAppend (NCDInterpValue *o, NCDInterpValue v);
static void NCDInterpValue_InitMap (NCDInterpValue *o);
static int NCDInterpValue_MapAppend (NCDInterpValue *o, NCDInterpValue key, NCDInterpValue val);

int NCDInterpValue_Init (NCDInterpValue *o, NCDValue *val_ast)
{
    switch (NCDValue_Type(val_ast)) {
        case NCDVALUE_STRING: {
            return NCDInterpValue_InitString(o, NCDValue_StringValue(val_ast), NCDValue_StringLength(val_ast));
        } break;
        
        case NCDVALUE_VAR: {
            return NCDInterpValue_InitVar(o, NCDValue_VarName(val_ast));
        } break;
        
        case NCDVALUE_LIST: {
            NCDInterpValue_InitList(o);
            
            for (NCDValue *ve = NCDValue_ListFirst(val_ast); ve; ve = NCDValue_ListNext(val_ast, ve)) {
                NCDInterpValue e;
                
                if (!NCDInterpValue_Init(&e, ve)) {
                    goto fail_list;
                }
                
                if (!NCDInterpValue_ListAppend(o, e)) {
                    NCDInterpValue_Free(&e);
                    goto fail_list;
                }
            }
            
            return 1;
            
        fail_list:
            NCDInterpValue_Free(o);
            return 0;
        } break;
        
        case NCDVALUE_MAP: {
            NCDInterpValue_InitMap(o);
            
            for (NCDValue *ekey = NCDValue_MapFirstKey(val_ast); ekey; ekey = NCDValue_MapNextKey(val_ast, ekey)) {
                NCDValue *eval = NCDValue_MapKeyValue(val_ast, ekey);
                
                NCDInterpValue key;
                NCDInterpValue val;
                
                if (!NCDInterpValue_Init(&key, ekey)) {
                    goto fail_map;
                }
                
                if (!NCDInterpValue_Init(&val, eval)) {
                    NCDInterpValue_Free(&key);
                    goto fail_map;
                }
                
                if (!NCDInterpValue_MapAppend(o, key, val)) {
                    NCDInterpValue_Free(&key);
                    NCDInterpValue_Free(&val);
                    goto fail_map;
                }
            }
            
            return 1;
            
        fail_map:
            NCDInterpValue_Free(o);
            return 0;
        } break;
        
        default:
            ASSERT(0);
            return 0;
    }
}

void NCDInterpValue_Free (NCDInterpValue *o)
{
    switch (o->type) {
        case NCDVALUE_STRING: {
            free(o->string);
        } break;
        
        case NCDVALUE_VAR: {
            free_strings(o->variable_names);
        } break;
        
        case NCDVALUE_LIST: {
            while (!LinkedList1_IsEmpty(&o->list)) {
                struct NCDInterpValueListElem *elem = UPPER_OBJECT(LinkedList1_GetFirst(&o->list), struct NCDInterpValueListElem, list_node);
                NCDInterpValue_Free(&elem->value);
                LinkedList1_Remove(&o->list, &elem->list_node);
                free(elem);
            }
        } break;
        
        case NCDVALUE_MAP: {
            while (!LinkedList1_IsEmpty(&o->maplist)) {
                struct NCDInterpValueMapElem *elem = UPPER_OBJECT(LinkedList1_GetFirst(&o->maplist), struct NCDInterpValueMapElem, maplist_node);
                NCDInterpValue_Free(&elem->key);
                NCDInterpValue_Free(&elem->val);
                LinkedList1_Remove(&o->maplist, &elem->maplist_node);
                free(elem);
            }
        } break;
        
        default: ASSERT(0);
    }
}

int NCDInterpValue_InitString (NCDInterpValue *o, const char *string, size_t len)
{
    o->type = NCDVALUE_STRING;
    
    if (!(o->string = malloc(len))) {
        BLog(BLOG_ERROR, "malloc failed");
        return 0;
    }
    memcpy(o->string, string, len);
    
    o->string_len = len;
    
    return 1;
}

int NCDInterpValue_InitVar (NCDInterpValue *o, const char *name)
{
    ASSERT(name)
    
    o->type = NCDVALUE_VAR;
    if (!(o->variable_names = split_string(name, '.'))) {
        return 0;
    }
    
    return 1;
}

void NCDInterpValue_InitList (NCDInterpValue *o)
{
    o->type = NCDVALUE_LIST;
    LinkedList1_Init(&o->list);
}

int NCDInterpValue_ListAppend (NCDInterpValue *o, NCDInterpValue v)
{
    ASSERT(o->type == NCDVALUE_LIST)
    
    struct NCDInterpValueListElem *elem = malloc(sizeof(*elem));
    if (!elem) {
        BLog(BLOG_ERROR, "malloc failed");
        return 0;
    }
    LinkedList1_Append(&o->list, &elem->list_node);
    elem->value = v;
    
    return 1;
}

void NCDInterpValue_InitMap (NCDInterpValue *o)
{
    o->type = NCDVALUE_MAP;
    LinkedList1_Init(&o->maplist);
}

int NCDInterpValue_MapAppend (NCDInterpValue *o, NCDInterpValue key, NCDInterpValue val)
{
    ASSERT(o->type == NCDVALUE_MAP)
    
    struct NCDInterpValueMapElem *elem = malloc(sizeof(*elem));
    if (!elem) {
        BLog(BLOG_ERROR, "malloc failed");
        return 0;
    }
    LinkedList1_Append(&o->maplist, &elem->maplist_node);
    elem->key = key;
    elem->val = val;
    
    return 1;
}
