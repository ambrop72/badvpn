/**
 * @file NCDEvaluator.c
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
#include <limits.h>

#include <base/BLog.h>
#include <ncd/make_name_indices.h>

#include "NCDEvaluator.h"

#include <generated/blog_channel_ncd.h>

struct NCDEvaluator__var {
    NCD_string_id_t *varnames;
    size_t num_names;
};

struct NCDEvaluator__call {
    int y;
};

#define GROWARRAY_NAME VarArray
#define GROWARRAY_OBJECT_TYPE NCDEvaluator
#define GROWARRAY_ARRAY_MEMBER vars
#define GROWARRAY_CAPACITY_MEMBER vars_capacity
#define GROWARRAY_MAX_CAPACITY INT_MAX
#include <misc/grow_array.h>

#define GROWARRAY_NAME CallArray
#define GROWARRAY_OBJECT_TYPE NCDEvaluator
#define GROWARRAY_ARRAY_MEMBER calls
#define GROWARRAY_CAPACITY_MEMBER calls_capacity
#define GROWARRAY_MAX_CAPACITY INT_MAX
#include <misc/grow_array.h>

static int add_expr_recurser (NCDEvaluator *o, NCDValue *value, NCDValMem *mem, NCDValRef *out)
{
    ASSERT((NCDValue_Type(value), 1))
    
    switch (NCDValue_Type(value)) {
        case NCDVALUE_STRING: {
            const char *str = NCDValue_StringValue(value);
            size_t len = NCDValue_StringLength(value);
            
            NCD_string_id_t string_id = NCDStringIndex_GetBin(o->string_index, str, len);
            if (string_id < 0) {
                BLog(BLOG_ERROR, "NCDStringIndex_GetBin failed");
                goto fail;
            }
            
            *out = NCDVal_NewIdString(mem, string_id, o->string_index);
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
                if (!add_expr_recurser(o, e, mem, &vval)) {
                    goto fail;
                }
                
                if (!NCDVal_ListAppend(*out, vval)) {
                    BLog(BLOG_ERROR, "depth limit exceeded");
                    goto fail;
                }
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
                if (!add_expr_recurser(o, ekey, mem, &vkey) ||
                    !add_expr_recurser(o, eval, mem, &vval)
                ) {
                    goto fail;
                }
                
                int inserted;
                if (!NCDVal_MapInsert(*out, vkey, vval, &inserted)) {
                    BLog(BLOG_ERROR, "depth limit exceeded");
                    goto fail;
                }
                if (!inserted) {
                    BLog(BLOG_ERROR, "duplicate key in map");
                    goto fail;
                }
            }
        } break;
        
        case NCDVALUE_VAR: {
            if (o->num_vars == o->vars_capacity && !VarArray_DoubleUp(o)) {
                BLog(BLOG_ERROR, "failed to grow var array");
                goto fail;
            }
            
            struct NCDEvaluator__var *var = &o->vars[o->num_vars];
            
            int plid = o->num_vars;
            if (plid >= NCDVAL_TOPPLID) {
                BLog(BLOG_ERROR, "too many placeholders");
                goto fail;
            }
            
            if (!ncd_make_name_indices(o->string_index, NCDValue_VarName(value), &var->varnames, &var->num_names)) {
                BLog(BLOG_ERROR, "ncd_make_name_indices failed");
                goto fail;
            }
            
            o->num_vars++;
            
            *out = NCDVal_NewPlaceholder(mem, plid);
        } break;
        
        default:
            BLog(BLOG_ERROR, "expression type not supported");
            goto fail;
    }
    
    return 1;
    
fail:
    return 0;
}

struct eval_context {
    NCDEvaluator *eval;
    NCDEvaluator_EvalFuncs const *funcs;
};

static int replace_placeholders_callback (void *arg, int plid, NCDValMem *mem, NCDValRef *out)
{
    struct eval_context const *context = arg;
    NCDEvaluator *o = context->eval;
    ASSERT(plid >= 0)
    ASSERT(plid < o->num_vars)
    
    struct NCDEvaluator__var *var = &o->vars[plid];
    
    return context->funcs->func_eval_var(context->funcs->user, var->varnames, var->num_names, mem, out);
}

int NCDEvaluator_Init (NCDEvaluator *o, NCDStringIndex *string_index)
{
    o->string_index = string_index;
    
    if (!VarArray_Init(o, 16)) {
        BLog(BLOG_ERROR, "NamesArray_Init failed");
        goto fail0;
    }
    
    if (!CallArray_Init(o, 16)) {
        BLog(BLOG_ERROR, "EntriesArray_Init failed");
        goto fail1;
    }
    
    o->num_vars = 0;
    o->num_calls = 0;
    
    return 1;
    
fail1:
    VarArray_Free(o);
fail0:
    return 0;
}

void NCDEvaluator_Free (NCDEvaluator *o)
{
    for (int i = 0; i < o->num_vars; i++) {
        BFree(o->vars[i].varnames);
    }
    
    CallArray_Free(o);
    VarArray_Free(o);
}

int NCDEvaluatorExpr_Init (NCDEvaluatorExpr *o, NCDEvaluator *eval, NCDValue *value)
{
    NCDValMem_Init(&o->mem);
    
    NCDValRef ref;
    if (!add_expr_recurser(eval, value, &o->mem, &ref)) {
        goto fail1;
    }
    
    o->ref = NCDVal_ToSafe(ref);
    
    if (!NCDValReplaceProg_Init(&o->prog, ref)) {
        BLog(BLOG_ERROR, "NCDValReplaceProg_Init failed");
        goto fail1;
    }
    
    return 1;
    
fail1:
    NCDValMem_Free(&o->mem);
    return 0;
}

void NCDEvaluatorExpr_Free (NCDEvaluatorExpr *o)
{
    NCDValReplaceProg_Free(&o->prog);
    NCDValMem_Free(&o->mem);
}

int NCDEvaluatorExpr_Eval (NCDEvaluatorExpr *o, NCDEvaluator *eval, NCDEvaluator_EvalFuncs const *funcs, NCDValMem *out_newmem, NCDValRef *out_val)
{
    if (!NCDValMem_InitCopy(out_newmem, &o->mem)) {
        BLog(BLOG_ERROR, "NCDValMem_InitCopy failed");
        goto fail0;
    }
    
    struct eval_context context;
    context.eval = eval;
    context.funcs = funcs;
    
    if (!NCDValReplaceProg_Execute(o->prog, out_newmem, replace_placeholders_callback, &context)) {
        BLog(BLOG_ERROR, "NCDValReplaceProg_Execute failed");
        goto fail1;
    }
    
    *out_val = NCDVal_FromSafe(out_newmem, o->ref);
    return 1;
    
fail1:
    NCDValMem_Free(out_newmem);
fail0:
    return 0;
}
