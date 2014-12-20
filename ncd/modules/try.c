/**
 * @file try.c
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
 * 
 * @section DESCRIPTION
 * 
 * 
 * Synopsis:
 *   try(string template_name, list args)
 *   do(string template_name)
 * 
 * Description:
 *   Does the following:
 *   1. Starts a template process from the specified template and arguments.
 *   2. Waits for the process to initialize completely, or for a _try->assert()
 *      assertion to fail or a _do->break() call.
 *   3. Initiates termination of the process and waits for it to terminate.
 *   4. Goes to up state. The "succeeded" variable reflects whether the process
 *      managed to initialize, or an assertion failed.
 *   If at any point during these steps termination of the try statement is
 *   requested, requests the process to terminate (if not already), and dies
 *   when it terminates. The differences between try() and do() are that do()
 *   directly exposes the caller scope (try() does via _caller), and the
 *   availability of assert/break.
 * 
 * Variables:
 *   string succeeded - "true" if the template process finished, "false" if assert
 *     or break was called.
 * 
 * 
 * Synopsis:
 *   try.try::assert(string cond)
 * 
 * Description:
 *   Call as _try->assert() from the template process of try(). If cond is
 *   "true", does nothing. Else, initiates termination of the process (if not
 *   already), and marks the try operation as not succeeded.
 * 
 * 
 * Synopsis:
 *   do.do::break()
 * 
 * Description:
 *   Call as _do->break() from the template process of do() to initiate
 *   premature termination, marking the do operation as not succeeded.
 */

#include <stdlib.h>
#include <string.h>

#include <misc/offset.h>

#include <ncd/module_common.h>

#include <generated/blog_channel_ncd_try.h>

struct instance {
    NCDModuleInst *i;
    int is_do;
    NCDModuleProcess process;
    int state;
    int dying;
    int succeeded;
};

enum {STATE_INIT, STATE_DEINIT, STATE_FINISHED};

static void process_handler_event (NCDModuleProcess *process, int event);
static int process_func_getspecialobj (NCDModuleProcess *process, NCD_string_id_t name, NCDObject *out_object);
static int process_caller_object_func_getobj (const NCDObject *obj, NCD_string_id_t name, NCDObject *out_object);
static void start_terminating (struct instance *o);
static void instance_free (struct instance *o);
static void instance_break (struct instance *o);

enum {STRING_TRY, STRING_TRY_TRY, STRING_DO, STRING_DO_DO};

static const char *strings[] = {"_try", "try.try", "_do", "do.do"};

static void process_handler_event (NCDModuleProcess *process, int event)
{
    struct instance *o = UPPER_OBJECT(process, struct instance, process);
    
    switch (event) {
        case NCDMODULEPROCESS_EVENT_UP: {
            ASSERT(o->state == STATE_INIT)
            
            // start terminating
            start_terminating(o);
        } break;
        
        case NCDMODULEPROCESS_EVENT_DOWN: {
            // Can't happen since we start terminating with it comes up.
            ASSERT(0)
        } break;
        
        case NCDMODULEPROCESS_EVENT_TERMINATED: {
            ASSERT(o->state == STATE_DEINIT)
            
            // free process
            NCDModuleProcess_Free(&o->process);
            
            // die finally if requested
            if (o->dying) {
                instance_free(o);
                return;
            }
            
            // signal up
            NCDModuleInst_Backend_Up(o->i);
            
            // set state finished
            o->state = STATE_FINISHED;
        } break;
    }
}

static int process_func_getspecialobj (NCDModuleProcess *process, NCD_string_id_t name, NCDObject *out_object)
{
    struct instance *o = UPPER_OBJECT(process, struct instance, process);
    ASSERT(o->state == STATE_INIT || o->state == STATE_DEINIT)
    
    if (o->is_do) {
        if (name == ModuleString(o->i, STRING_DO)) {
            *out_object = NCDObject_Build(ModuleString(o->i, STRING_DO_DO), o, NCDObject_no_getvar, NCDObject_no_getobj);
            return 1;
        }
        
        return NCDModuleInst_Backend_GetObj(o->i, name, out_object);
    } else {
        if (name == NCD_STRING_CALLER) {
            *out_object = NCDObject_Build(-1, o, NCDObject_no_getvar, process_caller_object_func_getobj);
            return 1;
        }
        
        if (name == ModuleString(o->i, STRING_TRY)) {
            *out_object = NCDObject_Build(ModuleString(o->i, STRING_TRY_TRY), o, NCDObject_no_getvar, NCDObject_no_getobj);
            return 1;
        }
        
        return 0;
    }
}

static int process_caller_object_func_getobj (const NCDObject *obj, NCD_string_id_t name, NCDObject *out_object)
{
    struct instance *o = NCDObject_DataPtr(obj);
    ASSERT(o->state == STATE_INIT || o->state == STATE_DEINIT)
    
    return NCDModuleInst_Backend_GetObj(o->i, name, out_object);
}

static void start_terminating (struct instance *o)
{
    ASSERT(o->state == STATE_INIT)
    
    // request process termination
    NCDModuleProcess_Terminate(&o->process);
    
    // set state deinit
    o->state = STATE_DEINIT;
}

static void func_new_common (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params, int is_do, NCDValRef template_name, NCDValRef args)
{
    struct instance *o = vo;
    o->i = i;
    o->is_do = is_do;
    
    // start process
    if (!NCDModuleProcess_InitValue(&o->process, i, template_name, args, process_handler_event)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDModuleProcess_Init failed");
        goto fail0;
    }
    
    // set special object function
    NCDModuleProcess_SetSpecialFuncs(&o->process, process_func_getspecialobj);
    
    // set state init, not dying, assume succeeded
    o->state = STATE_INIT;
    o->dying = 0;
    o->succeeded = 1;
    return;
    
fail0:
    NCDModuleInst_Backend_DeadError(i);
}

static void func_new_try (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    // check arguments
    NCDValRef template_name_arg;
    NCDValRef args_arg;
    if (!NCDVal_ListRead(params->args, 2, &template_name_arg, &args_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail;
    }
    if (!NCDVal_IsString(template_name_arg) || !NCDVal_IsList(args_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail;
    }
    
    return func_new_common(vo, i, params, 0, template_name_arg, args_arg);
    
fail:
    NCDModuleInst_Backend_DeadError(i);
}

static void func_new_do (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    // check arguments
    NCDValRef template_name_arg;
    if (!NCDVal_ListRead(params->args, 1, &template_name_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail;
    }
    if (!NCDVal_IsString(template_name_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail;
    }
    
    return func_new_common(vo, i, params, 1, template_name_arg, NCDVal_NewInvalid());
    
fail:
    NCDModuleInst_Backend_DeadError(i);
}

static void instance_free (struct instance *o)
{   
    NCDModuleInst_Backend_Dead(o->i);
}

static void instance_break (struct instance *o)
{
    ASSERT(o->state == STATE_INIT || o->state == STATE_DEINIT)
    
    // mark not succeeded
    o->succeeded = 0;
    
    // start terminating if not already
    if (o->state == STATE_INIT) {
        start_terminating(o);
    }
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    ASSERT(!o->dying)
    
    // if we're finished, die immediately
    if (o->state == STATE_FINISHED) {
        instance_free(o);
        return;
    }
    
    // set dying
    o->dying = 1;
    
    // start terminating if not already
    if (o->state == STATE_INIT) {
        start_terminating(o);
    }
}

static int func_getvar2 (void *vo, NCD_string_id_t name, NCDValMem *mem, NCDValRef *out)
{
    struct instance *o = vo;
    ASSERT(o->state == STATE_FINISHED)
    ASSERT(!o->dying)
    
    if (name == NCD_STRING_SUCCEEDED) {
        *out = ncd_make_boolean(mem, o->succeeded, o->i->params->iparams->string_index);
        return 1;
    }
    
    return 0;
}

static void assert_func_new (void *unused, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    // check arguments
    NCDValRef cond_arg;
    if (!NCDVal_ListRead(params->args, 1, &cond_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDVal_IsString(cond_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    
    // break if needed
    if (!NCDVal_StringEquals(cond_arg, "true")) {
        instance_break((struct instance *)params->method_user);
    }
    
    return;
    
fail1:
    NCDModuleInst_Backend_DeadError(i);
}

static void break_func_new (void *unused, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    // check arguments
    if (!NCDVal_ListRead(params->args, 0)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    
    // break
    instance_break((struct instance *)params->method_user);
    
    return;
    
fail1:
    NCDModuleInst_Backend_DeadError(i);
}

static struct NCDModule modules[] = {
    {
        .type = "try",
        .func_new2 = func_new_try,
        .func_die = func_die,
        .func_getvar2 = func_getvar2,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = "do",
        .func_new2 = func_new_do,
        .func_die = func_die,
        .func_getvar2 = func_getvar2,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = "try.try::assert",
        .func_new2 = assert_func_new
    }, {
        .type = "do.do::break",
        .func_new2 = break_func_new
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_try = {
    .modules = modules,
    .strings = strings
};
