/**
 * @file net_backend_waitdevice.c
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
 * Module which waits for the presence of a network interface.
 * 
 * Synopsis: net.backend.waitdevice(string ifname)
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDModule.h>
#include <ncd/NCDIfConfig.h>
#include <ncd/NCDInterfaceMonitor.h>

#include <generated/blog_channel_ncd_net_backend_waitdevice.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    const char *ifname;
    NCDInterfaceMonitor monitor;
    int up;
};

static void monitor_handler (struct instance *o, const char *ifname, int if_flags)
{
    if (strcmp(ifname, o->ifname)) {
        return;
    }
    
    int was_up = o->up;
    o->up = !!(if_flags & NCDIFCONFIG_FLAG_EXISTS);
    
    if (o->up && !was_up) {
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    }
    else if (!o->up && was_up) {
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_DOWN);
    }
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
    
    // check arguments
    NCDValue *arg;
    if (!NCDValue_ListRead(i->args, 1, &arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->ifname = NCDValue_StringValue(arg);
    
    // init monitor
    if (!NCDInterfaceMonitor_Init(&o->monitor, o->i->reactor, (NCDInterfaceMonitor_handler)monitor_handler, o)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDInterfaceMonitor_Init failed");
        goto fail1;
    }
    
    // query initial state
    o->up = !!(NCDIfConfig_query(o->ifname) & NCDIFCONFIG_FLAG_EXISTS);
    
    if (o->up) {
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    }
    
    return o;
    
fail1:
    free(o);
fail0:
    return NULL;
}

static void func_free (void *vo)
{
    struct instance *o = vo;
    
    // free monitor
    NCDInterfaceMonitor_Free(&o->monitor);
    
    // free instance
    free(o);
}

static const struct NCDModule modules[] = {
    {
        .type = "net.backend.waitdevice",
        .func_new = func_new,
        .func_free = func_free
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_backend_waitdevice = {
    .modules = modules
};
