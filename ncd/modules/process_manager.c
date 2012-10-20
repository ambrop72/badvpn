/**
 * @file process_manager.c
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
 * Module which allows starting and stopping processes from templates dynamically.
 * 
 * Synopsis: process_manager()
 * Description: manages processes. On deinitialization, initiates termination of all
 *   contained processes and waits for them to terminate.
 * 
 * Synopsis: process_manager::start(string name, string template_name, list args)
 * Description: creates a new process from the template named template_name, with arguments args,
 *   identified by name within the process manager. If a process with this name already exists
 *   and is not being terminated, does nothing. If it is being terminated, it will be restarted
 *   using the given parameters after it terminates.
 *   The process can access objects as seen from the process_manager() statement via _caller.
 * 
 * Synopsis: process_manager::stop(string name)
 * Description: initiates termination of the process identified by name within the process manager.
 *   If there is no such process, or the process is already being terminated, does nothing.
 */

#include <stdlib.h>
#include <string.h>

#include <misc/offset.h>
#include <misc/debug.h>
#include <structure/LinkedList1.h>
#include <ncd/NCDModule.h>
#include <ncd/value_utils.h>

#include <generated/blog_channel_ncd_process_manager.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define RETRY_TIME 10000

#define PROCESS_STATE_RUNNING 1
#define PROCESS_STATE_STOPPING 2
#define PROCESS_STATE_RESTARTING 3
#define PROCESS_STATE_RETRYING 4

struct instance {
    NCDModuleInst *i;
    LinkedList1 processes_list;
    int dying;
};

struct process {
    struct instance *manager;
    char *name;
    BTimer retry_timer;
    LinkedList1Node processes_list_node;
    int have_params;
    NCD_string_id_t params_template_name;
    NCDValMem params_mem;
    NCDValRef params_args;
    int have_module_process;
    NCDValMem process_mem;
    NCDValRef process_args;
    NCDModuleProcess module_process;
    int state;
};

static struct process * find_process (struct instance *o, const char *name);
static int process_new (struct instance *o, const char *name, NCDValRef template_name, NCDValRef args);
static void process_free (struct process *p);
static void process_retry_timer_handler (struct process *p);
static void process_module_process_handler_event (struct process *p, int event);
static int process_module_process_func_getspecialobj (struct process *p, NCD_string_id_t name, NCDObject *out_object);
static int process_module_process_caller_obj_func_getobj (struct process *p, NCD_string_id_t name, NCDObject *out_object);
static void process_stop (struct process *p);
static int process_restart (struct process *p, NCDValRef template_name, NCDValRef args);
static void process_try (struct process *p);
static int process_set_params (struct process *p, NCDValRef template_name, NCDValMem mem, NCDValSafeRef args);
static void instance_free (struct instance *o);

enum {STRING_CALLER};

static struct NCD_string_request strings[] = {
    {"_caller"}, {NULL}
};

struct process * find_process (struct instance *o, const char *name)
{
    LinkedList1Node *n = LinkedList1_GetFirst(&o->processes_list);
    while (n) {
        struct process *p = UPPER_OBJECT(n, struct process, processes_list_node);
        if (!strcmp(p->name, name)) {
            return p;
        }
        n = LinkedList1Node_Next(n);
    }
    
    return NULL;
}

int process_new (struct instance *o, const char *name, NCDValRef template_name, NCDValRef args)
{
    ASSERT(!o->dying)
    ASSERT(!find_process(o, name))
    ASSERT(NCDVal_IsString(template_name))
    ASSERT(NCDVal_IsList(args))
    
    // allocate structure
    struct process *p = malloc(sizeof(*p));
    if (!p) {
        ModuleLog(o->i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    
    // set manager
    p->manager = o;
    
    // copy name
    if (!(p->name = strdup(name))) {
        ModuleLog(o->i, BLOG_ERROR, "strdup failed");
        goto fail1;
    }
    
    // init retry timer
    BTimer_Init(&p->retry_timer, RETRY_TIME, (BTimer_handler)process_retry_timer_handler, p);
    
    // insert to processes list
    LinkedList1_Append(&o->processes_list, &p->processes_list_node);
    
    // have no params
    p->have_params = 0;
    
    // have no module process
    p->have_module_process = 0;
    
    // copy arguments
    NCDValMem mem;
    NCDValMem_Init(&mem);
    NCDValRef args2 = NCDVal_NewCopy(&mem, args);
    if (NCDVal_IsInvalid(args2)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewCopy failed");
        goto fail2;
    }
    
    // set params
    if (!process_set_params(p, template_name, mem, NCDVal_ToSafe(args2))) {
        goto fail2;
    }
    
    // try starting it
    process_try(p);
    
    return 1;
    
fail2:
    NCDValMem_Free(&mem);
    LinkedList1_Remove(&o->processes_list, &p->processes_list_node);
    free(p->name);
fail1:
    free(p);
fail0:
    return 0;
}

void process_free (struct process *p)
{
    ASSERT(!p->have_module_process)
    struct instance *o = p->manager;
    
    // free params
    if (p->have_params) {
        NCDValMem_Free(&p->params_mem);
    }
    
    // remove from processes list
    LinkedList1_Remove(&o->processes_list, &p->processes_list_node);
    
    // free timer
    BReactor_RemoveTimer(o->i->params->iparams->reactor, &p->retry_timer);
    
    // free name
    free(p->name);
    
    // free structure
    free(p);
}

void process_retry_timer_handler (struct process *p)
{
    struct instance *o = p->manager;
    B_USE(o)
    ASSERT(p->state == PROCESS_STATE_RETRYING)
    ASSERT(!o->dying)
    ASSERT(p->have_params)
    ASSERT(!p->have_module_process)
    
    // retry
    process_try(p);
}

void process_module_process_handler_event (struct process *p, int event)
{
    struct instance *o = p->manager;
    ASSERT(p->have_module_process)
    
    if (event == NCDMODULEPROCESS_EVENT_DOWN) {
        // allow process to continue
        NCDModuleProcess_Continue(&p->module_process);
    }
    
    if (event != NCDMODULEPROCESS_EVENT_TERMINATED) {
        return;
    }
    
    // free module process
    NCDModuleProcess_Free(&p->module_process);
    
    // free arguments mem
    NCDValMem_Free(&p->process_mem);
    
    // set no module process
    p->have_module_process = 0;
    
    switch (p->state) {
        case PROCESS_STATE_STOPPING: {
            // free process
            process_free(p);
        
            // if manager is dying and there are no more processes, let it die
            if (o->dying && LinkedList1_IsEmpty(&o->processes_list)) {
                instance_free(o);
            }
            
            return;
        } break;
        
        case PROCESS_STATE_RESTARTING: {
            ASSERT(!o->dying)
            ASSERT(p->have_params)
            
            // restart
            process_try(p);
        } break;
        
        default: ASSERT(0);
    }
}

int process_module_process_func_getspecialobj (struct process *p, NCD_string_id_t name, NCDObject *out_object)
{
    ASSERT(p->have_module_process)
    
    if (name == strings[STRING_CALLER].id) {
        *out_object = NCDObject_Build(-1, p, NULL, (NCDObject_func_getobj)process_module_process_caller_obj_func_getobj);
        return 1;
    }
    
    return 0;
}

int process_module_process_caller_obj_func_getobj (struct process *p, NCD_string_id_t name, NCDObject *out_object)
{
    struct instance *o = p->manager;
    ASSERT(p->have_module_process)
    
    return NCDModuleInst_Backend_GetObj(o->i, name, out_object);
}

void process_stop (struct process *p)
{
    switch (p->state) {
        case PROCESS_STATE_RETRYING: {
            ASSERT(!p->have_module_process)
            
            // free process
            process_free(p);
            return;
        } break;
        
        case PROCESS_STATE_RUNNING: {
            ASSERT(p->have_module_process)
            
            // request process to terminate
            NCDModuleProcess_Terminate(&p->module_process);
            
            // set state
            p->state = PROCESS_STATE_STOPPING;
        } break;
        
        case PROCESS_STATE_RESTARTING: {
            ASSERT(p->have_params)
            
            // free params
            NCDValMem_Free(&p->params_mem);
            p->have_params = 0;
            
            // set state
            p->state = PROCESS_STATE_STOPPING;
        } break;
        
        case PROCESS_STATE_STOPPING: {
            // nothing to do
        } break;
        
        default: ASSERT(0);
    }
}

int process_restart (struct process *p, NCDValRef template_name, NCDValRef args)
{
    struct instance *o = p->manager;
    ASSERT(!o->dying)
    ASSERT(p->state == PROCESS_STATE_STOPPING)
    ASSERT(!p->have_params)
    ASSERT(NCDVal_IsString(template_name))
    ASSERT(NCDVal_IsList(args))
    
    // copy arguments
    NCDValMem mem;
    NCDValMem_Init(&mem);
    NCDValRef args2 = NCDVal_NewCopy(&mem, args);
    if (NCDVal_IsInvalid(args2)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewCopy failed");
        goto fail1;
    }
    
    // set params
    if (!process_set_params(p, template_name, mem, NCDVal_ToSafe(args2))) {
        goto fail1;
    }
    
    // set state
    p->state = PROCESS_STATE_RESTARTING;
    
    return 1;
    
fail1:
    NCDValMem_Free(&mem);
    return 0;
}

void process_try (struct process *p)
{
    struct instance *o = p->manager;
    ASSERT(!o->dying)
    ASSERT(p->have_params)
    ASSERT(!p->have_module_process)
    
    ModuleLog(o->i, BLOG_INFO, "trying process %s", p->name);
    
    // move params
    p->process_mem = p->params_mem;
    p->process_args = NCDVal_Moved(&p->process_mem, p->params_args);
    
    // init module process
    if (!NCDModuleProcess_InitId(&p->module_process, o->i, p->params_template_name, p->process_args, p, (NCDModuleProcess_handler_event)process_module_process_handler_event)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDModuleProcess_Init failed");
        
        // set timer
        BReactor_SetTimer(o->i->params->iparams->reactor, &p->retry_timer);
        
        // set state
        p->state = PROCESS_STATE_RETRYING;
        return;
    }
    
    // set special objects function
    NCDModuleProcess_SetSpecialFuncs(&p->module_process, (NCDModuleProcess_func_getspecialobj)process_module_process_func_getspecialobj);
    
    // free params
    p->have_params = 0;
    
    // set have module process
    p->have_module_process = 1;
    
    // set state
    p->state = PROCESS_STATE_RUNNING;
}

int process_set_params (struct process *p, NCDValRef template_name, NCDValMem mem, NCDValSafeRef args)
{
    ASSERT(!p->have_params)
    ASSERT(NCDVal_IsString(template_name))
    ASSERT(NCDVal_IsList(NCDVal_FromSafe(&mem, args)))
    
    // get string ID for template name
    p->params_template_name = ncd_get_string_id(template_name, p->manager->i->params->iparams->string_index);
    if (p->params_template_name < 0) {
        ModuleLog(p->manager->i, BLOG_ERROR, "ncd_get_string_id failed");
        return 0;
    }
    
    // eat arguments
    p->params_mem = mem;
    p->params_args = NCDVal_FromSafe(&p->params_mem, args);
    
    // set have params
    p->have_params = 1;
    
    return 1;
}

static void func_new (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    struct instance *o = vo;
    o->i = i;
    
    // check arguments
    if (!NCDVal_ListRead(params->args, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    
    // init processes list
    LinkedList1_Init(&o->processes_list);
    
    // set not dying
    o->dying = 0;
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

void instance_free (struct instance *o)
{
    ASSERT(LinkedList1_IsEmpty(&o->processes_list))
    
    NCDModuleInst_Backend_Dead(o->i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    ASSERT(!o->dying)
    
    // request all processes to die
    LinkedList1Node *n = LinkedList1_GetFirst(&o->processes_list);
    while (n) {
        LinkedList1Node *next = LinkedList1Node_Next(n);
        struct process *p = UPPER_OBJECT(n, struct process, processes_list_node);
        process_stop(p);
        n = next;
    }
    
    // if there are no processes, die immediately
    if (LinkedList1_IsEmpty(&o->processes_list)) {
        instance_free(o);
        return;
    }
    
    // set dying
    o->dying = 1;
}

static void start_func_new (void *unused, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    // check arguments
    NCDValRef name_arg;
    NCDValRef template_name_arg;
    NCDValRef args_arg;
    if (!NCDVal_ListRead(params->args, 3, &name_arg, &template_name_arg, &args_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsStringNoNulls(name_arg) || !NCDVal_IsString(template_name_arg) ||
        !NCDVal_IsList(args_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    const char *name = NCDVal_StringValue(name_arg);
    
    // signal up.
    // Do it before creating the process so that the process starts initializing before our own process continues.
    NCDModuleInst_Backend_Up(i);
    
    // get method object
    struct instance *mo = NCDModuleInst_Backend_GetUser((NCDModuleInst *)params->method_user);
    
    if (mo->dying) {
        ModuleLog(i, BLOG_INFO, "manager is dying, not creating process %s", name);
    } else {
        struct process *p = find_process(mo, name);
        if (p && p->state != PROCESS_STATE_STOPPING) {
            ModuleLog(i, BLOG_INFO, "process %s already started", name);
        } else {
            if (p) {
                if (!process_restart(p, template_name_arg, args_arg)) {
                    ModuleLog(i, BLOG_ERROR, "failed to restart process %s", name);
                    goto fail0;
                }
            } else {
                if (!process_new(mo, name, template_name_arg, args_arg)) {
                    ModuleLog(i, BLOG_ERROR, "failed to create process %s", name);
                    goto fail0;
                }
            }
        }
    }
    
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void stop_func_new (void *unused, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    // check arguments
    NCDValRef name_arg;
    if (!NCDVal_ListRead(params->args, 1, &name_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsStringNoNulls(name_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    const char *name = NCDVal_StringValue(name_arg);
    
    // signal up.
    // Do it before stopping the process so that the process starts terminating before our own process continues.
    NCDModuleInst_Backend_Up(i);
    
    // get method object
    struct instance *mo = NCDModuleInst_Backend_GetUser((NCDModuleInst *)params->method_user);
    
    if (mo->dying) {
        ModuleLog(i, BLOG_INFO, "manager is dying, not stopping process %s", name);
    } else {
        struct process *p = find_process(mo, name);
        if (!(p && p->state != PROCESS_STATE_STOPPING)) {
            ModuleLog(i, BLOG_INFO, "process %s already stopped", name);
        } else {
            process_stop(p);
        }
    }
    
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static struct NCDModule modules[] = {
    {
        .type = "process_manager",
        .func_new2 = func_new,
        .func_die = func_die,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = "process_manager::start",
        .func_new2 = start_func_new
    }, {
        .type = "process_manager::stop",
        .func_new2 = stop_func_new
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_process_manager = {
    .modules = modules,
    .strings = strings
};
