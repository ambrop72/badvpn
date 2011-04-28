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

#include <misc/debug.h>
#include <misc/offset.h>
#include <misc/parse_number.h>
#include <structure/LinkedList1.h>
#include <udevmonitor/NCDUdevManager.h>
#include <ncd/NCDModule.h>
#include <ncd/modules/event_template.h>

#include <generated/blog_channel_ncd_net_watch_interfaces.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct device {
    char *ifname;
    char *devpath;
    uintmax_t ifindex;
    LinkedList1Node devices_list_node;
};

struct instance {
    NCDModuleInst *i;
    NCDUdevClient client;
    LinkedList1 devices_list;
    event_template templ;
};

struct nextevent_instance {
    NCDModuleInst *i;
};

static void templ_func_free (struct instance *o);

struct device * find_device_by_ifname (struct instance *o, const char *ifname)
{
    LinkedList1Node *list_node = LinkedList1_GetFirst(&o->devices_list);
    while (list_node) {
        struct device *device = UPPER_OBJECT(list_node, struct device, devices_list_node);
        if (!strcmp(device->ifname, ifname)) {
            return device;
        }
        list_node = LinkedList1Node_Next(list_node);
    }
    
    return NULL;
}

struct device * find_device_by_devpath (struct instance *o, const char *devpath)
{
    LinkedList1Node *list_node = LinkedList1_GetFirst(&o->devices_list);
    while (list_node) {
        struct device *device = UPPER_OBJECT(list_node, struct device, devices_list_node);
        if (!strcmp(device->devpath, devpath)) {
            return device;
        }
        list_node = LinkedList1Node_Next(list_node);
    }
    
    return NULL;
}

static void free_device (struct instance *o, struct device *device)
{
    // remove from devices list
    LinkedList1_Remove(&o->devices_list, &device->devices_list_node);
    
    // free devpath
    free(device->devpath);
    
    // free ifname
    free(device->ifname);
    
    // free structure
    free(device);
}

static int queue_event (struct instance *o, int added, const char *ifname)
{
    // init map
    BStringMap map;
    BStringMap_Init(&map);
    
    // set type
    if (!BStringMap_Set(&map, "event_type", (added ? "added" : "removed"))) {
        ModuleLog(o->i, BLOG_ERROR, "BStringMap_Set failed");
        goto fail1;
    }
    
    // set ifname
    if (!BStringMap_Set(&map, "devname", ifname)) {
        ModuleLog(o->i, BLOG_ERROR, "BStringMap_Set failed");
        goto fail1;
    }
    
    // pass event to template
    int was_empty;
    if (!event_template_queue(&o->templ, map, &was_empty)) {
        ModuleLog(o->i, BLOG_ERROR, "event_template_queue failed");
        goto fail1;
    }
    
    // if event queue was empty, stop receiving udev events
    if (was_empty) {
        NCDUdevClient_Pause(&o->client);
    }
    
    return 1;
    
fail1:
    BStringMap_Free(&map);
    ModuleLog(o->i, BLOG_ERROR, "failed to queue event");
    return 0;
}

static void add_device (struct instance *o, const char *ifname, const char *devpath, uintmax_t ifindex)
{
    ASSERT(!find_device_by_ifname(o, ifname))
    ASSERT(!find_device_by_devpath(o, devpath))
    
    // allocate structure
    struct device *device = malloc(sizeof(*device));
    if (!device) {
        ModuleLog(o->i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    
    // init ifname
    if (!(device->ifname = strdup(ifname))) {
        ModuleLog(o->i, BLOG_ERROR, "strdup failed");
        goto fail1;
    }
    
    // init devpath
    if (!(device->devpath = strdup(devpath))) {
        ModuleLog(o->i, BLOG_ERROR, "strdup failed");
        goto fail2;
    }
    
    // set ifindex
    device->ifindex = ifindex;
    
    // insert to devices list
    LinkedList1_Append(&o->devices_list, &device->devices_list_node);
    
    // queue event
    if (!queue_event(o, 1, device->ifname)) {
        goto fail3;
    }
    
    return;
    
fail3:
    LinkedList1_Remove(&o->devices_list, &device->devices_list_node);
    free(device->devpath);
fail2:
    free(device->ifname);
fail1:
    free(device);
fail0:
    ModuleLog(o->i, BLOG_ERROR, "failed to add device %s", ifname);
}

static int remove_device (struct instance *o, struct device *device)
{
    if (!queue_event(o, 0, device->ifname)) {
        goto fail0;
    }
    
    free_device(o, device);
    
    return 1;
    
fail0:
    ModuleLog(o->i, BLOG_ERROR, "failed to remove device %s", device->ifname);
    return 0;
}

static void next_event (struct instance *o)
{
    event_template_assert_enabled(&o->templ);
    
    // order template to finish the current event
    int is_empty;
    event_template_next(&o->templ, &is_empty);
    
    // if template has no events, continue udev events
    if (is_empty) {
        NCDUdevClient_Continue(&o->client);
    }
}

static void client_handler (struct instance *o, char *devpath, int have_map, BStringMap map)
{
    // lookup existing device with this devpath
    struct device *ex_device = find_device_by_devpath(o, devpath);
    // lookup cache entry
    const BStringMap *cache_map = NCDUdevManager_Query(o->i->umanager, devpath);
    
    if (!cache_map) {
        if (ex_device) {
            remove_device(o, ex_device);
        }
        goto out;
    }
    
    const char *subsystem = BStringMap_Get(cache_map, "SUBSYSTEM");
    const char *interface = BStringMap_Get(cache_map, "INTERFACE");
    const char *ifindex_str = BStringMap_Get(cache_map, "IFINDEX");
    
    uintmax_t ifindex;
    if (!(subsystem && !strcmp(subsystem, "net") && interface && ifindex_str && parse_unsigned_integer(ifindex_str, &ifindex))) {
        if (ex_device) {
            remove_device(o, ex_device);
        }
        goto out;
    }
    
    if (ex_device && (strcmp(ex_device->ifname, interface) || ex_device->ifindex != ifindex)) {
        if (!remove_device(o, ex_device)) {
            goto out;
        }
        ex_device = NULL;
    }
    
    if (!ex_device) {
        struct device *ex_ifname_device = find_device_by_ifname(o, interface);
        if (ex_ifname_device) {
            if (!remove_device(o, ex_ifname_device)) {
                goto out;
            }
        }
        
        add_device(o, interface, devpath, ifindex);
    }
    
out:
    free(devpath);
    if (have_map) {
        BStringMap_Free(&map);
    }
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
    
    // init client
    NCDUdevClient_Init(&o->client, o->i->umanager, o, (NCDUdevClient_handler)client_handler);
    
    // init devices list
    LinkedList1_Init(&o->devices_list);
    
    event_template_new(&o->templ, o->i, BLOG_CURRENT_CHANNEL, o, (event_template_func_free)templ_func_free);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void templ_func_free (struct instance *o)
{
    NCDModuleInst *i = o->i;
    
    // free devices
    LinkedList1Node *list_node;
    while (list_node = LinkedList1_GetFirst(&o->devices_list)) {
        struct device *device = UPPER_OBJECT(list_node, struct device, devices_list_node);
        free_device(o, device);
    }
    
    // free client
    NCDUdevClient_Free(&o->client);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    event_template_die(&o->templ);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    return event_template_getvar(&o->templ, name, out);
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
    event_template_assert_enabled(&mo->templ);
    
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
