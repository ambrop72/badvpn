/**
 * @file netmask.c
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
 *   ipv4_prefix_to_mask(string prefix)
 *   ipv4_mask_to_prefix(string mask)
 * 
 * Variables:
 *   string (empty) - converted
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <misc/ipaddr.h>
#include <misc/parse_number.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_netmask.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct prefix_to_mask_instance {
    NCDModuleInst *i;
    uint32_t mask;
};

struct mask_to_prefix_instance {
    NCDModuleInst *i;
    int prefix;
};

static void prefix_to_mask_func_init (NCDModuleInst *i)
{
    // allocate structure
    struct prefix_to_mask_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // read arguments
    NCDValue *prefix_arg;
    if (!NCDValue_ListRead(i->args, 1, &prefix_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDValue_IsStringNoNulls(prefix_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // parse prefix
    uintmax_t prefix;
    if (!parse_unsigned_integer(NCDValue_StringValue(prefix_arg), &prefix) || prefix > 32) {
        ModuleLog(i, BLOG_ERROR, "bad prefix");
        goto fail1;
    }
    
    // build mask
    o->mask = ipaddr_ipv4_mask_from_prefix(prefix);
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void prefix_to_mask_func_die (void *vo)
{
    struct prefix_to_mask_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free structure
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int prefix_to_mask_func_getvar (void *vo, const char *name, NCDValue *out_value)
{
    struct prefix_to_mask_instance *o = vo;
    
    if (!strcmp(name, "")) {
        uint8_t *x = (void *)&o->mask;
        char buf[20];
        sprintf(buf, "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8, x[0], x[1], x[2], x[3]);
        
        if (!NCDValue_InitString(out_value, buf)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static void mask_to_prefix_func_init (NCDModuleInst *i)
{
    // allocate structure
    struct mask_to_prefix_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // read arguments
    NCDValue *mask_arg;
    if (!NCDValue_ListRead(i->args, 1, &mask_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDValue_IsStringNoNulls(mask_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // parse mask
    uint32_t mask;
    if (!ipaddr_parse_ipv4_addr(NCDValue_StringValue(mask_arg), &mask)) {
        ModuleLog(i, BLOG_ERROR, "bad mask");
        goto fail1;
    }
    
    // build prefix
    if (!ipaddr_ipv4_prefix_from_mask(mask, &o->prefix)) {
        ModuleLog(i, BLOG_ERROR, "bad mask");
        goto fail1;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void mask_to_prefix_func_die (void *vo)
{
    struct mask_to_prefix_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free structure
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int mask_to_prefix_func_getvar (void *vo, const char *name, NCDValue *out_value)
{
    struct mask_to_prefix_instance *o = vo;
    
    if (!strcmp(name, "")) {
        char buf[6];
        sprintf(buf, "%d", o->prefix);
        
        if (!NCDValue_InitString(out_value, buf)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "ipv4_prefix_to_mask",
        .func_new = prefix_to_mask_func_init,
        .func_die = prefix_to_mask_func_die,
        .func_getvar = prefix_to_mask_func_getvar
    }, {
        .type = "ipv4_mask_to_prefix",
        .func_new = mask_to_prefix_func_init,
        .func_die = mask_to_prefix_func_die,
        .func_getvar = mask_to_prefix_func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_netmask = {
    .modules = modules
};
