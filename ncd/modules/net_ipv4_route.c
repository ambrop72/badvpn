/**
 * @file net_ipv4_route.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
 *     If 'gateway' is "none", the route will only be associated with an interface.
 *     If 'gateway' is "blackhole", the route will be a blackhole route (and 'ifname' is unused).
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDModule.h>
#include <ncd/NCDIfConfig.h>

#include <generated/blog_channel_ncd_net_ipv4_route.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define TYPE_NORMAL 1
#define TYPE_IFONLY 2
#define TYPE_BLACKHOLE 3

struct instance {
    NCDModuleInst *i;
    struct ipv4_ifaddr dest;
    int type;
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
    if (!NCDValue_IsStringNoNulls(dest_arg) || !NCDValue_IsStringNoNulls(dest_prefix_arg) || !NCDValue_IsStringNoNulls(gateway_arg) ||
        !NCDValue_IsStringNoNulls(metric_arg) || !NCDValue_IsStringNoNulls(ifname_arg)) {
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
    
    // read gateway and choose type
    char *gateway_str = NCDValue_StringValue(gateway_arg);
    if (!strcmp(gateway_str, "none")) {
        o->type = TYPE_IFONLY;
    }
    else if (!strcmp(gateway_str, "blackhole")) {
        o->type = TYPE_BLACKHOLE;
    } else {
        if (!ipaddr_parse_ipv4_addr(gateway_str, &o->gateway)) {
            ModuleLog(o->i, BLOG_ERROR, "wrong gateway");
            goto fail1;
        }
        o->type = TYPE_NORMAL;
    }
    
    // read metric
    o->metric = atoi(NCDValue_StringValue(metric_arg));
    
    // read ifname
    o->ifname = NCDValue_StringValue(ifname_arg);
    
    // add route
    int res;
    switch (o->type) {
        case TYPE_NORMAL:
            res = NCDIfConfig_add_ipv4_route(o->dest, &o->gateway, o->metric, o->ifname);
            break;
        case TYPE_IFONLY:
            res = NCDIfConfig_add_ipv4_route(o->dest, NULL, o->metric, o->ifname);
            break;
        case TYPE_BLACKHOLE:
            res = NCDIfConfig_add_ipv4_blackhole_route(o->dest, o->metric);
            break;
        default: ASSERT(0);
    }
    if (!res) {
        ModuleLog(o->i, BLOG_ERROR, "failed to add route");
        goto fail1;
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
    int res;
    switch (o->type) {
        case TYPE_NORMAL:
            res = NCDIfConfig_remove_ipv4_route(o->dest, &o->gateway, o->metric, o->ifname);
            break;
        case TYPE_IFONLY:
            res = NCDIfConfig_remove_ipv4_route(o->dest, NULL, o->metric, o->ifname);
            break;
        case TYPE_BLACKHOLE:
            res = NCDIfConfig_remove_ipv4_blackhole_route(o->dest, o->metric);
            break;
        default: ASSERT(0);
    }
    if (!res) {
        ModuleLog(o->i, BLOG_ERROR, "failed to remove route");
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
