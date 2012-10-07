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
 * Synopsis:
 *   try(string template_name, list args)
 * 
 * Description:
 *   Does the following:
 *   1. Starts a template process from the specified template and arguments.
 *   2. Waits for the process to initialize completely, or for a _try->assert()
 *      assertion to fail.
 *   3. Initiates termination of the process and waits for it to terminate.
 *   4. Goes to up state. The "succeeded" variable reflects whether the process
 *      managed to initialize, or an assertion failed.
 *   If at any point during these steps termination of the try statement is
 *   requested, requests the process to terminate (if not already), and dies
 *   when it terminates.
 * 
 * Variables:
 *   string succeeded - "true" if the template process finished, "false" if assert
 *     was called.
 * 
 * Synopsis:
 *   try.try::assert(string cond)
 * 
 * Description:
 *   Call as _try->assert() from the template process. If cond is "true",
 *   does nothing. Else, initiates termination of the process (if not already),
 *   and marks the try operation as not succeeded.
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_try.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    NCDModuleProcess process;
    int state;
    int dying;
    int succeeded;
};

#define STATE_INIT 1
#define STATE_DEINIT 2
#define STATE_FINISHED 3

static void process_handler_event (struct instance *o, int event);
static int process_func_getspecialobj (struct instance *o, const char *name, NCDObject *out_object);
static int process_caller_object_func_getobj (struct instance *o, const char *name, NCDObject *out_object);
static void start_terminating (struct instance *o);
static void instance_free (struct instance *o);

static void process_handler_event (struct instance *o, int event)
{
    switch (event) {
        case NCDMODULEPROCESS_EVENT_UP: {
            ASSERT(o->state == STATE_INIT)
            
            // start terminating
            start_terminating(o);
        } break;
        
        case NCDMODULEPROCESS_EVENT_DOWN: {
            ASSERT(o->state == STATE_INIT)
            
            // continue
            NCDModuleProcess_Continue(&o->process);
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

static int process_func_getspecialobj (struct instance *o, const char *name, NCDObject *out_object)
{
    ASSERT(o->state == STATE_INIT || o->state == STATE_DEINIT)
    
    if (!strcmp(name, "_caller")) {
        *out_object = NCDObject_Build(NULL, o, NULL, (NCDObject_func_getobj)process_caller_object_func_getobj);
        return 1;
    }
    
    if (!strcmp(name, "_try")) {
        *out_object = NCDObject_Build("try.try", o, NULL, NULL);
        return 1;
    }
    
    return 0;
}

static int process_caller_object_func_getobj (struct instance *o, const char *name, NCDObject *out_object)
{
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

static void func_new (void *vo, NCDModuleInst *i)
{
    struct instance *o = vo;
    o->i = i;
    
    // check arguments
    NCDValRef template_name_arg;
    NCDValRef args_arg;
    if (!NCDVal_ListRead(i->args, 2, &template_name_arg, &args_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsStringNoNulls(template_name_arg) || !NCDVal_IsList(args_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    const char *template_name = NCDVal_StringValue(template_name_arg);
    
    // start process
    if (!NCDModuleProcess_Init(&o->process, i, template_name, args_arg, o, (NCDModuleProcess_handler_event)process_handler_event)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDModuleProcess_Init failed");
        goto fail0;
    }
    
    // set special object function
    NCDModuleProcess_SetSpecialFuncs(&o->process, (NCDModuleProcess_func_getspecialobj)process_func_getspecialobj);
    
    // set state init, not dying, assume succeeded
    o->state = STATE_INIT;
    o->dying = 0;
    o->succeeded = 1;
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void instance_free (struct instance *o)
{   
    NCDModuleInst_Backend_Dead(o->i);
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

static int func_getvar (void *vo, const char *name, NCDValMem *mem, NCDValRef *out)
{
    struct instance *o = vo;
    ASSERT(o->state == STATE_FINISHED)
    ASSERT(!o->dying)
    
    if (!strcmp(name, "succeeded")) {
        const char *str = (o->succeeded ? "true" : "false");
        
        *out = NCDVal_NewString(mem, str);
        if (NCDVal_IsInvalid(*out)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewString failed");
        }
        return 1;
    }
    
    return 0;
}

static void assert_func_new (void *unused, NCDModuleInst *i)
{
    // check arguments
    NCDValRef cond_arg;
    if (!NCDVal_ListRead(i->args, 1, &cond_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDVal_IsString(cond_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // get instance
    struct instance *mo = i->method_user;
    ASSERT(mo->state == STATE_INIT || mo->state == STATE_DEINIT)
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    
    if (!NCDVal_StringEquals(cond_arg, "true")) {
        // mark not succeeded
        mo->succeeded = 0;
        
        // start terminating if not already
        if (mo->state == STATE_INIT) {
            start_terminating(mo);
        }
    }
    
    return;
    
fail1:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static const struct NCDModule modules[] = {
    {
        .type = "try",
        .func_new2 = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = "try.try::assert",
        .func_new2 = assert_func_new
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_try = {
    .modules = modules
};
