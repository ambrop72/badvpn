/**
 * @file basic_functions.c
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

#include <ncd/module_common.h>

#include <generated/blog_channel_ncd_basic_functions.h>

static int error_eval (NCDEvaluatorArgs args, NCDValMem *mem, NCDValRef *out, struct NCDModuleFunction_eval_params const *params)
{
    FunctionLog(params, BLOG_ERROR, "error: failing");
    return 0;
}

static int identity_eval (NCDEvaluatorArgs args, NCDValMem *mem, NCDValRef *out, struct NCDModuleFunction_eval_params const *params)
{
    if (NCDEvaluatorArgs_Count(&args) != 1) {
        FunctionLog(params, BLOG_ERROR, "identity: need one argument");
        return 0;
    }
    return NCDEvaluatorArgs_EvalArg(&args, 0, mem, out);
}

static int if_eval (NCDEvaluatorArgs args, NCDValMem *mem, NCDValRef *out, struct NCDModuleFunction_eval_params const *params)
{
    if (NCDEvaluatorArgs_Count(&args) != 3) {
        FunctionLog(params, BLOG_ERROR, "if: need three arguments");
        return 0;
    }
    NCDValRef cond;
    if (!NCDEvaluatorArgs_EvalArg(&args, 0, mem, &cond)) {
        return 0;
    }
    int eval_arg = 2 - ncd_read_boolean(cond);
    return NCDEvaluatorArgs_EvalArg(&args, eval_arg, mem, out);
}

static int bool_eval (NCDEvaluatorArgs args, NCDValMem *mem, NCDValRef *out, struct NCDModuleFunction_eval_params const *params)
{
    if (NCDEvaluatorArgs_Count(&args) != 1) {
        FunctionLog(params, BLOG_ERROR, "bool: need one argument");
        return 0;
    }
    NCDValRef cond;
    if (!NCDEvaluatorArgs_EvalArg(&args, 0, mem, &cond)) {
        return 0;
    }
    int res = ncd_read_boolean(cond);
    *out = ncd_make_boolean(mem, res, params->params->iparams->string_index);
    return 1;
}

static int not_eval (NCDEvaluatorArgs args, NCDValMem *mem, NCDValRef *out, struct NCDModuleFunction_eval_params const *params)
{
    if (NCDEvaluatorArgs_Count(&args) != 1) {
        FunctionLog(params, BLOG_ERROR, "not: need one argument");
        return 0;
    }
    NCDValRef cond;
    if (!NCDEvaluatorArgs_EvalArg(&args, 0, mem, &cond)) {
        return 0;
    }
    int res = !ncd_read_boolean(cond);
    *out = ncd_make_boolean(mem, res, params->params->iparams->string_index);
    return 1;
}

static int and_eval (NCDEvaluatorArgs args, NCDValMem *mem, NCDValRef *out, struct NCDModuleFunction_eval_params const *params)
{
    size_t count = NCDEvaluatorArgs_Count(&args);
    int res = 1;
    for (size_t i = 0; i < count; i++) {
        NCDValRef cond;
        if (!NCDEvaluatorArgs_EvalArg(&args, i, mem, &cond)) {
            return 0;
        }
        if (!ncd_read_boolean(cond)) {
            res = 0;
            break;
        }
    }
    *out = ncd_make_boolean(mem, res, params->params->iparams->string_index);
    return 1;
}

static int or_eval (NCDEvaluatorArgs args, NCDValMem *mem, NCDValRef *out, struct NCDModuleFunction_eval_params const *params)
{
    size_t count = NCDEvaluatorArgs_Count(&args);
    int res = 0;
    for (size_t i = 0; i < count; i++) {
        NCDValRef cond;
        if (!NCDEvaluatorArgs_EvalArg(&args, i, mem, &cond)) {
            return 0;
        }
        if (ncd_read_boolean(cond)) {
            res = 1;
            break;
        }
    }
    *out = ncd_make_boolean(mem, res, params->params->iparams->string_index);
    return 1;
}

static int imp_eval (NCDEvaluatorArgs args, NCDValMem *mem, NCDValRef *out, struct NCDModuleFunction_eval_params const *params)
{
    if (NCDEvaluatorArgs_Count(&args) != 2) {
        FunctionLog(params, BLOG_ERROR, "imp: need two arguments");
        return 0;
    }
    int res = 0;
    for (size_t i = 0; i < 2; i++) {
        NCDValRef cond;
        if (!NCDEvaluatorArgs_EvalArg(&args, i, mem, &cond)) {
            return 0;
        }
        if (ncd_read_boolean(cond) == i) {
            res = 1;
            break;
        }
    }
    *out = ncd_make_boolean(mem, res, params->params->iparams->string_index);
    return 1;
}

static struct NCDModuleFunction const functions[] = {
    {
        .func_name = "__error__",
        .func_eval = error_eval
    }, {
        .func_name = "__identity__",
        .func_eval = identity_eval
    }, {
        .func_name = "__if__",
        .func_eval = if_eval
    }, {
        .func_name = "__bool__",
        .func_eval = bool_eval
    }, {
        .func_name = "__not__",
        .func_eval = not_eval
    }, {
        .func_name = "__and__",
        .func_eval = and_eval
    }, {
        .func_name = "__or__",
        .func_eval = or_eval
    }, {
        .func_name = "__imp__",
        .func_eval = imp_eval
    }, {
        .func_name = NULL
    }
};

const struct NCDModuleGroup ncdmodule_basic_functions = {
    .functions = functions
};
