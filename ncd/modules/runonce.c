/**
 * @file runonce.c
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
 * Imperative program execution module. On initialization, starts the process.
 * Goes to UP state when the process terminates. When requested to die, waits for
 * the process to terminate if it's running, optionally sending SIGTERM.
 * 
 * Synopsis: runonce(list(string) cmd, [list opts])
 * Arguments:
 *   cmd - Command to run on startup. The first element is the full path
 *     to the executable, other elements are command line arguments (excluding
 *     the zeroth argument).
 *   opts - List of options:
 *     "term_on_deinit" - If we get a deinit request while the process is running,
 *                        send it SIGTERM.
 *     "keep_stdout" - Start the program with the same stdout as the NCD process.
 *     "keep_stderr" - Start the program with the same stderr as the NCD process.
 *     "do_setsid" - Call setsid() in the child before exec. This is needed to
 *                   start the 'agetty' program.
 * Variables:
 *   string exit_status - if the program exited normally, the non-negative exit code, otherwise -1
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <misc/cmdline.h>
#include <system/BProcess.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_runonce.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define STATE_RUNNING 1
#define STATE_RUNNING_DIE 2
#define STATE_FINISHED 3

struct instance {
    NCDModuleInst *i;
    int term_on_deinit;
    int state;
    BProcess process;
    int exit_status;
};

static void instance_free (struct instance *o);

static int build_cmdline (NCDModuleInst *i, NCDValRef cmd_arg, char **exec, CmdLine *cl)
{
    ASSERT(!NCDVal_IsInvalid(cmd_arg))
    
    if (!NCDVal_IsList(cmd_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    size_t count = NCDVal_ListCount(cmd_arg);
    
    // read exec
    if (count == 0) {
        ModuleLog(i, BLOG_ERROR, "missing executable name");
        goto fail0;
    }
    NCDValRef exec_arg = NCDVal_ListGet(cmd_arg, 0);
    if (!NCDVal_IsStringNoNulls(exec_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    if (!(*exec = strdup(NCDVal_StringValue(exec_arg)))) {
        ModuleLog(i, BLOG_ERROR, "strdup failed");
        goto fail0;
    }
    
    // start cmdline
    if (!CmdLine_Init(cl)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Init failed");
        goto fail1;
    }
    
    // add header
    if (!CmdLine_Append(cl, *exec)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Append failed");
        goto fail2;
    }
    
    // add additional arguments
    for (size_t j = 1; j < count; j++) {
        NCDValRef arg = NCDVal_ListGet(cmd_arg, j);
        
        if (!NCDVal_IsStringNoNulls(arg)) {
            ModuleLog(i, BLOG_ERROR, "wrong type");
            goto fail2;
        }
        
        if (!CmdLine_Append(cl, NCDVal_StringValue(arg))) {
            ModuleLog(i, BLOG_ERROR, "CmdLine_Append failed");
            goto fail2;
        }
    }
    
    // finish
    if (!CmdLine_Finish(cl)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Finish failed");
        goto fail2;
    }
    
    return 1;
    
fail2:
    CmdLine_Free(cl);
fail1:
    free(*exec);
fail0:
    return 0;
}

static void process_handler (struct instance *o, int normally, uint8_t normally_exit_status)
{
    ASSERT(o->state == STATE_RUNNING || o->state == STATE_RUNNING_DIE)
    
    // free process
    BProcess_Free(&o->process);
    
    // if we were requested to die, die now
    if (o->state == STATE_RUNNING_DIE) {
        instance_free(o);
        return;
    }
    
    // remember exit status
    o->exit_status = (normally ? normally_exit_status : -1);
    
    // set state
    o->state = STATE_FINISHED;
    
    // set up
    NCDModuleInst_Backend_Up(o->i);
}

static void func_new (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    struct instance *o = vo;
    o->i = i;
    
    // init arguments
    o->term_on_deinit = 0;
    
    // read arguments
    NCDValRef cmd_arg;
    NCDValRef opts_arg = NCDVal_NewInvalid();
    if (!NCDVal_ListRead(params->args, 1, &cmd_arg) && !NCDVal_ListRead(params->args, 2, &cmd_arg, &opts_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsInvalid(opts_arg) && !NCDVal_IsList(opts_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    int keep_stdout = 0;
    int keep_stderr = 0;
    int do_setsid = 0;
    
    // read options
    size_t count = NCDVal_IsInvalid(opts_arg) ? 0 : NCDVal_ListCount(opts_arg);
    for (size_t j = 0; j < count; j++) {
        NCDValRef opt = NCDVal_ListGet(opts_arg, j);
        
        // read name
        if (!NCDVal_IsStringNoNulls(opt)) {
            ModuleLog(o->i, BLOG_ERROR, "wrong option name type");
            goto fail0;
        }
        const char *optname = NCDVal_StringValue(opt);
        
        if (!strcmp(optname, "term_on_deinit")) {
            o->term_on_deinit = 1;
        }
        else if (!strcmp(optname, "keep_stdout")) {
            keep_stdout = 1;
        }
        else if (!strcmp(optname, "keep_stderr")) {
            keep_stderr = 1;
        }
        else if (!strcmp(optname, "do_setsid")) {
            do_setsid = 1;
        }
        else {
            ModuleLog(o->i, BLOG_ERROR, "unknown option name");
            goto fail0;
        }
    }
    
    // build cmdline
    char *exec;
    CmdLine cl;
    if (!build_cmdline(o->i, cmd_arg, &exec, &cl)) {
        goto fail0;
    }
    
    // build fd mapping
    int fds[3];
    int fds_map[2];
    int nfds = 0;
    if (keep_stdout) {
        fds[nfds] = 1;
        fds_map[nfds++] = 1;
    }
    if (keep_stderr) {
        fds[nfds] = 2;
        fds_map[nfds++] = 2;
    }
    fds[nfds] = -1;
    
    // build params
    struct BProcess_params p_params;
    p_params.username = NULL;
    p_params.fds = fds;
    p_params.fds_map = fds_map;
    p_params.do_setsid = do_setsid;
    
    // start process
    if (!BProcess_Init2(&o->process, o->i->params->iparams->manager, (BProcess_handler)process_handler, o, exec, CmdLine_Get(&cl), p_params)) {
        ModuleLog(i, BLOG_ERROR, "BProcess_Init failed");
        CmdLine_Free(&cl);
        free(exec);
        goto fail0;
    }
    
    CmdLine_Free(&cl);
    free(exec);
    
    // set state
    o->state = STATE_RUNNING;
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void instance_free (struct instance *o)
{
    NCDModuleInst_Backend_Dead(o->i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    ASSERT(o->state != STATE_RUNNING_DIE)
    
    if (o->state == STATE_FINISHED) {
        instance_free(o);
        return;
    }
    
    // send SIGTERM if requested
    if (o->term_on_deinit) {
        BProcess_Terminate(&o->process);
    }
    
    o->state = STATE_RUNNING_DIE;
}

static int func_getvar (void *vo, const char *name, NCDValMem *mem, NCDValRef *out)
{
    struct instance *o = vo;
    ASSERT(o->state == STATE_FINISHED)
    
    if (!strcmp(name, "exit_status")) {
        char str[30];
        snprintf(str, sizeof(str), "%d", o->exit_status);
        
        *out = NCDVal_NewString(mem, str);
        if (NCDVal_IsInvalid(*out)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDVal_NewString failed");
        }
        return 1;
    }
    
    return 0;
}

static const struct NCDModule modules[] = {
    {
        .type = "runonce",
        .func_new2 = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = NULL
    }
};

struct NCDModuleGroup ncdmodule_runonce = {
    .modules = modules
};
