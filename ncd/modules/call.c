/**
 * @file call.c
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
 *   callrefhere()
 * 
 * Description:
 *   Exposes variables and objects to call() statements as seen from this
 *   callrefhere() statement.
 * 
 * Synopsis:
 *   call(string template_name, list args)
 *   callhefhere::call(string template_name, list args)
 * 
 * Description:
 *   Module which allows using a single statement to represent multiple statements
 *   in a process template, allowing reuse of repetitive code.
 *   The created template process can access variables and objects as seen from the
 *   call statement via "_caller.variable".
 *   The second form also exposes variables and objects from the corresponding
 *   callrefhere() statement via "_ref.variable".
 *   If template_name is "<none>", then the call() is a no-op - it goes up
 *   immediately and immediately terminates on request.
 * 
 * Variables:
 *   Exposes variables as seen from the end of the called process template.
 * 
 * Behavior in detail (assuming template_name is not "<none>"):
 *   - On initialization, creates a new process from the template named
 *     template_name, with arguments args.
 *   - When all the statements in the created process go UP, transitions UP.
 *   - When one of the statements is no longer UP, transitions DOWN. The
 *     created process remais paused until the call statement receives the
 *     clean signal, to wait for following statements to deinitialize.
 *   - On deinitialization, initiates termination of the created process and waits
 *     for all its statements to deinitialize.
 */

#include <stdlib.h>
#include <string.h>

#include <misc/string_begins_with.h>
#include <misc/offset.h>
#include <structure/LinkedList0.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_call.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define STATE_WORKING 1
#define STATE_UP 2
#define STATE_WAITING 3
#define STATE_TERMINATING 4
#define STATE_NONE 5

struct callrefhere_instance {
    NCDModuleInst *i;
    LinkedList0 calls_list;
};

struct instance {
    NCDModuleInst *i;
    NCDValMem args_mem;
    NCDModuleProcess process;
    int state;
    struct callrefhere_instance *crh;
    LinkedList0Node calls_list_node;
};

static void instance_free (struct instance *o);
static int caller_obj_func_getobj (struct instance *o, const char *name, NCDObject *out_object);
static int ref_obj_func_getobj (struct instance *o, const char *name, NCDObject *out_object);

static void process_handler_event (struct instance *o, int event)
{
    switch (event) {
        case NCDMODULEPROCESS_EVENT_UP: {
            ASSERT(o->state == STATE_WORKING)
            
            // signal up
            NCDModuleInst_Backend_Up(o->i);
            
            // set state up
            o->state = STATE_UP;
        } break;
        
        case NCDMODULEPROCESS_EVENT_DOWN: {
            ASSERT(o->state == STATE_UP)
            
            // signal down
            NCDModuleInst_Backend_Down(o->i);
            
            // set state waiting
            o->state = STATE_WAITING;
        } break;
        
        case NCDMODULEPROCESS_EVENT_TERMINATED: {
            ASSERT(o->state == STATE_TERMINATING)
            
            // die finally
            instance_free(o);
            return;
        } break;
        
        default: ASSERT(0);
    }
}

static int process_func_getspecialobj (struct instance *o, const char *name, NCDObject *out_object)
{     
    if (!strcmp(name, "_caller")) {
        *out_object = NCDObject_Build(NULL, o, NULL, (NCDObject_func_getobj)caller_obj_func_getobj);
        return 1;
    }
    
    if (!strcmp(name, "_ref")) {
        *out_object = NCDObject_Build(NULL, o, NULL, (NCDObject_func_getobj)ref_obj_func_getobj);
        return 1;
    }
    
    return 0;
}

static void callrefhere_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct callrefhere_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // set arguments
    o->i = i;
    
    // init calls list
    LinkedList0_Init(&o->calls_list);
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void callrefhere_func_die (void *vo)
{
    struct callrefhere_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // disconnect calls
    while (!LinkedList0_IsEmpty(&o->calls_list)) {
        struct instance *inst = UPPER_OBJECT(LinkedList0_GetFirst(&o->calls_list), struct instance, calls_list_node);
        ASSERT(inst->crh == o)
        LinkedList0_Remove(&o->calls_list, &inst->calls_list_node);
        inst->crh = NULL;
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void func_new (NCDModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // check arguments
    NCDValRef template_name_arg;
    NCDValRef args_arg;
    if (!NCDVal_ListRead(i->args, 2, &template_name_arg, &args_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDVal_IsStringNoNulls(template_name_arg) || !NCDVal_IsList(args_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    const char *template_name = NCDVal_StringValue(template_name_arg);
    
    // calling none?
    if (!strcmp(template_name, "<none>")) {
        // signal up
        NCDModuleInst_Backend_Up(o->i);
        
        // set state none
        o->state = STATE_NONE;
    } else {
        // init args mem
        NCDValMem_Init(&o->args_mem);
        
        // copy arguments
        NCDValRef args = NCDVal_NewCopy(&o->args_mem, args_arg);
        if (NCDVal_IsInvalid(args)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewCopy failed");
            NCDValMem_Free(&o->args_mem);
            goto fail1;
        }
        
        // create process
        if (!NCDModuleProcess_Init(&o->process, o->i, template_name, args, o, (NCDModuleProcess_handler_event)process_handler_event)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDModuleProcess_Init failed");
            NCDValMem_Free(&o->args_mem);
            goto fail1;
        }
        
        // set special functions
        NCDModuleProcess_SetSpecialFuncs(&o->process,
                                        (NCDModuleProcess_func_getspecialobj)process_func_getspecialobj);
        
        // set callrefhere
        o->crh = (o->i->method_user ? NCDModuleInst_Backend_GetUser((NCDModuleInst *)i->method_user) : NULL);
        
        // add to callrefhere's calls list
        if (o->crh) {
            LinkedList0_Prepend(&o->crh->calls_list, &o->calls_list_node);
        }
        
        // set state working
        o->state = STATE_WORKING;
    }
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

void instance_free (struct instance *o)
{
    NCDModuleInst *i = o->i;
    
    if (o->state != STATE_NONE) {
        // remove from callrefhere's calls list
        if (o->crh) {
            LinkedList0_Remove(&o->crh->calls_list, &o->calls_list_node);
        }
        
        // free args mem
        NCDValMem_Free(&o->args_mem);
        
        // free process
        NCDModuleProcess_Free(&o->process);
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    ASSERT(o->state != STATE_TERMINATING)
    
    // if none, die now
    if (o->state == STATE_NONE) {
        instance_free(o);
        return;
    }
    
    // request process to terminate
    NCDModuleProcess_Terminate(&o->process);
    
    // set state terminating
    o->state = STATE_TERMINATING;
}

static void func_clean (void *vo)
{
    struct instance *o = vo;
    if (o->state != STATE_WAITING) {
        return;
    }
    
    // allow process to continue
    NCDModuleProcess_Continue(&o->process);
    
    // set state working
    o->state = STATE_WORKING;
}

static int func_getobj (void *vo, const char *name, NCDObject *out_object)
{
    struct instance *o = vo;
    
    if (o->state == STATE_NONE) {
        return 0;
    }
    
    return NCDModuleProcess_GetObj(&o->process, name, out_object);
}

static int caller_obj_func_getobj (struct instance *o, const char *name, NCDObject *out_object)
{
    return NCDModuleInst_Backend_GetObj(o->i, name, out_object);
}

static int ref_obj_func_getobj (struct instance *o, const char *name, NCDObject *out_object)
{
    if (!o->crh) {
        return 0;
    }
    
    return NCDModuleInst_Backend_GetObj(o->crh->i, name, out_object);
}

static const struct NCDModule modules[] = {
    {
        .type = "callrefhere",
        .func_new = callrefhere_func_new,
        .func_die = callrefhere_func_die
    }, {
        .type = "call",
        .func_new = func_new,
        .func_die = func_die,
        .func_clean = func_clean,
        .func_getobj = func_getobj,
        .flags = NCDMODULE_FLAG_CAN_RESOLVE_WHEN_DOWN
    }, {
        .type = "callrefhere::call",
        .func_new = func_new,
        .func_die = func_die,
        .func_clean = func_clean,
        .func_getobj = func_getobj,
        .flags = NCDMODULE_FLAG_CAN_RESOLVE_WHEN_DOWN
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_call = {
    .modules = modules
};
