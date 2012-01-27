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
    NCDModuleProcess process;
    int state;
    struct callrefhere_instance *crh;
    LinkedList0Node calls_list_node;
};

static void instance_free (struct instance *o);

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

static int process_func_getspecialvar (struct instance *o, const char *name, NCDValue *out)
{
    size_t p;
    
    if (p = string_begins_with(name, "_caller.")) {
        return NCDModuleInst_Backend_GetVar(o->i, name + p, out);
    }
    
    if (o->crh && (p = string_begins_with(name, "_ref."))) {
        return NCDModuleInst_Backend_GetVar(o->crh->i, name + p, out);
    }
    
    return 0;
}

static NCDModuleInst * process_func_getspecialobj (struct instance *o, const char *name)
{
    size_t p;
    
    if (p = string_begins_with(name, "_caller.")) {
        return NCDModuleInst_Backend_GetObj(o->i, name + p);
    }
    
    if (o->crh && (p = string_begins_with(name, "_ref."))) {
        return NCDModuleInst_Backend_GetObj(o->crh->i, name + p);
    }
    
    return NULL;
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
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    NCDValue *template_name_arg;
    NCDValue *args_arg;
    if (!NCDValue_ListRead(o->i->args, 2, &template_name_arg, &args_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(template_name_arg) != NCDVALUE_STRING || NCDValue_Type(args_arg) != NCDVALUE_LIST) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    char *template_name = NCDValue_StringValue(template_name_arg);
    
    // calling none?
    if (!strcmp(template_name, "<none>")) {
        // signal up
        NCDModuleInst_Backend_Up(o->i);
        
        // set state none
        o->state = STATE_NONE;
    } else {
        // copy arguments
        NCDValue args;
        if (!NCDValue_InitCopy(&args, args_arg)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
            goto fail1;
        }
        
        // create process
        if (!NCDModuleProcess_Init(&o->process, o->i, template_name, args, o, (NCDModuleProcess_handler_event)process_handler_event)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDModuleProcess_Init failed");
            NCDValue_Free(&args);
            goto fail1;
        }
        
        // set special functions
        NCDModuleProcess_SetSpecialFuncs(&o->process,
                                        (NCDModuleProcess_func_getspecialvar)process_func_getspecialvar,
                                        (NCDModuleProcess_func_getspecialobj)process_func_getspecialobj);
        
        // set callrefhere
        o->crh = (o->i->method_object ? o->i->method_object->inst_user : NULL);
        
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

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    
    if (o->state == STATE_NONE) {
        return 0;
    }
    
    return NCDModuleProcess_GetVar(&o->process, name, out);
}

static NCDModuleInst * func_getobj (void *vo, const char *name)
{
    struct instance *o = vo;
    
    if (o->state == STATE_NONE) {
        return NULL;
    }
    
    return NCDModuleProcess_GetObj(&o->process, name);
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
        .func_getvar = func_getvar,
        .func_getobj = func_getobj,
        .can_resolve_when_down = 1
    }, {
        .type = "callrefhere::call",
        .func_new = func_new,
        .func_die = func_die,
        .func_clean = func_clean,
        .func_getvar = func_getvar,
        .func_getobj = func_getobj,
        .can_resolve_when_down = 1
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_call = {
    .modules = modules
};
