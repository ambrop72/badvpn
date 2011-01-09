/**
 * @file command_template.h
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
 * Template for a module which executes a command to start and stop.
 * The command is executed asynchronously.
 */

#ifndef BADVPN_NCD_MODULES_COMMAND_TEMPLATE_H
#define BADVPN_NCD_MODULES_COMMAND_TEMPLATE_H

#include <misc/cmdline.h>
#include <system/BEventLock.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_net_iptables.h>

typedef int (*command_template_build_cmdline) (NCDModuleInst *i, int remove, char **exec, CmdLine *cl);

void * command_template_new (NCDModuleInst *i, command_template_build_cmdline build_cmdline, int blog_channel, BEventLock *elock);
void command_template_func_free (void *vo);
void command_template_func_die (void *vo);

#endif
