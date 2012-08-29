/**
 * @file ip_in_network.c
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
 * Module for checking whether two IP addresses belong to the same network.
 * 
 * Synopsis: ip_in_network(string addr1, string addr2, string netprefix)
 * Variables:
 *   string (empty) - "true" if addr1 and addr2 are in the same network, with
 *     netprefix prefix, "false" if not (IPv4 only).
 */

#include <stdlib.h>
#include <string.h>

#include <misc/ipaddr.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_ip_in_network.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    int value;
};

static void func_new (void *vo, NCDModuleInst *i)
{
    struct instance *o = vo;
    o->i = i;
    
    // read arguments
    NCDValRef arg_addr1;
    NCDValRef arg_addr2;
    NCDValRef arg_netprefix;
    if (!NCDVal_ListRead(o->i->args, 3, &arg_addr1, &arg_addr2, &arg_netprefix)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsStringNoNulls(arg_addr1) || !NCDVal_IsStringNoNulls(arg_addr2) || !NCDVal_IsStringNoNulls(arg_netprefix)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    // parse
    uint32_t addr1;
    uint32_t addr2;
    int netprefix;
    if (!ipaddr_parse_ipv4_addr((char *)NCDVal_StringValue(arg_addr1), &addr1)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong addr1");
        goto fail0;
    }
    if (!ipaddr_parse_ipv4_addr((char *)NCDVal_StringValue(arg_addr2), &addr2)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong addr2");
        goto fail0;
    }
    if (!ipaddr_parse_ipv4_prefix((char *)NCDVal_StringValue(arg_netprefix), &netprefix)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong netprefix");
        goto fail0;
    }
    
    // test
    o->value = ipaddr_ipv4_addrs_in_network(addr1, addr2, netprefix);
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    
    NCDModuleInst_Backend_Dead(o->i);
}

static int func_getvar (void *vo, const char *name, NCDValMem *mem, NCDValRef *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "")) {
        const char *v = (o->value ? "true" : "false");
        
        *out = NCDVal_NewString(mem, v);
        if (NCDVal_IsInvalid(*out)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewString failed");
        }
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "ip_in_network",
        .func_new2 = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_ip_in_network = {
    .modules = modules
};
