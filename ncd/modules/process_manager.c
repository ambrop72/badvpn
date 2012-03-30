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
#include <structure/LinkedList2.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_process_manager.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define RETRY_TIME 10000

#define PROCESS_STATE_RUNNING 1
#define PROCESS_STATE_STOPPING 2
#define PROCESS_STATE_RESTARTING 3
#define PROCESS_STATE_RETRYING 4

struct instance {
    NCDModuleInst *i;
    LinkedList2 processes_list;
    int dying;
};

struct process {
    struct instance *manager;
    char *name;
    BTimer retry_timer;
    LinkedList2Node processes_list_node;
    int have_params;
    char *params_template_name;
    NCDValue params_args;
    int have_module_process;
    NCDModuleProcess module_process;
    int state;
};

static struct process * find_process (struct instance *o, const char *name);
static int process_new (struct instance *o, const char *name, const char *template_name, NCDValue *args);
static void process_free (struct process *p);
static void process_retry_timer_handler (struct process *p);
static void process_module_process_handler_event (struct process *p, int event);
static int process_module_process_func_getspecialobj (struct process *p, const char *name, NCDObject *out_object);
static int process_module_process_caller_obj_func_getobj (struct process *p, const char *name, NCDObject *out_object);
static void process_stop (struct process *p);
static int process_restart (struct process *p, const char *template_name, NCDValue *args);
static void process_try (struct process *p);
static int process_set_params (struct process *p, const char *template_name, NCDValue args);
static void instance_free (struct instance *o);

struct process * find_process (struct instance *o, const char *name)
{
    LinkedList2Node *n = LinkedList2_GetFirst(&o->processes_list);
    while (n) {
        struct process *p = UPPER_OBJECT(n, struct process, processes_list_node);
        if (!strcmp(p->name, name)) {
            return p;
        }
        n = LinkedList2Node_Next(n);
    }
    
    return NULL;
}

int process_new (struct instance *o, const char *name, const char *template_name, NCDValue *args)
{
    ASSERT(!o->dying)
    ASSERT(!find_process(o, name))
    ASSERT(NCDValue_Type(args) == NCDVALUE_LIST)
    
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
    LinkedList2_Append(&o->processes_list, &p->processes_list_node);
    
    // have no params
    p->have_params = 0;
    
    // have no module process
    p->have_module_process = 0;
    
    // copy arguments
    NCDValue args2;
    if (!NCDValue_InitCopy(&args2, args)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail2;
    }
    
    // set params
    if (!process_set_params(p, template_name, args2)) {
        NCDValue_Free(&args2);
        goto fail2;
    }
    
    // try starting it
    process_try(p);
    
    return 1;
    
fail2:
    LinkedList2_Remove(&o->processes_list, &p->processes_list_node);
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
        NCDValue_Free(&p->params_args);
        free(p->params_template_name);
    }
    
    // remove from processes list
    LinkedList2_Remove(&o->processes_list, &p->processes_list_node);
    
    // free timer
    BReactor_RemoveTimer(o->i->params->reactor, &p->retry_timer);
    
    // free name
    free(p->name);
    
    // free structure
    free(p);
}

void process_retry_timer_handler (struct process *p)
{
    struct instance *o = p->manager;
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
    
    // set no module process
    p->have_module_process = 0;
    
    switch (p->state) {
        case PROCESS_STATE_STOPPING: {
            // free process
            process_free(p);
        
            // if manager is dying and there are no more processes, let it die
            if (o->dying && LinkedList2_IsEmpty(&o->processes_list)) {
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

int process_module_process_func_getspecialobj (struct process *p, const char *name, NCDObject *out_object)
{
    ASSERT(p->have_module_process)
    
    if (!strcmp(name, "_caller")) {
        *out_object = NCDObject_Build(NULL, p, NULL, (NCDObject_func_getobj)process_module_process_caller_obj_func_getobj);
        return 1;
    }
    
    return 0;
}

int process_module_process_caller_obj_func_getobj (struct process *p, const char *name, NCDObject *out_object)
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
            NCDValue_Free(&p->params_args);
            free(p->params_template_name);
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

int process_restart (struct process *p, const char *template_name, NCDValue *args)
{
    struct instance *o = p->manager;
    ASSERT(!o->dying)
    ASSERT(p->state == PROCESS_STATE_STOPPING)
    ASSERT(!p->have_params)
    
    // copy arguments
    NCDValue args2;
    if (!NCDValue_InitCopy(&args2, args)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        return 0;
    }
    
    // set params
    if (!process_set_params(p, template_name, args2)) {
        NCDValue_Free(&args2);
        return 0;
    }
    
    // set state
    p->state = PROCESS_STATE_RESTARTING;
    
    return 1;
}

void process_try (struct process *p)
{
    struct instance *o = p->manager;
    ASSERT(!o->dying)
    ASSERT(p->have_params)
    ASSERT(!p->have_module_process)
    
    ModuleLog(o->i, BLOG_INFO, "trying process %s", p->name);
    
    // init module process
    if (!NCDModuleProcess_Init(&p->module_process, o->i, p->params_template_name, p->params_args, p, (NCDModuleProcess_handler_event)process_module_process_handler_event)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDModuleProcess_Init failed");
        
        // set timer
        BReactor_SetTimer(o->i->params->reactor, &p->retry_timer);
        
        // set state
        p->state = PROCESS_STATE_RETRYING;
        
        return;
    }
    
    // set special objects function
    NCDModuleProcess_SetSpecialFuncs(&p->module_process, (NCDModuleProcess_func_getspecialobj)process_module_process_func_getspecialobj);
    
    // free params
    free(p->params_template_name);
    p->have_params = 0;
    
    // set have module process
    p->have_module_process = 1;
    
    // set state
    p->state = PROCESS_STATE_RUNNING;
}

int process_set_params (struct process *p, const char *template_name, NCDValue args)
{
    ASSERT(!p->have_params)
    
    // copy template name
    if (!(p->params_template_name = strdup(template_name))) {
        ModuleLog(p->manager->i, BLOG_ERROR, "strdup failed");
        return 0;
    }
    
    // eat arguments
    p->params_args = args;
    
    // set have params
    p->have_params = 1;
    
    return 1;
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
    if (!NCDValue_ListRead(o->i->args, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // init processes list
    LinkedList2_Init(&o->processes_list);
    
    // set not dying
    o->dying = 0;
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

void instance_free (struct instance *o)
{
    ASSERT(LinkedList2_IsEmpty(&o->processes_list))
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    ASSERT(!o->dying)
    
    // request all processes to die
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &o->processes_list);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        struct process *p = UPPER_OBJECT(n, struct process, processes_list_node);
        process_stop(p);
    }
    
    // if there are no processes, die immediately
    if (LinkedList2_IsEmpty(&o->processes_list)) {
        instance_free(o);
        return;
    }
    
    // set dying
    o->dying = 1;
}

static void start_func_new (NCDModuleInst *i)
{
    // check arguments
    NCDValue *name_arg;
    NCDValue *template_name_arg;
    NCDValue *args_arg;
    if (!NCDValue_ListRead(i->args, 3, &name_arg, &template_name_arg, &args_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (NCDValue_Type(name_arg) != NCDVALUE_STRING || NCDValue_Type(template_name_arg) != NCDVALUE_STRING ||
        NCDValue_Type(args_arg) != NCDVALUE_LIST) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    char *name = NCDValue_StringValue(name_arg);
    char *template_name = NCDValue_StringValue(template_name_arg);
    
    // signal up.
    // Do it before creating the process so that the process starts initializing before our own process continues.
    NCDModuleInst_Backend_Up(i);
    
    // get method object
    struct instance *mo = ((NCDModuleInst *)i->method_user)->inst_user;
    
    if (mo->dying) {
        ModuleLog(i, BLOG_INFO, "manager is dying, not creating process %s", name);
    } else {
        struct process *p = find_process(mo, name);
        if (p && p->state != PROCESS_STATE_STOPPING) {
            ModuleLog(i, BLOG_INFO, "process %s already started", name);
        } else {
            if (p) {
                if (!process_restart(p, template_name, args_arg)) {
                    ModuleLog(i, BLOG_ERROR, "failed to restart process %s", name);
                    goto fail0;
                }
            } else {
                if (!process_new(mo, name, template_name, args_arg)) {
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

static void stop_func_new (NCDModuleInst *i)
{
    // check arguments
    NCDValue *name_arg;
    if (!NCDValue_ListRead(i->args, 1, &name_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (NCDValue_Type(name_arg) != NCDVALUE_STRING) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    char *name = NCDValue_StringValue(name_arg);
    
    // signal up.
    // Do it before stopping the process so that the process starts terminating before our own process continues.
    NCDModuleInst_Backend_Up(i);
    
    // get method object
    struct instance *mo = ((NCDModuleInst *)i->method_user)->inst_user;
    
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

static const struct NCDModule modules[] = {
    {
        .type = "process_manager",
        .func_new = func_new,
        .func_die = func_die
    }, {
        .type = "process_manager::start",
        .func_new = start_func_new
    }, {
        .type = "process_manager::stop",
        .func_new = stop_func_new
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_process_manager = {
    .modules = modules
};
