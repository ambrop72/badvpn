/**
 * @file NCDInterfaceMonitor.c
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

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include <sys/socket.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <asm/types.h>
#include <asm/types.h>

#include <misc/debug.h>
#include <misc/nonblocking.h>
#include <system/BLog.h>

#include <ncd/NCDInterfaceMonitor.h>

#include <generated/blog_channel_NCDInterfaceMonitor.h>

static void netlink_fd_handler (NCDInterfaceMonitor *o, int events);
static void process_buffer (NCDInterfaceMonitor *o);
static void more_job_handler (NCDInterfaceMonitor *o);

void netlink_fd_handler (NCDInterfaceMonitor *o, int events)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->buf_left == -1)
    
    // read from netlink fd
    int len = read(o->netlink_fd, o->buf, sizeof(o->buf));
    if (len < 0) {
        BLog(BLOG_ERROR, "read failed");
        return;
    }
    
    // stop receiving fd events
    BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, 0);
    
    // set buffer
    o->buf_nh = (struct nlmsghdr *)o->buf;
    o->buf_left = len;
    
    // process buffer
    process_buffer(o);
    return;
}

void process_buffer (NCDInterfaceMonitor *o)
{
    ASSERT(o->buf_left >= 0)
    
    for (; NLMSG_OK(o->buf_nh, o->buf_left); o->buf_nh = NLMSG_NEXT(o->buf_nh, o->buf_left)) {
        if (o->buf_nh->nlmsg_type == NLMSG_DONE) {
            break;
        }
        
        if (o->buf_nh->nlmsg_type != RTM_NEWLINK && o->buf_nh->nlmsg_type != RTM_DELLINK) {
            continue;
        }
        
        void *pl = NLMSG_DATA(o->buf_nh);
        int pl_len = NLMSG_PAYLOAD(o->buf_nh, 0);
        
        if (pl_len < sizeof(struct ifinfomsg)) {
            BLog(BLOG_ERROR, "missing infomsg");
            continue;
        }
        struct ifinfomsg *im = (void *)pl;
        
        // parse attributes to get interface name
        
        char *ifname = NULL;
        
        int rta_len = pl_len - sizeof(struct ifinfomsg);
        
        for (struct rtattr *rta = (void *)(im + 1); RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len)) {
            uint8_t *attr = RTA_DATA(rta);
            int attr_len = RTA_PAYLOAD(rta);
            
            if (rta->rta_type == IFLA_IFNAME && attr_len > 0 && attr[attr_len - 1] == '\0') {
                ifname = (char *)attr;
            }
        }
        
        if (!ifname) {
            continue;
        }
        
        // finish this message
        o->buf_nh = NLMSG_NEXT(o->buf_nh, o->buf_left);
        
        // schedule more job
        BPending_Set(&o->more_job);
        
        // dispatch event
        o->handler(o->user, ifname, NCDIfConfig_query(ifname));
        return;
    }
    
    // set no buffer
    o->buf_left = -1;
    
    // continue receiving fd events
    BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, BREACTOR_READ);
}

void more_job_handler (NCDInterfaceMonitor *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->buf_left >= 0)
    
    // process buffer
    process_buffer(o);
    return;
}

int NCDInterfaceMonitor_Init (NCDInterfaceMonitor *o, BReactor *reactor, NCDInterfaceMonitor_handler handler, void *user)
{
    // init arguments
    o->reactor = reactor;
    o->handler = handler;
    o->user = user;
    
    // init netlink fd
    if ((o->netlink_fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0) {
        BLog(BLOG_ERROR, "socket failed");
        goto fail0;
    }
    
    // set fd non-blocking
    if (!badvpn_set_nonblocking(o->netlink_fd)) {
        BLog(BLOG_ERROR, "badvpn_set_nonblocking failed");
        goto fail1;
    }
    
    // bind netlink fd
    struct sockaddr_nl sa;
    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_groups = RTMGRP_LINK;
    if (bind(o->netlink_fd, (void *)&sa, sizeof(sa)) < 0) {
        BLog(BLOG_ERROR, "bind failed");
        goto fail1;
    }
    
    // init BFileDescriptor
    BFileDescriptor_Init(&o->bfd, o->netlink_fd, (BFileDescriptor_handler)netlink_fd_handler, o);
    if (!BReactor_AddFileDescriptor(o->reactor, &o->bfd)) {
        BLog(BLOG_ERROR, "BReactor_AddFileDescriptor failed");
        goto fail1;
    }
    BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, BREACTOR_READ);
    
    // set nothing in buffer
    o->buf_left = -1;
    
    // init more job
    BPending_Init(&o->more_job, BReactor_PendingGroup(o->reactor), (BPending_handler)more_job_handler, o);
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    close(o->netlink_fd);
fail0:
    return 0;
}

void NCDInterfaceMonitor_Free (NCDInterfaceMonitor *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free more job
    BPending_Free(&o->more_job);
    
    // free BFileDescriptor
    BReactor_RemoveFileDescriptor(o->reactor, &o->bfd);
    
    // close netlink fd
    close(o->netlink_fd);
}

void NCDInterfaceMonitor_Pause (NCDInterfaceMonitor *o)
{
    DebugObject_Access(&o->d_obj);
    
    if (o->buf_left >= 0) {
        BPending_Unset(&o->more_job);
    } else {
        BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, 0);
    }
}

void NCDInterfaceMonitor_Continue (NCDInterfaceMonitor *o)
{
    DebugObject_Access(&o->d_obj);
    
    if (o->buf_left >= 0) {
        BPending_Set(&o->more_job);
    } else {
        BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, BREACTOR_READ);
    }
}
