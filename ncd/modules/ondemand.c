/**
 * @file ondemand.c
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
 * On-demand process manager.
 * 
 * Synopsis:
 *   ondemand(string template_name, list args)
 * 
 * Description:
 *   Manages an on-demand template process using a process template named
 *   template_name.
 *   On deinitialization, if the process is running, reqests its termination
 *   and waits for it to terminate.
 * 
 * Synopsis:
 *   ondemand::demand()
 * 
 * Description:
 *   Demands the availability of an on-demand template process.
 *   This statement is in UP state if and only if the template process of the
 *   corresponding ondemand object is completely up.
 * 
 * Variables:
 *   Exposes variables and objects from the template process corresponding to
 *   the ondemand object.
 */

#include <stdlib.h>
#include <string.h>

#include <misc/offset.h>
#include <misc/debug.h>
#include <structure/LinkedList1.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_ondemand.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct ondemand {
    NCDModuleInst *i;
    char *template_name;
    NCDValue *args;
    LinkedList1 demands_list;
    int dying;
    int have_process;
    NCDModuleProcess process;
    int process_terminating;
    int process_up;
};

struct demand {
    NCDModuleInst *i;
    struct ondemand *od;
    LinkedList1Node demands_list_node;
};

static int ondemand_start_process (struct ondemand *o);
static void ondemand_terminate_process (struct ondemand *o);
static void ondemand_process_handler (struct ondemand *o, int event);
static void ondemand_free (struct ondemand *o);
static void demand_free (struct demand *o);

static int ondemand_start_process (struct ondemand *o)
{
    ASSERT(!o->dying)
    ASSERT(!o->have_process)
    
    // copy arguments
    NCDValue args;
    if (!NCDValue_InitCopy(&args, o->args)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
        goto fail0;
    }
    
    // start process
    if (!NCDModuleProcess_Init(&o->process, o->i, o->template_name, args, o, (NCDModuleProcess_handler_event)ondemand_process_handler)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDModuleProcess_Init failed");
        NCDValue_Free(&args);
        goto fail0;
    }
    
    // set have process
    o->have_process = 1;
    
    // set process not terminating
    o->process_terminating = 0;
    
    // set process not up
    o->process_up = 0;
    
    return 1;
    
fail0:
    return 0;
}

static void ondemand_terminate_process (struct ondemand *o)
{
    ASSERT(o->have_process)
    ASSERT(!o->process_terminating)
    
    // request termination
    NCDModuleProcess_Terminate(&o->process);
    
    // set process terminating
    o->process_terminating = 1;
    
    if (o->process_up) {
        // set process down
        o->process_up = 0;
        
        // signal demands down
        for (LinkedList1Node *n = LinkedList1_GetFirst(&o->demands_list); n; n = LinkedList1Node_Next(n)) {
            struct demand *demand = UPPER_OBJECT(n, struct demand, demands_list_node);
            ASSERT(demand->od == o)
            NCDModuleInst_Backend_Down(demand->i);
        }
    }
}

static void ondemand_process_handler (struct ondemand *o, int event)
{
    ASSERT(o->have_process)
    
    switch (event) {
        case NCDMODULEPROCESS_EVENT_UP: {
            ASSERT(!o->process_terminating)
            ASSERT(!o->process_up)
            
            // set process up
            o->process_up = 1;
            
            // signal demands up
            for (LinkedList1Node *n = LinkedList1_GetFirst(&o->demands_list); n; n = LinkedList1Node_Next(n)) {
                struct demand *demand = UPPER_OBJECT(n, struct demand, demands_list_node);
                ASSERT(demand->od == o)
                NCDModuleInst_Backend_Up(demand->i);
            }
        } break;
        
        case NCDMODULEPROCESS_EVENT_DOWN: {
            ASSERT(!o->process_terminating)
            ASSERT(o->process_up)
            
            // continue process
            NCDModuleProcess_Continue(&o->process);
            
            // set process down
            o->process_up = 0;
            
            // signal demands down
            for (LinkedList1Node *n = LinkedList1_GetFirst(&o->demands_list); n; n = LinkedList1Node_Next(n)) {
                struct demand *demand = UPPER_OBJECT(n, struct demand, demands_list_node);
                ASSERT(demand->od == o)
                NCDModuleInst_Backend_Down(demand->i);
            }
        } break;
        
        case NCDMODULEPROCESS_EVENT_TERMINATED: {
            ASSERT(o->process_terminating)
            ASSERT(!o->process_up)
            
            // free process
            NCDModuleProcess_Free(&o->process);
            
            // set have no process
            o->have_process = 0;
            
            // if dying, die finally
            if (o->dying) {
                ondemand_free(o);
                return;
            }
            
            // if demands arrivied, restart process
            if (!LinkedList1_IsEmpty(&o->demands_list)) {
                if (!ondemand_start_process(o)) {
                    // error demands
                    while (!LinkedList1_IsEmpty(&o->demands_list)) {
                        struct demand *demand = UPPER_OBJECT(LinkedList1_GetFirst(&o->demands_list), struct demand, demands_list_node);
                        ASSERT(demand->od == o)
                        NCDModuleInst_Backend_SetError(demand->i);
                        demand_free(demand);
                    }
                }
            }
        } break;
    }
}

static void ondemand_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct ondemand *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // read arguments
    NCDValue *arg_template_name;
    NCDValue *arg_args;
    if (!NCDValue_ListRead(i->args, 2, &arg_template_name, &arg_args)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(arg_template_name) != NCDVALUE_STRING || NCDValue_Type(arg_args) != NCDVALUE_LIST) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->template_name = NCDValue_StringValue(arg_template_name);
    o->args = arg_args;
    
    // init demands list
    LinkedList1_Init(&o->demands_list);
    
    // set not dying
    o->dying = 0;
    
    // set have no process
    o->have_process = 0;
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void ondemand_free (struct ondemand *o)
{
    ASSERT(!o->have_process)
    NCDModuleInst *i = o->i;
    
    // die demands
    while (!LinkedList1_IsEmpty(&o->demands_list)) {
        struct demand *demand = UPPER_OBJECT(LinkedList1_GetFirst(&o->demands_list), struct demand, demands_list_node);
        ASSERT(demand->od == o)
        demand_free(demand);
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void ondemand_func_die (void *vo)
{
    struct ondemand *o = vo;
    ASSERT(!o->dying)
    
    // if not have process, die right away
    if (!o->have_process) {
        ondemand_free(o);
        return;
    }
    
    // set dying
    o->dying = 1;
    
    // request process termination if not already
    if (!o->process_terminating) {
        ondemand_terminate_process(o);
    }
}

static void demand_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct demand *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // read arguments
    if (!NCDValue_ListRead(i->args, 0)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // set ondemand
    o->od = i->method_object->inst_user;
    
    // add to ondemand's demands list
    LinkedList1_Append(&o->od->demands_list, &o->demands_list_node);
    
    // start process if needed
    if (!o->od->have_process) {
        ASSERT(!o->od->dying)
        
        if (!ondemand_start_process(o->od)) {
            goto fail2;
        }
    }
    
    // if process is up, signal up
    if (o->od->process_up) {
        NCDModuleInst_Backend_Up(i);
    }
    
    return;
    
fail2:
    LinkedList1_Remove(&o->od->demands_list, &o->demands_list_node);
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void demand_free (struct demand *o)
{
    NCDModuleInst *i = o->i;
    
    // remove from ondemand's demands list
    LinkedList1_Remove(&o->od->demands_list, &o->demands_list_node);
    
    // request process termination if no longer needed
    if (o->od->have_process && !o->od->process_terminating && LinkedList1_IsEmpty(&o->od->demands_list)) {
        ondemand_terminate_process(o->od);
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void demand_func_die (void *vo)
{
    struct demand *o = vo;
    
    demand_free(o);
}

static int demand_func_getvar (void *vo, const char *varname, NCDValue *out)
{
    struct demand *o = vo;
    ASSERT(o->od->have_process)
    ASSERT(o->od->process_up)
    
    return NCDModuleProcess_GetVar(&o->od->process, varname, out);
}

static NCDModuleInst * demand_func_getobj (void *vo, const char *objname)
{
    struct demand *o = vo;
    ASSERT(o->od->have_process)
    ASSERT(o->od->process_up)
    
    return NCDModuleProcess_GetObj(&o->od->process, objname);
}

static const struct NCDModule modules[] = {
    {
        .type = "ondemand",
        .func_new = ondemand_func_new,
        .func_die = ondemand_func_die
    }, {
        .type = "ondemand::demand",
        .func_new = demand_func_new,
        .func_die = demand_func_die,
        .func_getvar = demand_func_getvar,
        .func_getobj = demand_func_getobj
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_ondemand = {
    .modules = modules
};
