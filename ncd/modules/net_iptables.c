/**
 * @file net_iptables.c
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
 * iptables module.
 * 
 * Synopsis: net.iptables.append(string table, string chain, string arg1,  ...)
 * Synopsis: net.iptables.policy(string table, string chain, string target, string revert_target)
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/modules/command_template.h>

#include <generated/blog_channel_ncd_net_iptables.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define IPTABLES_PATH "/sbin/iptables"

static int build_append_cmdline (NCDModuleInst *i, int remove, char **exec, CmdLine *cl)
{
    // read arguments
    NCDValue *table_arg;
    NCDValue *chain_arg;
    if (!NCDValue_ListReadHead(i->args, 2, &table_arg, &chain_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (NCDValue_Type(table_arg) != NCDVALUE_STRING || NCDValue_Type(chain_arg) != NCDVALUE_STRING) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    char *table = NCDValue_StringValue(table_arg);
    char *chain = NCDValue_StringValue(chain_arg);
    
    // alloc exec
    if (!(*exec = strdup(IPTABLES_PATH))) {
        ModuleLog(i, BLOG_ERROR, "strdup failed");
        goto fail0;
    }
    
    // start cmdline
    if (!CmdLine_Init(cl)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Init failed");
        goto fail1;
    }
    
    // add header
    if (!CmdLine_Append(cl, IPTABLES_PATH) || !CmdLine_Append(cl, "-t") || !CmdLine_Append(cl, table) || !CmdLine_Append(cl, (remove ? "-D" : "-A")) || !CmdLine_Append(cl, chain)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Append failed");
        goto fail2;
    }
    
    // add additional arguments
    NCDValue *arg = NCDValue_ListNext(i->args, chain_arg);
    while (arg) {
        if (NCDValue_Type(arg) != NCDVALUE_STRING) {
            ModuleLog(i, BLOG_ERROR, "wrong type");
            goto fail2;
        }
        
        if (!CmdLine_Append(cl, NCDValue_StringValue(arg))) {
            ModuleLog(i, BLOG_ERROR, "CmdLine_Append failed");
            goto fail2;
        }
        
        arg = NCDValue_ListNext(i->args, arg);
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

static int build_policy_cmdline (NCDModuleInst *i, int remove, char **exec, CmdLine *cl)
{
    // read arguments
    NCDValue *table_arg;
    NCDValue *chain_arg;
    NCDValue *target_arg;
    NCDValue *revert_target_arg;
    if (!NCDValue_ListRead(i->args, 4, &table_arg, &chain_arg, &target_arg, &revert_target_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (NCDValue_Type(table_arg) != NCDVALUE_STRING || NCDValue_Type(chain_arg) != NCDVALUE_STRING ||
        NCDValue_Type(target_arg) != NCDVALUE_STRING || NCDValue_Type(revert_target_arg) != NCDVALUE_STRING
    ) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    char *table = NCDValue_StringValue(table_arg);
    char *chain = NCDValue_StringValue(chain_arg);
    char *target = NCDValue_StringValue(target_arg);
    char *revert_target = NCDValue_StringValue(revert_target_arg);
    
    // alloc exec
    if (!(*exec = strdup(IPTABLES_PATH))) {
        ModuleLog(i, BLOG_ERROR, "strdup failed");
        goto fail0;
    }
    
    // start cmdline
    if (!CmdLine_Init(cl)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Init failed");
        goto fail1;
    }
    
    // add arguments
    if (!CmdLine_Append(cl, IPTABLES_PATH) || !CmdLine_Append(cl, "-t") || !CmdLine_Append(cl, table) ||
        !CmdLine_Append(cl, "-P") || !CmdLine_Append(cl, chain) || !CmdLine_Append(cl, (remove ? revert_target : target))) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Append failed");
        goto fail2;
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

static void * append_func_new (NCDModuleInst *i)
{
    return command_template_new(i, build_append_cmdline, BLOG_CURRENT_CHANNEL);
}

static void * policy_func_new (NCDModuleInst *i)
{
    return command_template_new(i, build_policy_cmdline, BLOG_CURRENT_CHANNEL);
}

static const struct NCDModule modules[] = {
    {
        .type = "net.iptables.append",
        .func_new = append_func_new,
        .func_free = command_template_func_free,
        .func_die = command_template_func_die
    }, {
        .type = "net.iptables.policy",
        .func_new = policy_func_new,
        .func_free = command_template_func_free,
        .func_die = command_template_func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_iptables = {
    .modules = modules
};
