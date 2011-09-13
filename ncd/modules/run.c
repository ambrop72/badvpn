/**
 * @file run.c
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
 * Module for running arbitrary programs.
 * NOTE: There is no locking - the program may run in parallel with other
 * NCD processes and their programs.
 * 
 * Synopsis: run(list do_cmd, list undo_cmd)
 * Arguments:
 *   list do_cmd - Command run on startup. The first element is the full path
 *     to the executable, other elements are command line arguments (excluding
 *     the zeroth argument). An empty list is interpreted as no operation.
 *   list undo_cmd - Command run on shutdown, like do_cmd.
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/modules/command_template.h>

#include <generated/blog_channel_ncd_run.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

static void template_free_func (void *vo, int is_error);

struct instance {
    NCDModuleInst *i;
    BEventLock lock;
    command_template_instance cti;
};

static int build_cmdline (NCDModuleInst *i, int remove, char **exec, CmdLine *cl)
{
    // read arguments
    NCDValue *do_cmd_arg;
    NCDValue *undo_cmd_arg;
    if (!NCDValue_ListRead(i->args, 2, &do_cmd_arg, &undo_cmd_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (NCDValue_Type(do_cmd_arg) != NCDVALUE_LIST || NCDValue_Type(undo_cmd_arg) != NCDVALUE_LIST) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    NCDValue *list = (remove ? undo_cmd_arg : do_cmd_arg);
    
    // check if there is no command
    if (!NCDValue_ListFirst(list)) {
        *exec = NULL;
        return 1;
    }
    
    // read exec
    NCDValue *exec_arg = NCDValue_ListFirst(list);
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
    while (arg = NCDValue_ListNext(list, arg)) {
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

static void func_new (NCDModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        BLog(BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // init dummy event lock
    BEventLock_Init(&o->lock, BReactor_PendingGroup(i->reactor));
    
    command_template_new(&o->cti, i, build_cmdline, template_free_func, o, BLOG_CURRENT_CHANNEL, &o->lock);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

void template_free_func (void *vo, int is_error)
{
    struct instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free dummy event lock
    BEventLock_Free(&o->lock);
    
    // free instance
    free(o);
    
    if (is_error) {
        NCDModuleInst_Backend_SetError(i);
    }
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    
    command_template_die(&o->cti);
}

static const struct NCDModule modules[] = {
    {
        .type = "run",
        .func_new = func_new,
        .func_die = func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_run= {
    .modules = modules
};
