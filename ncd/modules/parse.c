/**
 * @file parse.c
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
 *   parse_number(string str)
 *   parse_value(string str)
 *   parse_ipv4_addr(string str)
 *   
 * Variables:
 *   succeeded - "true" or "false", reflecting success of the parsing
 *   (empty) - normalized parsed value (only if succeeded)
 * 
 * Synopsis:
 *   parse_ipv4_cidr_addr(string str)
 * 
 * Variables:
 *   succeeded - "true" or "false", reflecting success of the parsing
 *   (empty) - normalized CIDR notation address (only if succeeded)
 *   addr - normalized address without prefix (only if succeeded)
 *   prefix - normalized prefix without address (only if succeeded)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <misc/parse_number.h>
#include <misc/ipaddr.h>
#include <ncd/NCDValParser.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_parse.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    NCDValMem mem;
    NCDValRef value;
    int succeeded;
};

struct ipv4_cidr_instance {
    NCDModuleInst *i;
    int succeeded;
    struct ipv4_ifaddr ifaddr;
};

typedef int (*parse_func) (NCDModuleInst *i, const char *str, NCDValMem *mem, NCDValRef *out);

static int parse_number (NCDModuleInst *i, const char *str, NCDValMem *mem, NCDValRef *out)
{
    uintmax_t n;
    if (!parse_unsigned_integer(str, &n)) {
        ModuleLog(i, BLOG_ERROR, "failed to parse number");
        return 0;
    }
    
    char buf[25];
    snprintf(buf, sizeof(buf), "%"PRIuMAX, n);
    
    *out = NCDVal_NewString(mem, buf);
    if (NCDVal_IsInvalid(*out)) {
        ModuleLog(i, BLOG_ERROR, "NCDVal_NewString failed");
        return 0;
    }
    
    return 1;
}

static int parse_value (NCDModuleInst *i, const char *str, NCDValMem *mem, NCDValRef *out)
{
    if (!NCDValParser_Parse(str, strlen(str), mem, out)) {
        ModuleLog(i, BLOG_ERROR, "failed to parse value");
        return 0;
    }
    
    return 1;
}

static int parse_ipv4_addr (NCDModuleInst *i, const char *str, NCDValMem *mem, NCDValRef *out)
{
    uint32_t addr;
    if (!ipaddr_parse_ipv4_addr((char *)str, &addr)) {
        ModuleLog(i, BLOG_ERROR, "failed to parse ipv4 addresss");
        return 0;
    }
    
    char buf[IPADDR_PRINT_MAX];
    ipaddr_print_addr(addr, buf);
    
    *out = NCDVal_NewString(mem, buf);
    if (NCDVal_IsInvalid(*out)) {
        ModuleLog(i, BLOG_ERROR, "NCDVal_NewString failed");
        return 0;
    }
    
    return 1;
}

static void new_templ (void *vo, NCDModuleInst *i, parse_func pfunc)
{
    struct instance *o = vo;
    o->i = i;
    
    // read arguments
    NCDValRef str_arg;
    if (!NCDVal_ListRead(i->args, 1, &str_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsString(str_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    // init mem
    NCDValMem_Init(&o->mem);
    
    // parse
    if (NCDVal_StringHasNulls(str_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "string has nulls");
        o->succeeded = 0;
    } else {
        o->succeeded = pfunc(i, NCDVal_StringValue(str_arg), &o->mem, &o->value);
    }
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    
    // free mem
    NCDValMem_Free(&o->mem);
    
    NCDModuleInst_Backend_Dead(o->i);
}

static int func_getvar (void *vo, const char *name, NCDValMem *mem, NCDValRef *out)
{
    struct instance *o = vo;
    
    if (!strcmp(name, "succeeded")) {
        const char *str = o->succeeded ? "true" : "false";
        *out = NCDVal_NewString(mem, str);
        if (NCDVal_IsInvalid(*out)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewString failed");
        }
        return 1;
    }
    
    if (o->succeeded && !strcmp(name, "")) {
        *out = NCDVal_NewCopy(mem, o->value);
        if (NCDVal_IsInvalid(*out)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewCopy failed");
        }
        return 1;
    }
    
    return 0;
}

static void func_new_parse_number (void *vo, NCDModuleInst *i)
{
    new_templ(vo, i, parse_number);
}

static void func_new_parse_value (void *vo, NCDModuleInst *i)
{
    new_templ(vo, i, parse_value);
}

static void func_new_parse_ipv4_addr (void *vo, NCDModuleInst *i)
{
    new_templ(vo, i, parse_ipv4_addr);
}

static void ipv4_cidr_addr_func_new (void *vo, NCDModuleInst *i)
{
    struct ipv4_cidr_instance *o = vo;
    o->i = i;
    
    NCDValRef str_arg;
    if (!NCDVal_ListRead(i->args, 1, &str_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsString(str_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    o->succeeded = ipaddr_parse_ipv4_ifaddr_bin(NCDVal_StringValue(str_arg), NCDVal_StringLength(str_arg), &o->ifaddr);
    
    NCDModuleInst_Backend_Up(i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static int ipv4_cidr_addr_func_getvar (void *vo, const char *name, NCDValMem *mem, NCDValRef *out)
{
    struct ipv4_cidr_instance *o = vo;
    
    if (!strcmp(name, "succeeded")) {
        const char *str = o->succeeded ? "true" : "false";
        *out = NCDVal_NewString(mem, str);
        if (NCDVal_IsInvalid(*out)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewString failed");
        }
        return 1;
    }
    
    if (!o->succeeded) {
        return 0;
    }
    
    char str[IPADDR_PRINT_MAX];
    
    if (!strcmp(name, "")) {
        ipaddr_print_ifaddr(o->ifaddr, str);
    }
    else if (!strcmp(name, "addr")) {
        ipaddr_print_addr(o->ifaddr.addr, str);
    }
    else if (!strcmp(name, "prefix")) {
        sprintf(str, "%d", o->ifaddr.prefix);
    }
    else {
        return 0;
    }
    
    *out = NCDVal_NewString(mem, str);
    if (NCDVal_IsInvalid(*out)) {
        ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewString failed");
    }
    return 1;
}

static const struct NCDModule modules[] = {
    {
        .type = "parse_number",
        .func_new2 = func_new_parse_number,
        .func_die = func_die,
        .func_getvar = func_getvar,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = "parse_value",
        .func_new2 = func_new_parse_value,
        .func_die = func_die,
        .func_getvar = func_getvar,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = "parse_ipv4_addr",
        .func_new2 = func_new_parse_ipv4_addr,
        .func_die = func_die,
        .func_getvar = func_getvar,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = "parse_ipv4_cidr_addr",
        .func_new2 = ipv4_cidr_addr_func_new,
        .func_getvar = ipv4_cidr_addr_func_getvar,
        .alloc_size = sizeof(struct ipv4_cidr_instance)
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_parse = {
    .modules = modules
};
