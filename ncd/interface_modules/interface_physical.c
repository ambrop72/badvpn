/**
 * @file interface_physical.c
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
 * Physical interface backend.
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDInterfaceModule.h>
#include <ncd/NCDIfConfig.h>
#include <ncd/NCDInterfaceMonitor.h>

#define STATE_WAITDEVICE 1
#define STATE_WAITLINK 2
#define STATE_FINISHED 3

struct instance {
    NCDInterfaceModuleInst *i;
    NCDInterfaceMonitor monitor;
    int state;
};

static int try_start (struct instance *o)
{
    // query interface state
    int flags = NCDIfConfig_query(o->i->conf->name);
    
    if (!(flags&NCDIFCONFIG_FLAG_EXISTS)) {
        NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_INFO, "device doesn't exist");
        
        // waiting for device
        o->state = STATE_WAITDEVICE;
    } else {
        if ((flags&NCDIFCONFIG_FLAG_UP)) {
            NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_ERROR, "device already up - NOT configuring");
            return 0;
        }
        
        // set interface up
        if (!NCDIfConfig_set_up(o->i->conf->name)) {
            NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_ERROR, "failed to set device up");
            return 0;
        }
        
        NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_INFO, "waiting for link");
        
        // waiting for link
        o->state = STATE_WAITLINK;
    }
    
    return 1;
}

static void monitor_handler (struct instance *o, const char *ifname, int if_flags)
{
    if (strcmp(ifname, o->i->conf->name)) {
        return;
    }
    
    if (!(if_flags&NCDIFCONFIG_FLAG_EXISTS)) {
        if (o->state > STATE_WAITDEVICE) {
            int prev_state = o->state;
            
            NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_INFO, "device down");
            
            // set state
            o->state = STATE_WAITDEVICE;
            
            // report
            if (prev_state == STATE_FINISHED) {
                NCDInterfaceModuleInst_Backend_Event(o->i, NCDINTERFACEMODULE_EVENT_DOWN);
            }
        }
    } else {
        if (o->state == STATE_WAITDEVICE) {
            NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_INFO, "device up");
            
            if (!try_start(o)) {
                NCDInterfaceModuleInst_Backend_Error(o->i);
                return;
            }
            
            return;
        }
        
        if ((if_flags&NCDIFCONFIG_FLAG_RUNNING)) {
            if (o->state == STATE_WAITLINK) {
                NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_INFO, "link up");
                
                // set state
                o->state = STATE_FINISHED;
                
                // report
                NCDInterfaceModuleInst_Backend_Event(o->i, NCDINTERFACEMODULE_EVENT_UP);
            }
        } else {
            if (o->state == STATE_FINISHED) {
                NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_INFO, "link down");
                
                // set state
                o->state = STATE_WAITLINK;
                
                // report
                NCDInterfaceModuleInst_Backend_Event(o->i, NCDINTERFACEMODULE_EVENT_DOWN);
            }
        }
    }
}

static void * func_new (NCDInterfaceModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        NCDInterfaceModuleInst_Backend_Log(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    
    // init arguments
    o->i = i;
    
    // init monitor
    if (!NCDInterfaceMonitor_Init(&o->monitor, o->i->reactor, (NCDInterfaceMonitor_handler)monitor_handler, o)) {
        NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_ERROR, "NCDInterfaceMonitor_Init failed");
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
        NCDIfConfig_set_down(o->i->conf->name);
    }
    
    // free monitor
    NCDInterfaceMonitor_Free(&o->monitor);
    
    // free instance
    free(o);
}

static void func_finish (void *vo)
{
    struct instance *o = vo;
    
    NCDInterfaceModuleInst_Backend_Error(o->i);
    return;
}

const struct NCDInterfaceModule ncd_interface_physical = {
    .type = "physical",
    .func_new = func_new,
    .func_free = func_free,
    .func_finish = func_finish
};
