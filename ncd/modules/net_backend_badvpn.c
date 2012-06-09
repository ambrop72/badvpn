/**
 * @file net_backend_badvpn.c
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
 * BadVPN interface module.
 * 
 * Synopsis: net.backend.badvpn(string ifname, string user, string exec, list(string) args)
 */

#include <stdlib.h>
#include <string.h>

#include <misc/cmdline.h>
#include <ncd/NCDModule.h>
#include <ncd/NCDIfConfig.h>

#include <generated/blog_channel_ncd_net_backend_badvpn.h>

#define RETRY_TIME 5000

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    const char *ifname;
    const char *user;
    const char *exec;
    NCDValRef args;
    int dying;
    int started;
    BTimer timer;
    BProcess process;
};

static void try_process (struct instance *o);
static void process_handler (struct instance *o, int normally, uint8_t normally_exit_status);
static void timer_handler (struct instance *o);
static void instance_free (struct instance *o);

void try_process (struct instance *o)
{
    CmdLine c;
    if (!CmdLine_Init(&c)) {
        goto fail0;
    }
    
    // append exec
    if (!CmdLine_Append(&c, o->exec)) {
        goto fail1;
    }
    
    // append tapdev
    if (!CmdLine_Append(&c, "--tapdev") || !CmdLine_Append(&c, o->ifname)) {
        goto fail1;
    }
    
    // append arguments
    size_t count = NCDVal_ListCount(o->args);
    for (size_t j = 0; j < count; j++) {
        NCDValRef arg = NCDVal_ListGet(o->args, j);
        if (!CmdLine_Append(&c, NCDVal_StringValue(arg))) {
            goto fail1;
        }
    }
    
    // terminate cmdline
    if (!CmdLine_Finish(&c)) {
        goto fail1;
    }
    
    // start process
    if (!BProcess_Init(&o->process, o->i->iparams->manager, (BProcess_handler)process_handler, o, ((char **)c.arr.v)[0], (char **)c.arr.v, o->user)) {
        ModuleLog(o->i, BLOG_ERROR, "BProcess_Init failed");
        goto fail1;
    }
    
    CmdLine_Free(&c);
    
    // set started
    o->started = 1;
    
    return;
    
fail1:
    CmdLine_Free(&c);
fail0:
    // retry
    o->started = 0;
    BReactor_SetTimer(o->i->iparams->reactor, &o->timer);
}

void process_handler (struct instance *o, int normally, uint8_t normally_exit_status)
{
    ASSERT(o->started)
    
    ModuleLog(o->i, BLOG_INFO, "process terminated");
    
    // free process
    BProcess_Free(&o->process);
    
    // set not started
    o->started = 0;
    
    if (o->dying) {
        instance_free(o);
        return;
    }
    
    // set timer
    BReactor_SetTimer(o->i->iparams->reactor, &o->timer);
}

void timer_handler (struct instance *o)
{
    ASSERT(!o->started)
    
    ModuleLog(o->i, BLOG_INFO, "retrying");
    
    // try starting process again
    try_process(o);
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
    NCDValRef ifname_arg;
    NCDValRef user_arg;
    NCDValRef exec_arg;
    NCDValRef args_arg;
    if (!NCDVal_ListRead(o->i->args, 4, &ifname_arg, &user_arg, &exec_arg, &args_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDVal_IsStringNoNulls(ifname_arg) || !NCDVal_IsStringNoNulls(user_arg) ||
        !NCDVal_IsStringNoNulls(exec_arg) || !NCDVal_IsList(args_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->ifname = NCDVal_StringValue(ifname_arg);
    o->user = NCDVal_StringValue(user_arg);
    o->exec = NCDVal_StringValue(exec_arg);
    o->args = args_arg;
    
    // check arguments
    size_t count = NCDVal_ListCount(o->args);
    for (size_t j = 0; j < count; j++) {
        NCDValRef arg = NCDVal_ListGet(o->args, j);
        if (!NCDVal_IsStringNoNulls(arg)) {
            ModuleLog(o->i, BLOG_ERROR, "wrong type");
            goto fail1;
        }
    }
    
    // create TAP device
    if (!NCDIfConfig_make_tuntap(o->ifname, o->user, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to create TAP device");
        goto fail1;
    }
    
    // set device up
    if (!NCDIfConfig_set_up(o->ifname)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to set device up");
        goto fail2;
    }
    
    // set not dying
    o->dying = 0;
    
    // init timer
    BTimer_Init(&o->timer, RETRY_TIME, (BTimer_handler)timer_handler, o);
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    
    // try starting process
    try_process(o);
    
    return;
    
fail2:
    if (!NCDIfConfig_remove_tuntap(o->ifname, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to remove TAP device");
    }
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

void instance_free (struct instance *o)
{
    ASSERT(!o->started)
    NCDModuleInst *i = o->i;
    
    // free timer
    BReactor_RemoveTimer(o->i->iparams->reactor, &o->timer);
    
    // set device down
    if (!NCDIfConfig_set_down(o->ifname)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to set device down");
    }
    
    // free TAP device
    if (!NCDIfConfig_remove_tuntap(o->ifname, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to remove TAP device");
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    ASSERT(!o->dying)
    
    if (!o->started) {
        instance_free(o);
        return;
    }
    
    // request termination
    BProcess_Terminate(&o->process);
    
    // remember dying
    o->dying = 1;
}

static const struct NCDModule modules[] = {
    {
        .type = "net.backend.badvpn",
        .func_new = func_new,
        .func_die = func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_backend_badvpn = {
    .modules = modules
};
