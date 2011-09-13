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
 * Description: statement is UP when a network interface named ifname
 *   exists, and DOWN when it does not.
 */

#include <stdlib.h>
#include <string.h>

#include <misc/parse_number.h>
#include <ncd/NCDModule.h>
#include <ncd/NCDIfConfig.h>

#include <generated/blog_channel_ncd_net_backend_waitdevice.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    const char *ifname;
    NCDUdevClient client;
    char *devpath;
    uintmax_t ifindex;
};

static void client_handler (struct instance *o, char *devpath, int have_map, BStringMap map)
{
    if (o->devpath && !strcmp(devpath, o->devpath) && !NCDUdevManager_Query(o->i->umanager, o->devpath)) {
        // free devpath
        free(o->devpath);
        
        // set no devpath
        o->devpath = NULL;
        
        // signal down
        NCDModuleInst_Backend_Down(o->i);
    } else {
        const BStringMap *cache_map = NCDUdevManager_Query(o->i->umanager, devpath);
        if (!cache_map) {
            goto out;
        }
        
        const char *subsystem = BStringMap_Get(cache_map, "SUBSYSTEM");
        const char *interface = BStringMap_Get(cache_map, "INTERFACE");
        const char *ifindex_str = BStringMap_Get(cache_map, "IFINDEX");
        
        uintmax_t ifindex;
        if (!(subsystem && !strcmp(subsystem, "net") && interface && !strcmp(interface, o->ifname) && ifindex_str && parse_unsigned_integer(ifindex_str, &ifindex))) {
            goto out;
        }
        
        if (o->devpath && (strcmp(o->devpath, devpath) || o->ifindex != ifindex)) {
            // free devpath
            free(o->devpath);
            
            // set no devpath
            o->devpath = NULL;
            
            // signal down
            NCDModuleInst_Backend_Down(o->i);
        }
        
        if (!o->devpath) {
            // grab devpath
            o->devpath = devpath;
            devpath = NULL;
            
            // remember ifindex
            o->ifindex = ifindex;
            
            // signal up
            NCDModuleInst_Backend_Up(o->i);
        }
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
    
    // init client
    NCDUdevClient_Init(&o->client, o->i->umanager, o, (NCDUdevClient_handler)client_handler);
    
    // set no devpath
    o->devpath = NULL;
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free devpath
    if (o->devpath) {
        free(o->devpath);
    }
    
    // free client
    NCDUdevClient_Free(&o->client);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static const struct NCDModule modules[] = {
    {
        .type = "net.backend.waitdevice",
        .func_new = func_new,
        .func_die = func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_backend_waitdevice = {
    .modules = modules
};
