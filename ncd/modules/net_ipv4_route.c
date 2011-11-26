/**
 * @file net_ipv4_route.c
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
 * IPv4 route module.
 * 
 * Synopsis:
 *     net.ipv4.route(string dest, string dest_prefix, string gateway, string metric, string ifname)
 * Description:
 *     Adds an IPv4 route to the system's routing table on initiailzation, and removes it on
 *     deinitialization.
 *     The 'ifname' argument can be "<blackhole>" for a route which drops packets. In this case,
 *     the 'gateway' argument is not used.
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDModule.h>
#include <ncd/NCDIfConfig.h>

#include <generated/blog_channel_ncd_net_ipv4_route.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    struct ipv4_ifaddr dest;
    int have_gateway;
    uint32_t gateway;
    int metric;
    const char *ifname;
};

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
    
    // read arguments
    NCDValue *dest_arg;
    NCDValue *dest_prefix_arg;
    NCDValue *gateway_arg;
    NCDValue *metric_arg;
    NCDValue *ifname_arg;
    if (!NCDValue_ListRead(o->i->args, 5, &dest_arg, &dest_prefix_arg, &gateway_arg, &metric_arg, &ifname_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(dest_arg) != NCDVALUE_STRING || NCDValue_Type(dest_prefix_arg) != NCDVALUE_STRING || NCDValue_Type(gateway_arg) != NCDVALUE_STRING ||
        NCDValue_Type(metric_arg) != NCDVALUE_STRING || NCDValue_Type(ifname_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // read dest
    if (!ipaddr_parse_ipv4_addr(NCDValue_StringValue(dest_arg), &o->dest.addr)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong dest addr");
        goto fail1;
    }
    if (!ipaddr_parse_ipv4_prefix(NCDValue_StringValue(dest_prefix_arg), &o->dest.prefix)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong dest prefix");
        goto fail1;
    }
    
    // read gateway
    char *gateway_str = NCDValue_StringValue(gateway_arg);
    if (!strcmp(gateway_str, "none")) {
        o->have_gateway = 0;
    } else {
        if (!ipaddr_parse_ipv4_addr(gateway_str, &o->gateway)) {
            ModuleLog(o->i, BLOG_ERROR, "wrong gateway");
            goto fail1;
        }
        o->have_gateway = 1;
    }
    
    // read metric
    o->metric = atoi(NCDValue_StringValue(metric_arg));
    
    // read ifname
    o->ifname = NCDValue_StringValue(ifname_arg);
    
    // add route
    if (!strcmp(o->ifname, "<blackhole>")) {
        if (!NCDIfConfig_add_ipv4_blackhole_route(o->dest, o->metric)) {
            ModuleLog(o->i, BLOG_ERROR, "failed to add blackhole route");
            goto fail1;
        }
    } else {
        if (!NCDIfConfig_add_ipv4_route(o->dest, (o->have_gateway ? &o->gateway : NULL), o->metric, o->ifname)) {
            ModuleLog(o->i, BLOG_ERROR, "failed to add route");
            goto fail1;
        }
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    
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
    
    // remove route
    if (!strcmp(o->ifname, "<blackhole>")) {
        if (!NCDIfConfig_remove_ipv4_blackhole_route(o->dest, o->metric)) {
            ModuleLog(o->i, BLOG_ERROR, "failed to remove blackhole route");
        }
    } else {
        if (!NCDIfConfig_remove_ipv4_route(o->dest, (o->have_gateway ? &o->gateway : NULL), o->metric, o->ifname)) {
            ModuleLog(o->i, BLOG_ERROR, "failed to remove route");
        }
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static const struct NCDModule modules[] = {
    {
        .type = "net.ipv4.route",
        .func_new = func_new,
        .func_die = func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_ipv4_route = {
    .modules = modules
};
