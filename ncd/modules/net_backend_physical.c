/**
 * @file net_backend_physical.c
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
 * Physical network interface module.
 * 
 * Synopsis: net.backend.physical(string ifname)
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDModule.h>
#include <ncd/NCDIfConfig.h>
#include <ncd/NCDInterfaceMonitor.h>

#include <generated/blog_channel_ncd_net_backend_physical.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define STATE_WAITDEVICE 1
#define STATE_WAITLINK 2
#define STATE_FINISHED 3

struct instance {
    NCDModuleInst *i;
    const char *ifname;
    NCDInterfaceMonitor monitor;
    int state;
};

static int try_start (struct instance *o)
{
    // query interface state
    int flags = NCDIfConfig_query(o->ifname);
    
    if (!(flags&NCDIFCONFIG_FLAG_EXISTS)) {
        ModuleLog(o->i, BLOG_INFO, "device doesn't exist");
        
        // waiting for device
        o->state = STATE_WAITDEVICE;
    } else {
        if ((flags&NCDIFCONFIG_FLAG_UP)) {
            ModuleLog(o->i, BLOG_ERROR, "device already up - NOT configuring");
            return 0;
        }
        
        // set interface up
        if (!NCDIfConfig_set_up(o->ifname)) {
            ModuleLog(o->i, BLOG_ERROR, "failed to set device up");
            return 0;
        }
        
        ModuleLog(o->i, BLOG_INFO, "waiting for link");
        
        // waiting for link
        o->state = STATE_WAITLINK;
    }
    
    return 1;
}

static void monitor_handler (struct instance *o, const char *ifname, int if_flags)
{
    if (strcmp(ifname, o->ifname)) {
        return;
    }
    
    if (!(if_flags&NCDIFCONFIG_FLAG_EXISTS)) {
        if (o->state > STATE_WAITDEVICE) {
            int prev_state = o->state;
            
            ModuleLog(o->i, BLOG_INFO, "device down");
            
            // set state
            o->state = STATE_WAITDEVICE;
            
            // report
            if (prev_state == STATE_FINISHED) {
                NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_DOWN);
            }
        }
    } else {
        if (o->state == STATE_WAITDEVICE) {
            ModuleLog(o->i, BLOG_INFO, "device up");
            
            if (!try_start(o)) {
                NCDModuleInst_Backend_Died(o->i, 1);
                return;
            }
            
            return;
        }
        
        if ((if_flags&NCDIFCONFIG_FLAG_RUNNING)) {
            if (o->state == STATE_WAITLINK) {
                ModuleLog(o->i, BLOG_INFO, "link up");
                
                // set state
                o->state = STATE_FINISHED;
                
                // report
                NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
            }
        } else {
            if (o->state == STATE_FINISHED) {
                ModuleLog(o->i, BLOG_INFO, "link down");
                
                // set state
                o->state = STATE_WAITLINK;
                
                // report
                NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_DOWN);
            }
        }
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
    
    if (!try_start(o)) {
        goto fail2;
    }
    
    return o;
    
fail2:
    NCDInterfaceMonitor_Free(&o->monitor);
fail1:
    free(o);
fail0:
    return NULL;
}

static void func_free (void *vo)
{
    struct instance *o = vo;
    
    // set interface down
    if (o->state > STATE_WAITDEVICE) {
        NCDIfConfig_set_down(o->ifname);
    }
    
    // free monitor
    NCDInterfaceMonitor_Free(&o->monitor);
    
    // free instance
    free(o);
}

static const struct NCDModule modules[] = {
    {
        .type = "net.backend.physical",
        .func_new = func_new,
        .func_free = func_free
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_backend_physical = {
    .modules = modules
};
