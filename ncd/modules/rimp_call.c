/**
 * @file rimp_call.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * This file is part of BadVPN.
 * 
 * BadVPN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * BadVPN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * @section DESCRIPTION
 * 
 * Reverse imperative call.
 * 
 * Synopsis:
 *   rimp_call(string template_name, list args)
 *   rimp_call_timeout(string template_name, list args, string timeout_ms)
 * 
 * Description:
 *   Goes up immediately. On deinitialization, does the following, in order:
 *     1. starts a template process from the given template and arguments
 *        and waits for it to completely initialize, or for the timeout to
 *        elapse, then
 *     2. requests termination of the process and waits for it to terminate,
 *        then finally
 *     3. deinitializes.
 * 
 *   WARNING: if there's a bug in the NCD program and the started template
 *            process never initializes completely, rimp_call() will never
 *            terminate, and you will have to kill the NCD process by force.
 */

#include <stdlib.h>

#include <misc/parse_number.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_rimp_call.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define STATE_UP 1
#define STATE_WORKING 2
#define STATE_CLEANING 3

struct instance {
    NCDModuleInst *i;
    char *template_name;
    NCDValue *args;
    int have_timeout;
    BTimer timer;
    NCDModuleProcess process;
    int state;
};

static void instance_free (struct instance *o);

static void process_handler_event (struct instance *o, int event)
{
    switch (event) {
        case NCDMODULEPROCESS_EVENT_UP: {
            ASSERT(o->state == STATE_WORKING)
            
            // stop timer
            if (o->have_timeout) {
                BReactor_RemoveTimer(o->i->reactor, &o->timer);
            }
            
            // start terminating
            NCDModuleProcess_Terminate(&o->process);
            
            // set state cleaning
            o->state = STATE_CLEANING;
        } break;
        
        case NCDMODULEPROCESS_EVENT_TERMINATED: {
            ASSERT(o->state == STATE_CLEANING)
            
            // free process
            NCDModuleProcess_Free(&o->process);
            
            // free instance
            instance_free(o);
            return;
        } break;
        
        default: ASSERT(0);
    }
}

static void timer_handler (struct instance *o)
{
    ASSERT(o->have_timeout)
    ASSERT(o->state == STATE_WORKING)
    
    ModuleLog(o->i, BLOG_ERROR, "rimp_call timeout elapsed");
    
    // start terminating
    NCDModuleProcess_Terminate(&o->process);
    
    // set state cleaning
    o->state = STATE_CLEANING;
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
    if (!NCDValue_ListRead(i->args, 2, &template_name_arg, &o->args)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(template_name_arg) != NCDVALUE_STRING || NCDValue_Type(o->args) != NCDVALUE_LIST) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->template_name = NCDValue_StringValue(template_name_arg);
    
    // set have no timeout
    o->have_timeout = 0;
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    
    // set state up
    o->state = STATE_UP;
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_timeout (NCDModuleInst *i)
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
    NCDValue *timeout_arg;
    if (!NCDValue_ListRead(i->args, 3, &template_name_arg, &o->args, &timeout_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(template_name_arg) != NCDVALUE_STRING || NCDValue_Type(o->args) != NCDVALUE_LIST ||
        NCDValue_Type(timeout_arg) != NCDVALUE_STRING) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->template_name = NCDValue_StringValue(template_name_arg);
    
    // parse timeout
    uintmax_t timeout;
    if (!parse_unsigned_integer(NCDValue_StringValue(timeout_arg), &timeout) || timeout > UINT64_MAX) {
        ModuleLog(i, BLOG_ERROR, "wrong timeout");
        goto fail1;
    }
    
    // set have timeout
    o->have_timeout = 1;
    
    // init timer
    BTimer_Init(&o->timer, timeout, (BTimer_handler)timer_handler, o);
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    
    // set state up
    o->state = STATE_UP;
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
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    ASSERT(o->state == STATE_UP)
    
    // copy arguments
    NCDValue args;
    if (!NCDValue_InitCopy(&args, o->args)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail;
    }
    
    // create process
    if (!NCDModuleProcess_Init(&o->process, o->i, o->template_name, args, o, (NCDModuleProcess_handler_event)process_handler_event)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDModuleProcess_Init failed");
        NCDValue_Free(&args);
        goto fail;
    }
    
    // start timer if used
    if (o->have_timeout) {
        BReactor_SetTimer(o->i->reactor, &o->timer);
    }
    
    // set state working
    o->state = STATE_WORKING;
    return;
    
fail:
    instance_free(o);
}

static const struct NCDModule modules[] = {
    {
        .type = "rimp_call",
        .func_new = func_new,
        .func_die = func_die
    }, {
        .type = "rimp_call_timeout",
        .func_new = func_new_timeout,
        .func_die = func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_rimp_call = {
    .modules = modules
};
