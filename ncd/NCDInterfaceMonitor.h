/**
 * @file NCDInterfaceMonitor.h
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
 */

#ifndef BADVPN_NCD_NCDINTERFACEMONITOR_H
#define BADVPN_NCD_NCDINTERFACEMONITOR_H

#include <linux/netlink.h>

#include <ncd/NCDIfConfig.h>

#include <system/DebugObject.h>
#include <system/BReactor.h>
#include <system/BPending.h>

typedef void (*NCDInterfaceMonitor_handler) (void *user, const char *ifname, int if_flags);

typedef struct {
    BReactor *reactor;
    NCDInterfaceMonitor_handler handler;
    void *user;
    int netlink_fd;
    BFileDescriptor bfd;
    uint8_t buf[4096];
    struct nlmsghdr *buf_nh;
    int buf_left;
    BPending more_job;
    DebugObject d_obj;
} NCDInterfaceMonitor;

int NCDInterfaceMonitor_Init (NCDInterfaceMonitor *o, BReactor *reactor, NCDInterfaceMonitor_handler handler, void *user);
void NCDInterfaceMonitor_Free (NCDInterfaceMonitor *o);

#endif
