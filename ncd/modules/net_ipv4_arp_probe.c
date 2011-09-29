/**
 * @file net_ipv4_arp_probe.c
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
 * ARP probing module.
 * 
 * Synopsis:
 *   net.ipv4.arp_probe(string ifname, string addr)
 * 
 * Description:
 *   Monitors local presence of an IPv4 host on a network interface.
 *   On initialization, may take some time to determine whether
 *   the host is present or not, then goes to UP state. When it
 *   determines that presence has changed, toggles itself DOWN then
 *   UP to expose the new determination.
 * 
 * Variables:
 *   exists - "true" if the host exists, "false" if not
 */

#include <stdlib.h>

#include <misc/ipaddr.h>
#include <arpprobe/BArpProbe.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_net_ipv4_arp_probe.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define STATE_UNKNOWN 1
#define STATE_EXIST 2
#define STATE_NOEXIST 3

struct instance {
    NCDModuleInst *i;
    BArpProbe arpprobe;
    int state;
};

static void instance_free (struct instance *o);

static void arpprobe_handler (struct instance *o, int event)
{
    switch (event) {
        case BARPPROBE_EVENT_EXIST: {
            ASSERT(o->state == STATE_UNKNOWN || o->state == STATE_NOEXIST)
            
            ModuleLog(o->i, BLOG_INFO, "exist");
            
            if (o->state == STATE_NOEXIST) {
                // signal down
                NCDModuleInst_Backend_Down(o->i);
            }
            
            // signal up
            NCDModuleInst_Backend_Up(o->i);
            
            // set state exist
            o->state = STATE_EXIST;
        } break;
        
        case BARPPROBE_EVENT_NOEXIST: {
            ASSERT(o->state == STATE_UNKNOWN || o->state == STATE_EXIST)
            
            ModuleLog(o->i, BLOG_INFO, "noexist");
            
            if (o->state == STATE_EXIST) {
                // signal down
                NCDModuleInst_Backend_Down(o->i);
            }
            
            // signal up
            NCDModuleInst_Backend_Up(o->i);
            
            // set state noexist
            o->state = STATE_NOEXIST;
        } break;
        
        case BARPPROBE_EVENT_ERROR: {
            ModuleLog(o->i, BLOG_ERROR, "error");
            
            // set error
            NCDModuleInst_Backend_SetError(o->i);
            
            // die
            instance_free(o);
            return;
        } break;
        
        default: ASSERT(0);
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
    
    // read arguments
    NCDValue *arg_ifname;
    NCDValue *arg_addr;
    if (!NCDValue_ListRead(i->args, 2, &arg_ifname, &arg_addr)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(arg_ifname) != NCDVALUE_STRING || NCDValue_Type(arg_addr) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    char *ifname = NCDValue_StringValue(arg_ifname);
    char *addr_str = NCDValue_StringValue(arg_addr);
    
    // parse address
    uint32_t addr;
    if (!ipaddr_parse_ipv4_addr(addr_str, &addr)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong address");
        goto fail1;
    }
    
    // init arpprobe
    if (!BArpProbe_Init(&o->arpprobe, ifname, addr, i->reactor, o, (BArpProbe_handler)arpprobe_handler)) {
        ModuleLog(o->i, BLOG_ERROR, "BArpProbe_Init failed");
        goto fail1;
    }
    
    // set state unknown
    o->state = STATE_UNKNOWN;
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
    
    // free arpprobe
    BArpProbe_Free(&o->arpprobe);
    
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
    ASSERT(o->state == STATE_EXIST || o->state == STATE_NOEXIST)
    
    if (!strcmp(name, "exists")) {
        const char *str = (o->state == STATE_EXIST ? "true" : "false");
        
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
        .type = "net.ipv4.arp_probe",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_ipv4_arp_probe = {
    .modules = modules
};
