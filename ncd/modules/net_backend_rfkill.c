/**
 * @file net_backend_rfkill.c
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
 * Rfkill monitoring module.
 * 
 * Synopsis: net.backend.rfkill(string type, string name)
 * Arguments:
 *   type - method of determining the index of the rfkill device. "index" for
 *     rfkill device index, "wlan" for wireless device. Be aware that, for
 *     the wireless device method, the index is resloved at initialization,
 *     and no attempt is made to refresh it if the device goes away. In other
 *     words, you should probably put a "net.backend.waitdevice" statement
 *     in front of the rfkill statement.
 *   name - rfkill index or wireless device name
 */

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

#include <misc/string_begins_with.h>
#include <ncd/NCDModule.h>
#include <ncd/NCDRfkillMonitor.h>

#include <generated/blog_channel_ncd_net_backend_rfkill.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    uint32_t index;
    NCDRfkillMonitor monitor;
    int up;
};

static int find_wlan_rfill (const char *ifname, uint32_t *out_index)
{
    char ieee_path[100];
    snprintf(ieee_path, sizeof(ieee_path), "/sys/class/net/%s/../../ieee80211", ifname);
    
    int res = 0;
    
    DIR *d = opendir(ieee_path);
    if (!d) {
        goto fail0;
    }
    
    struct dirent *e;
    while (e = readdir(d)) {
        if (!string_begins_with(e->d_name, "phy")) {
            continue;
        }
        
        char phy_path[150];
        snprintf(phy_path, sizeof(phy_path), "%s/%s", ieee_path, e->d_name);
        
        DIR *d2 = opendir(phy_path);
        if (!d2) {
            continue;
        }
        
        struct dirent *e2;
        while (e2 = readdir(d2)) {
            int index_pos;
            if (!(index_pos = string_begins_with(e2->d_name, "rfkill"))) {
                continue;
            }
            
            uint32_t index;
            if (sscanf(e2->d_name + index_pos, "%"SCNu32, &index) != 1) {
                continue;
            }
            
            res = 1;
            *out_index = index;
        }
        
        closedir(d2);
    }
    
    closedir(d);
fail0:
    return res;
}

static void monitor_handler (struct instance *o, struct rfkill_event event)
{
    if (event.idx != o->index) {
        return;
    }
    
    int was_up = o->up;
    o->up = (event.op != RFKILL_OP_DEL && !event.soft && !event.hard);
    
    if (o->up && !was_up) {
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    }
    else if (!o->up && was_up) {
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_DOWN);
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
    NCDValue *type_arg;
    NCDValue *name_arg;
    if (!NCDValue_ListRead(i->args, 2, &type_arg, &name_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(type_arg) != NCDVALUE_STRING || NCDValue_Type(name_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    char *type = NCDValue_StringValue(type_arg);
    char *name = NCDValue_StringValue(name_arg);
    
    if (!strcmp(type, "index")) {
        if (sscanf(name, "%"SCNu32, &o->index) != 1) {
            ModuleLog(o->i, BLOG_ERROR, "wrong index argument");
            goto fail1;
        }
    }
    else if (!strcmp(type, "wlan")) {
        if (!find_wlan_rfill(name, &o->index)) {
            ModuleLog(o->i, BLOG_ERROR, "failed to find rfkill for wlan interface");
            goto fail1;
        }
    }
    else {
        ModuleLog(o->i, BLOG_ERROR, "unknown type argument");
        goto fail1;
    }
    
    // init monitor
    if (!NCDRfkillMonitor_Init(&o->monitor, o->i->reactor, (NCDRfkillMonitor_handler)monitor_handler, o)) {
        ModuleLog(o->i, BLOG_ERROR, "monitor failed");
        goto fail1;
    }
    
    // set not up
    o->up = 0;
    
    return;
    
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
    
    // free monitor
    NCDRfkillMonitor_Free(&o->monitor);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static const struct NCDModule modules[] = {
    {
        .type = "net.backend.rfkill",
        .func_new = func_new,
        .func_die = func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_backend_rfkill = {
    .modules = modules
};
