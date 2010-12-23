/**
 * @file net_ipv4_addr.c
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
 * IPv4 address module.
 * 
 * Synopsis: net.ipv4.addr(string ifname, string addr, string prefix)
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDModule.h>
#include <ncd/NCDIfConfig.h>

#include <generated/blog_channel_ncd_net_ipv4_addr.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    const char *ifname;
    struct ipv4_ifaddr ifaddr;
};

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
    NCDValue *addr_arg;
    NCDValue *prefix_arg;
    if (!NCDValue_ListRead(o->i->args, 3, &ifname_arg, &addr_arg, &prefix_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(ifname_arg) != NCDVALUE_STRING || NCDValue_Type(addr_arg) != NCDVALUE_STRING || NCDValue_Type(prefix_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->ifname = NCDValue_StringValue(ifname_arg);
    if (!ipaddr_parse_ipv4_addr(NCDValue_StringValue(addr_arg), &o->ifaddr.addr)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong address");
        goto fail1;
    }
    if (!ipaddr_parse_ipv4_prefix(NCDValue_StringValue(prefix_arg), &o->ifaddr.prefix)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong prefix");
        goto fail1;
    }
    
    // add address
    if (!NCDIfConfig_add_ipv4_addr(o->ifname, o->ifaddr)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to add IP address");
        goto fail1;
    }
    
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    return o;
    
fail1:
    free(o);
fail0:
    return NULL;
}

static void func_free (void *vo)
{
    struct instance *o = vo;
    
    // remove address
    if (!NCDIfConfig_remove_ipv4_addr(o->ifname, o->ifaddr)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to remove IP address");
    }
    
    // free instance
    free(o);
}

const struct NCDModule ncdmodule_net_ipv4_addr = {
    .type = "net.ipv4.addr",
    .func_new = func_new,
    .func_free = func_free
};
