/**
 * @file net_ipv4_dhcp.c
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
 * Synopsis: net.ipv4.dhcp(string ifname)
 * Variables:
 *   string addr - assigned IP address ("A.B.C.D")
 *   string prefix - address prefix length ("N")
 *   string gateway - router address ("A.B.C.D")
 *   list(string) dns_servers - DNS server addresses ("A.B.C.D" ...)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <misc/debug.h>
#include <misc/ipaddr.h>
#include <dhcpclient/BDHCPClient.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_net_ipv4_dhcp.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    BDHCPClient dhcp;
};

static void dhcp_handler (struct instance *o, int event)
{
    switch (event) {
        case BDHCPCLIENT_EVENT_UP:
            NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
            break;
        case BDHCPCLIENT_EVENT_DOWN:
            NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_DOWN);
            break;
        default:
            ASSERT(0);
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
    if (!NCDValue_ListRead(o->i->args, 1, &arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    char *ifname = NCDValue_StringValue(arg);
    
    // init DHCP
    if (!BDHCPClient_Init(&o->dhcp, ifname, o->i->reactor, (BDHCPClient_handler)dhcp_handler, o)) {
        ModuleLog(o->i, BLOG_ERROR, "BDHCPClient_Init failed");
        goto fail1;
    }
    
    return o;
    
fail1:
    free(o);
fail0:
    return NULL;
}

static void func_free (void *vo)
{
    struct instance *o = vo;
    
    // free DHCP
    BDHCPClient_Free(&o->dhcp);
    
    // free instance
    free(o);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "addr")) {
        uint32_t addr;
        BDHCPClient_GetClientIP(&o->dhcp, &addr);
        
        uint8_t *b = (uint8_t *)&addr;
        char str[50];
        sprintf(str, "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8, b[0], b[1], b[2], b[3]);
        
        if (!NCDValue_InitString(out, str)) {
            return 0;
        }
        
        return 1;
    }
    
    if (!strcmp(name, "prefix")) {
        uint32_t addr;
        BDHCPClient_GetClientIP(&o->dhcp, &addr);
        uint32_t mask;
        BDHCPClient_GetClientMask(&o->dhcp, &mask);
        
        struct ipv4_ifaddr ifaddr;
        if (!ipaddr_ipv4_ifaddr_from_addr_mask(addr, mask, &ifaddr)) {
            return 0;
        }
        
        char str[10];
        sprintf(str, "%d", ifaddr.prefix);
        
        if (!NCDValue_InitString(out, str)) {
            return 0;
        }
        
        return 1;
    }
    
    if (!strcmp(name, "gateway")) {
        uint32_t addr;
        if (!BDHCPClient_GetRouter(&o->dhcp, &addr)) {
            return 0;
        }
        
        uint8_t *b = (uint8_t *)&addr;
        char str[50];
        sprintf(str, "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8, b[0], b[1], b[2], b[3]);
        
        if (!NCDValue_InitString(out, str)) {
            return 0;
        }
        
        return 1;
    }
    
    if (!strcmp(name, "dns_servers")) {
        uint32_t servers[BDHCPCLIENT_MAX_DOMAIN_NAME_SERVERS];
        int num_servers = BDHCPClient_GetDNS(&o->dhcp, servers, BDHCPCLIENT_MAX_DOMAIN_NAME_SERVERS);
        
        NCDValue list;
        NCDValue_InitList(&list);
        
        for (int i = 0; i < num_servers; i++) {
            uint8_t *b = (uint8_t *)&servers[i];
            char str[50];
            sprintf(str, "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8, b[0], b[1], b[2], b[3]);
            
            NCDValue server;
            if (!NCDValue_InitString(&server, str)) {
                goto fail1;
            }
            
            if (!NCDValue_ListAppend(&list, server)) {
                NCDValue_Free(&server);
                goto fail1;
            }
        }
        
        *out = list;
        return 1;
        
    fail1:
        NCDValue_Free(&list);
        return 0;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "net.ipv4.dhcp",
        .func_new = func_new,
        .func_free = func_free,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_ipv4_dhcp = {
    .modules = modules
};
