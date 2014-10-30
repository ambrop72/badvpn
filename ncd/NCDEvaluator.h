/**
 * @file NCDEvaluator.h
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

#ifndef BADVPN_NCDEVALUATOR_H
#define BADVPN_NCDEVALUATOR_H

#include <stddef.h>

#include <misc/debug.h>
#include <ncd/NCDAst.h>
#include <ncd/NCDStringIndex.h>
#include <ncd/NCDVal.h>

struct NCDEvaluator__var;
struct NCDEvaluator__call;

typedef struct {
    NCDStringIndex *string_index;
    struct NCDEvaluator__var *vars;
    struct NCDEvaluator__call *calls;
    int vars_capacity;
    int calls_capacity;
    int num_vars;
    int num_calls;
} NCDEvaluator;

typedef struct {
    NCDValMem mem;
    NCDValSafeRef ref;
    NCDValReplaceProg prog;
} NCDEvaluatorExpr;

typedef struct {
    void *user;
    int (*func_eval_var) (void *user, NCD_string_id_t const *varnames, size_t num_names, NCDValMem *mem, NCDValRef *out);
} NCDEvaluator_EvalFuncs;

int NCDEvaluator_Init (NCDEvaluator *o, NCDStringIndex *string_index) WARN_UNUSED;
void NCDEvaluator_Free (NCDEvaluator *o);
int NCDEvaluatorExpr_Init (NCDEvaluatorExpr *o, NCDEvaluator *eval, NCDValue *value) WARN_UNUSED;
void NCDEvaluatorExpr_Free (NCDEvaluatorExpr *o);
int NCDEvaluatorExpr_Eval (NCDEvaluatorExpr *o, NCDEvaluator *eval, NCDEvaluator_EvalFuncs const *funcs, NCDValMem *out_newmem, NCDValRef *out_val) WARN_UNUSED;

#endif
