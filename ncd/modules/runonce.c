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

static int build_cmdline (NCDModuleInst *i, NCDValue *cmd_arg, char **exec, CmdLine *cl)
{
    if (NCDValue_Type(cmd_arg) != NCDVALUE_LIST) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    // read exec
    NCDValue *exec_arg = NCDValue_ListFirst(cmd_arg);
    if (!exec_arg) {
        ModuleLog(i, BLOG_ERROR, "missing executable name");
        goto fail0;
    }
    if (NCDValue_Type(exec_arg) != NCDVALUE_STRING) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    if (!(*exec = strdup(NCDValue_StringValue(exec_arg)))) {
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
    NCDValue *arg = exec_arg;
    while (arg = NCDValue_ListNext(cmd_arg, arg)) {
        if (NCDValue_Type(arg) != NCDVALUE_STRING) {
            ModuleLog(i, BLOG_ERROR, "wrong type");
            goto fail2;
        }
        
        if (!CmdLine_Append(cl, NCDValue_StringValue(arg))) {
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
    o->term_on_deinit = 0;
    
    // read arguments
    NCDValue *cmd_arg;
    NCDValue *opts_arg = NULL;
    if (!NCDValue_ListRead(i->args, 1, &cmd_arg) && !NCDValue_ListRead(i->args, 2, &cmd_arg, &opts_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (opts_arg && NCDValue_Type(opts_arg) != NCDVALUE_LIST) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    int keep_stdout = 0;
    int keep_stderr = 0;
    
    // read options
    for (NCDValue *opt = (opts_arg ? NCDValue_ListFirst(opts_arg) : NULL); opt; opt = NCDValue_ListNext(opts_arg, opt)) {
        // read name
        if (NCDValue_Type(opt) != NCDVALUE_STRING) {
            ModuleLog(o->i, BLOG_ERROR, "wrong option name type");
            goto fail1;
        }
        char *optname = NCDValue_StringValue(opt);
        
        if (!strcmp(optname, "term_on_deinit")) {
            o->term_on_deinit = 1;
        }
        else if (!strcmp(optname, "keep_stdout")) {
            keep_stdout = 1;
        }
        else if (!strcmp(optname, "keep_stderr")) {
            keep_stderr = 1;
        }
        else {
            ModuleLog(o->i, BLOG_ERROR, "unknown option name");
            goto fail1;
        }
    }
    
    // build cmdline
    char *exec;
    CmdLine cl;
    if (!build_cmdline(o->i, cmd_arg, &exec, &cl)) {
        goto fail1;
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
    
    // start process
    if (!BProcess_InitWithFds(&o->process, o->i->params->manager, (BProcess_handler)process_handler, o, exec, CmdLine_Get(&cl), NULL, fds, fds_map)) {
        ModuleLog(i, BLOG_ERROR, "BProcess_Init failed");
        CmdLine_Free(&cl);
        free(exec);
        goto fail1;
    }
    
    CmdLine_Free(&cl);
    free(exec);
    
    // set state
    o->state = STATE_RUNNING;
    
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
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
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

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    ASSERT(o->state == STATE_FINISHED)
    
    if (!strcmp(name, "exit_status")) {
        char str[30];
        snprintf(str, sizeof(str), "%d", o->exit_status);
        
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
        .type = "runonce",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_runonce = {
    .modules = modules
};
