/**
 * @file net_backend_badvpn.c
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
 * BadVPN interface module.
 * 
 * Synopsis: net.backend.badvpn(string ifname, string user, string exec, list(string) args)
 */

#include <stdlib.h>
#include <string.h>

#include <misc/cmdline.h>
#include <ncd/NCDModule.h>
#include <ncd/NCDIfConfig.h>

#include <generated/blog_channel_ncd_net_backend_badvpn.h>

#define RETRY_TIME 5000

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    char *ifname;
    char *user;
    char *exec;
    NCDValue *args;
    int dying;
    int started;
    BTimer timer;
    BProcess process;
};

static void try_process (struct instance *o);
static void process_handler (struct instance *o, int normally, uint8_t normally_exit_status);
static void timer_handler (struct instance *o);

void try_process (struct instance *o)
{
    CmdLine c;
    if (!CmdLine_Init(&c)) {
        goto fail0;
    }
    
    // append exec
    if (!CmdLine_Append(&c, o->exec)) {
        goto fail1;
    }
    
    // append tapdev
    if (!CmdLine_Append(&c, "--tapdev") || !CmdLine_Append(&c, o->ifname)) {
        goto fail1;
    }
    
    // iterate arguments
    NCDValue *arg = NCDValue_ListFirst(o->args);
    while (arg) {
        if (NCDValue_Type(arg) != NCDVALUE_STRING) {
            ModuleLog(o->i, BLOG_ERROR, "wrong type");
            goto fail1;
        }
        
        // append argument
        if (!CmdLine_Append(&c, NCDValue_StringValue(arg))) {
            goto fail1;
        }
        
        arg = NCDValue_ListNext(o->args, arg);
    }
    
    // terminate cmdline
    if (!CmdLine_Finish(&c)) {
        goto fail1;
    }
    
    // start process
    if (!BProcess_Init(&o->process, o->i->manager, (BProcess_handler)process_handler, o, ((char **)c.arr.v)[0], (char **)c.arr.v, o->user)) {
        ModuleLog(o->i, BLOG_ERROR, "BProcess_Init failed");
        goto fail1;
    }
    
    CmdLine_Free(&c);
    
    // set started
    o->started = 1;
    
    return;
    
fail1:
    CmdLine_Free(&c);
fail0:
    // retry
    o->started = 0;
    BReactor_SetTimer(o->i->reactor, &o->timer);
}

void process_handler (struct instance *o, int normally, uint8_t normally_exit_status)
{
    ASSERT(o->started)
    
    ModuleLog(o->i, BLOG_INFO, "process terminated");
    
    // free process
    BProcess_Free(&o->process);
    
    // set not started
    o->started = 0;
    
    if (o->dying) {
        NCDModuleInst_Backend_Died(o->i, 0);
        return;
    }
    
    // set timer
    BReactor_SetTimer(o->i->reactor, &o->timer);
}

void timer_handler (struct instance *o)
{
    ASSERT(!o->started)
    
    ModuleLog(o->i, BLOG_INFO, "retrying");
    
    try_process(o);
}

static void * func_new (NCDModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    
    // init arguments
    o->i = i;
    
    // read arguments
    NCDValue *ifname_arg;
    NCDValue *user_arg;
    NCDValue *exec_arg;
    NCDValue *args_arg;
    if (!NCDValue_ListRead(o->i->args, 4, &ifname_arg, &user_arg, &exec_arg, &args_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(ifname_arg) != NCDVALUE_STRING || NCDValue_Type(user_arg) != NCDVALUE_STRING ||
        NCDValue_Type(exec_arg) != NCDVALUE_STRING || NCDValue_Type(args_arg) != NCDVALUE_LIST) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->ifname = NCDValue_StringValue(ifname_arg);
    o->user = NCDValue_StringValue(user_arg);
    o->exec = NCDValue_StringValue(exec_arg);
    o->args = args_arg;
    
    // create TAP device
    if (!NCDIfConfig_make_tuntap(o->ifname, o->user, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to create TAP device");
        goto fail1;
    }
    
    // set device up
    if (!NCDIfConfig_set_up(o->ifname)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to set device up");
        goto fail2;
    }
    
    // set not dying
    o->dying = 0;
    
    // init timer
    BTimer_Init(&o->timer, RETRY_TIME, (BTimer_handler)timer_handler, o);
    
    try_process(o);
    
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    return o;
    
fail2:
    if (!NCDIfConfig_remove_tuntap(o->ifname, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to remove TAP device");
    }
fail1:
    free(o);
fail0:
    return NULL;
}

static void func_free (void *vo)
{
    struct instance *o = vo;
    
    if (o->started) {
        // kill process
        BProcess_Kill(&o->process);
        
        // free process
        BProcess_Free(&o->process);
    }
    
    // free timer
    BReactor_RemoveTimer(o->i->reactor, &o->timer);
    
    // free TAP device
    if (!NCDIfConfig_remove_tuntap(o->ifname, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to remove TAP device");
    }
    
    // free instance
    free(o);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    ASSERT(!o->dying)
    
    if (o->started) {
        // request termination
        BProcess_Terminate(&o->process);
        
        // remember dying
        o->dying = 1;
        
        return;
    }
    
    NCDModuleInst_Backend_Died(o->i, 0);
    return;
}

static const struct NCDModule modules[] = {
    {
        .type = "net.backend.badvpn",
        .func_new = func_new,
        .func_free = func_free,
        .func_die = func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_backend_badvpn = {
    .modules = modules
};
