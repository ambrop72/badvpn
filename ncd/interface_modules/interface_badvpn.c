/**
 * @file interface_badvpn.c
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
 * BadVPN interface backend.
 */

#include <stdlib.h>
#include <string.h>

#include <misc/cmdline.h>
#include <system/BProcess.h>
#include <ncd/NCDInterfaceModule.h>
#include <ncd/NCDIfConfig.h>

struct instance {
    NCDInterfaceModuleInst *i;
    BProcess process;
    int need_terminate;
};

static int build_cmdline (struct instance *o, CmdLine *c)
{
    if (!CmdLine_Init(c)) {
        goto fail0;
    }
    
    // find exec statement
    struct NCDConfig_statements *exec_st = NCDConfig_find_statement(o->i->conf->statements, "badvpn.exec");
    if (!exec_st) {
        NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_ERROR, "badvpn.exec missing");
        goto fail1;
    }
    
    // check arity
    char *exec_arg;
    if (!NCDConfig_statement_has_one_arg(exec_st, &exec_arg)) {
        NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_ERROR, "badvpn.exec: wrong arity");
        goto fail1;
    }
    
    // append to cmdline
    if (!CmdLine_Append(c, exec_arg)) {
        goto fail1;
    }
    
    // append tapdev
    if (!CmdLine_Append(c, "--tapdev") || !CmdLine_Append(c, o->i->conf->name)) {
        goto fail1;
    }
    
    // iterate arg statements
    struct NCDConfig_statements *st = o->i->conf->statements;
    while (st = NCDConfig_find_statement(st, "badvpn.arg")) {
        // iterate arguments
        struct NCDConfig_strings *arg = st->args;
        while (arg) {
            if (!CmdLine_Append(c, arg->value)) {
                goto fail1;
            }
            
            arg = arg->next;
        }
        
        st = st->next;
    }
    
    // terminate
    if (!CmdLine_Finish(c)) {
        goto fail1;
    }
    
    return 1;
    
fail1:
    CmdLine_Free(c);
fail0:
    return 0;
}

static const char * read_user (struct instance *o)
{
    // find statement
    struct NCDConfig_statements *user_st = NCDConfig_find_statement(o->i->conf->statements, "badvpn.user");
    if (!user_st) {
        NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_ERROR, "badvpn.user missing");
        return NULL;
    }
    
    // check arity
    char *user_arg;
    if (!NCDConfig_statement_has_one_arg(user_st, &user_arg)) {
        NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_ERROR, "badvpn.user: wrong arity");
        return NULL;
    }
    
    return user_arg;
}

static void process_handler (struct instance *o, int normally, uint8_t normally_exit_status)
{
    NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_INFO, "process terminated");
    
    // set not need terminate
    o->need_terminate = 0;
    
    // report error
    NCDInterfaceModuleInst_Backend_Error(o->i);
    return;
}

static void * func_new (NCDInterfaceModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        NCDInterfaceModuleInst_Backend_Log(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    
    // init arguments
    o->i = i;
    
    // create TAP device
    if (!NCDIfConfig_make_tuntap(o->i->conf->name, NULL, 0)) {
        NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_ERROR, "failed to create TAP device");
        goto fail1;
    }
    
    // set device up
    if (!NCDIfConfig_set_up(o->i->conf->name)) {
        NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_ERROR, "failed to set device up");
        goto fail2;
    }
    
    // read username
    const char *username = read_user(o);
    if (!username) {
        goto fail2;
    }
    
    // build cmdline
    CmdLine cl;
    if (!build_cmdline(o, &cl)) {
        NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_ERROR, "failed to build cmdline");
        goto fail2;
    }
    
    // start process
    if (!BProcess_Init(&o->process, o->i->manager, (BProcess_handler)process_handler, o, ((char **)cl.arr.v)[0], (char **)cl.arr.v, username)) {
        NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_ERROR, "BProcess_Init failed");
        CmdLine_Free(&cl);
        goto fail2;
    }
    
    CmdLine_Free(&cl);
    
    // set need terminate
    o->need_terminate = 1;
    
    // report up
    NCDInterfaceModuleInst_Backend_Event(o->i, NCDINTERFACEMODULE_EVENT_UP);
    
    return o;
    
fail2:
    NCDIfConfig_remove_tuntap(o->i->conf->name, 0);
fail1:
    free(o);
fail0:
    return NULL;
}

static void func_free (void *vo)
{
    struct instance *o = vo;
    
    // order process to terminate
    if (o->need_terminate) {
        BProcess_Terminate(&o->process);
    }
    
    // free process
    BProcess_Free(&o->process);
    
    // remove TAP device
    if (!NCDIfConfig_remove_tuntap(o->i->conf->name, 0)) {
        NCDInterfaceModuleInst_Backend_Log(o->i, BLOG_ERROR, "failed to remove TAP device");
    }
    
    // free instance
    free(o);
}

static void func_finish (void *vo)
{
    struct instance *o = vo;
    ASSERT(o->need_terminate)
    
    // order process to terminate
    BProcess_Terminate(&o->process);
    
    // set not need terminate
    o->need_terminate = 0;
}

const struct NCDInterfaceModule ncd_interface_badvpn = {
    .type = "badvpn",
    .func_new = func_new,
    .func_free = func_free,
    .func_finish = func_finish
};
