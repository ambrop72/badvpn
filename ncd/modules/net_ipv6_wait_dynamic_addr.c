/**
 * @file net_ipv6_wait_dynamic_addr.c
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
 * Synopsis:
 *   net.ipv6.wait_dynamic_addr(string ifname)
 * 
 * Description:
 *   Waits for a dynamic IPv6 address to be obtained on the interface,
 *   and goes up when it is obtained.
 *   If the address is subsequently lost, goes back down and again waits
 *   for an address.
 * 
 * Variables:
 *   string addr - dynamic address obtained on the interface, as formatted
 *                 by getnameinfo(..., NI_NUMERICHOST)
 *   string prefix - prefix length
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <misc/get_iface_info.h>
#include <misc/ipaddr6.h>
#include <ncd/NCDModule.h>
#include <ncd/NCDInterfaceMonitor.h>

#include <generated/blog_channel_ncd_net_ipv6_wait_dynamic_addr.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    NCDInterfaceMonitor monitor;
    struct ipv6_ifaddr ifaddr;
    int up;
};

static void instance_free (struct instance *o);

static void monitor_handler (struct instance *o, struct NCDInterfaceMonitor_event event)
{
    if (!o->up && event.event == NCDIFMONITOR_EVENT_IPV6_ADDR_ADDED && (event.u.ipv6_addr.addr_flags & NCDIFMONITOR_ADDR_FLAG_DYNAMIC)) {
        // rememeber address, set up
        o->ifaddr = event.u.ipv6_addr.addr;
        o->up = 1;
        
        // signal up
        NCDModuleInst_Backend_Up(o->i);
    }
    else if (o->up && event.event == NCDIFMONITOR_EVENT_IPV6_ADDR_REMOVED && !memcmp(event.u.ipv6_addr.addr.addr, o->ifaddr.addr, 16) && event.u.ipv6_addr.addr.prefix == o->ifaddr.prefix) {
        // set not up
        o->up = 0;
        
        // signal down
        NCDModuleInst_Backend_Down(o->i);
    }
}

static void monitor_handler_error (struct instance *o)
{
    ModuleLog(o->i, BLOG_ERROR, "monitor error");
    
    NCDModuleInst_Backend_SetError(o->i);
    instance_free(o);
}

static void func_new (NCDModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // read arguments
    NCDValue *ifname_arg;
    if (!NCDValue_ListRead(i->args, 1, &ifname_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDValue_IsStringNoNulls(ifname_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    const char *ifname = NCDValue_StringValue(ifname_arg);
    
    // get interface index
    int ifindex;
    if (!get_iface_info(ifname, NULL, NULL, &ifindex)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to get interface index");
        goto fail1;
    }
    
    // init monitor
    if (!NCDInterfaceMonitor_Init(&o->monitor, ifindex, NCDIFMONITOR_WATCH_IPV6_ADDR, i->params->reactor, o, (NCDInterfaceMonitor_handler)monitor_handler, (NCDInterfaceMonitor_handler_error)monitor_handler_error)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDInterfaceMonitor_Init failed");
        goto fail1;
    }
    
    // set not up
    o->up = 0;
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void instance_free (struct instance *o)
{
    NCDModuleInst *i = o->i;
    
    // free monitor
    NCDInterfaceMonitor_Free(&o->monitor);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    instance_free(o);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    ASSERT(o->up)
    
    if (!strcmp(name, "addr")) {
        char str[IPADDR6_PRINT_MAX];
        if (!ipaddr6_print_addr(o->ifaddr.addr, str)) {
            ModuleLog(o->i, BLOG_ERROR, "ipaddr6_print_addr failed");
            return 0;
        }
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        return 1;
    }
    
    if (!strcmp(name, "prefix")) {
        char str[10];
        sprintf(str, "%d", o->ifaddr.prefix);
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "net.ipv6.wait_dynamic_addr",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_ipv6_wait_dynamic_addr = {
    .modules = modules
};
