/**
 * @file net_watch_interfaces.c
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
 * Network interface watcher.
 * 
 * Synopsis: net.watch_interfaces()
 * Description: reports network interface events. Transitions up when an event is detected, and
 *   goes down waiting for the next event when net.watch_interfaces::nextevent() is called.
 *   On startup, "added" events are reported for existing interfaces.
 * Variables:
 *   string event_type - what happened with the interface: "added" or "removed". This may not be
 *     consistent across events.
 *   string devname - interface name
 * 
 * Synopsis: net.watch_interfaces::nextevent()
 * Description: makes the watch_interfaces module transition down in order to report the next event.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <ncd/NCDInterfaceMonitor.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_net_watch_interfaces.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    NCDInterfaceMonitor monitor;
    FILE *net_file;
    char line_buf[256];
    int processing;
    const char *processing_type;
    char processing_name[256];
};

struct nextevent_instance {
    NCDModuleInst *i;
};

static void next_file_event (struct instance *o)
{
    ASSERT(!o->processing)
    ASSERT(o->net_file)
    
    char *name;
    
    while (1) {
        if (!fgets(o->line_buf, sizeof(o->line_buf), o->net_file)) {
            // close file
            fclose(o->net_file);
            
            // set no net file
            o->net_file = NULL;
            
            // start processing monitor events
            NCDInterfaceMonitor_Continue(&o->monitor);
            
            return;
        }
        
        // parse line to get interface name
        char *c = o->line_buf;
        while (*c && isspace(*c)) {
            c++;
        }
        name = c;
        while (*c && *c != ':') {
            c++;
        }
        if (*c != ':') {
            continue;
        }
        *c = '\0';
        
        break;
    }
    
    // set event
    o->processing_type = "added";
    snprintf(o->processing_name, sizeof(o->processing_name), "%s", name);
    o->processing = 1;
    
    // signal up
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
}

static void next_event (struct instance *o)
{
    ASSERT(o->processing)
    
    // set not processing
    o->processing = 0;
    
    // signal down
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_DOWN);
    
    if (o->net_file) {
        next_file_event(o);
        return;
    } else {
        // continue processing monitor events
        NCDInterfaceMonitor_Continue(&o->monitor);
    }
}

static void monitor_handler (struct instance *o, const char *ifname, int if_flags)
{
    ASSERT(!o->processing)
    ASSERT(!o->net_file)
    
    // pause monitor
    NCDInterfaceMonitor_Pause(&o->monitor);
    
    // set event
    o->processing_type = ((if_flags & NCDIFCONFIG_FLAG_EXISTS) ? "added" : "removed");
    snprintf(o->processing_name, sizeof(o->processing_name), "%s", ifname);
    o->processing = 1;
    
    // signal up
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
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
    
    // init monitor
    if (!NCDInterfaceMonitor_Init(&o->monitor, o->i->reactor, (NCDInterfaceMonitor_handler)monitor_handler, o)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDInterfaceMonitor_Init failed");
        goto fail1;
    }
    NCDInterfaceMonitor_Pause(&o->monitor);
    
    // open /proc/net/dev
    if (!(o->net_file = fopen("/proc/net/dev", "r"))) {
        ModuleLog(o->i, BLOG_ERROR, "fopen(/proc/net/dev) failed");
        goto fail2;
    }
    
    // set not processing
    o->processing = 0;
    
    next_file_event(o);
    return;
    
fail2:
    NCDInterfaceMonitor_Free(&o->monitor);
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // close /proc/net/dev
    if (o->net_file) {
        fclose(o->net_file);
    }
    
    // free monitor
    NCDInterfaceMonitor_Free(&o->monitor);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    ASSERT(o->processing)
    
    if (!strcmp(name, "event_type")) {
        if (!NCDValue_InitString(out, o->processing_type)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    if (!strcmp(name, "devname")) {
        if (!NCDValue_InitString(out, o->processing_name)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static void nextevent_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct nextevent_instance *o = malloc(sizeof(*o));
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
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    ASSERT(mo->processing)
    
    // signal up.
    // Do it before finishing the event so our process does not advance any further if
    // we would be killed the event provider going down.
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    // wait for next event
    next_event(mo);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void nextevent_func_die (void *vo)
{
    struct nextevent_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static const struct NCDModule modules[] = {
    {
        .type = "net.watch_interfaces",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "net.watch_interfaces::nextevent",
        .func_new = nextevent_func_new,
        .func_die = nextevent_func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_watch_interfaces = {
    .modules = modules
};
