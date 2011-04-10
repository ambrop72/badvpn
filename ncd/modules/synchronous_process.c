/**
 * @file synchronous_process.c
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
 * Module which starts a process from a process template on initialization, and
 * stops it on deinitialization.
 * 
 * Synopsis: synchronous_process(string template_name, list(string) args)
 * Description: on initialization, creates a new process from the template named
 *   template_name, with arguments args. On deinitialization, initiates termination
 *   of the process and waits for it to terminate.
 */

#include <stdlib.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_synchronous_process.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    NCDModuleProcess process;
};

static void instance_free (struct instance *o);

static void process_handler_dead (struct instance *o)
{
    // die now
    instance_free(o);
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
    
    // signal up.
    // Do it before creating the process so that the process starts initializing before our own process continues.
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    // copy arguments
    NCDValue args;
    if (!NCDValue_InitCopy(&args, args_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail1;
    }
    
    // create process
    if (!NCDModuleProcess_Init(&o->process, o->i, NCDValue_StringValue(template_name_arg), args, o, (NCDModuleProcess_handler_dead)process_handler_dead)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDModuleProcess_Init failed");
        NCDValue_Free(&args);
        goto fail1;
    }
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

void instance_free (struct instance *o)
{
    NCDModuleInst *i = o->i;
    
    // free process
    NCDModuleProcess_Free(&o->process);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    
    // request process to die
    NCDModuleProcess_Die(&o->process);
}

static const struct NCDModule modules[] = {
    {
        .type = "synchronous_process",
        .func_new = func_new,
        .func_die = func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_synchronous_process = {
    .modules = modules
};
