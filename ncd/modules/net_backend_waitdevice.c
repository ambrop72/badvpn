/**
 * @file net_backend_waitdevice.c
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
 * Module which waits for the presence of a network interface.
 * 
 * Synopsis: net.backend.waitdevice(string ifname)
 * Description: statement is UP when a network interface named ifname
 *   exists, and DOWN when it does not.
 */

#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include <misc/parse_number.h>
#include <ncd/NCDModule.h>
#include <ncd/NCDIfConfig.h>

#include <generated/blog_channel_ncd_net_backend_waitdevice.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define DEVPATH_REGEX "/net/[^/]+$"

struct instance {
    NCDModuleInst *i;
    const char *ifname;
    NCDUdevClient client;
    regex_t reg;
    char *devpath;
    uintmax_t ifindex;
};

static void client_handler (struct instance *o, char *devpath, int have_map, BStringMap map)
{
    if (o->devpath && !strcmp(devpath, o->devpath) && !NCDUdevManager_Query(o->i->params->umanager, o->devpath)) {
        // free devpath
        free(o->devpath);
        
        // set no devpath
        o->devpath = NULL;
        
        // signal down
        NCDModuleInst_Backend_Down(o->i);
    } else {
        const BStringMap *cache_map = NCDUdevManager_Query(o->i->params->umanager, devpath);
        if (!cache_map) {
            goto out;
        }
        
        int match_res = regexec(&o->reg, devpath, 0, NULL, 0);
        const char *interface = BStringMap_Get(cache_map, "INTERFACE");
        const char *ifindex_str = BStringMap_Get(cache_map, "IFINDEX");
        
        uintmax_t ifindex;
        if (!(!match_res && interface && !strcmp(interface, o->ifname) && ifindex_str && parse_unsigned_integer(ifindex_str, &ifindex))) {
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
    if (!NCDValue_IsStringNoNulls(arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->ifname = NCDValue_StringValue(arg);
    
    // init client
    NCDUdevClient_Init(&o->client, o->i->params->umanager, o, (NCDUdevClient_handler)client_handler);
    
    // compile regex
    if (regcomp(&o->reg, DEVPATH_REGEX, REG_EXTENDED)) {
        ModuleLog(o->i, BLOG_ERROR, "regcomp failed");
        goto fail2;
    }
    
    // set no devpath
    o->devpath = NULL;
    return;
    
fail2:
    NCDUdevClient_Free(&o->client);
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
    
    // free regex
    regfree(&o->reg);
    
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
