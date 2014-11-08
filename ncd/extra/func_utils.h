/**
 * @file func_utils.h
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

#ifndef NCD_FUNC_UTILS_H
#define NCD_FUNC_UTILS_H

#include <stddef.h>

#include <misc/debug.h>
#include <ncd/NCDVal.h>
#include <ncd/NCDEvaluator.h>

static int ncd_eval_func_args_ext (NCDEvaluatorArgs args, size_t start, size_t count, NCDValMem *mem, NCDValRef *out)
{
    ASSERT(start <= NCDEvaluatorArgs_Count(&args))
    ASSERT(count <= NCDEvaluatorArgs_Count(&args) - start)
    
    *out = NCDVal_NewList(mem, count);
    if (NCDVal_IsInvalid(*out)) {
        goto fail;
    }
    
    for (size_t i = 0; i < count; i++) {
        NCDValRef elem;
        if (!NCDEvaluatorArgs_EvalArg(&args, start + i, mem, &elem)) {
            goto fail;
        }
        if (!NCDVal_ListAppend(*out, elem)) {
            goto fail;
        }
    }
    
    return 1;
    
fail:
    return 0;
}

static int ncd_eval_func_args (NCDEvaluatorArgs args, NCDValMem *mem, NCDValRef *out)
{
    return ncd_eval_func_args_ext(args, 0, NCDEvaluatorArgs_Count(&args), mem, out);
}

#endif
