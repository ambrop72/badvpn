/**
 * @file NCDRfkillMonitor.c
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <misc/debug.h>
#include <misc/nonblocking.h>
#include <base/BLog.h>

#include <ncd/NCDRfkillMonitor.h>

#include <generated/blog_channel_NCDRfkillMonitor.h>

#define RFKILL_DEVICE_NODE "/dev/rfkill"

static void rfkill_fd_handler (NCDRfkillMonitor *o, int events);

void rfkill_fd_handler (NCDRfkillMonitor *o, int events)
{
    DebugObject_Access(&o->d_obj);
    
    // read from netlink fd
    struct rfkill_event event;
    int len = read(o->rfkill_fd, &event, sizeof(event));
    if (len < 0) {
        BLog(BLOG_ERROR, "read failed");
        return;
    }
    if (len != sizeof(event)) {
        BLog(BLOG_ERROR, "read returned wrong length");
        return;
    }
    
    // call handler
    o->handler(o->user, event);
    return;
}

int NCDRfkillMonitor_Init (NCDRfkillMonitor *o, BReactor *reactor, NCDRfkillMonitor_handler handler, void *user)
{
    // init arguments
    o->reactor = reactor;
    o->handler = handler;
    o->user = user;
    
    // open rfkill
    if ((o->rfkill_fd = open(RFKILL_DEVICE_NODE, O_RDONLY)) < 0) {
        BLog(BLOG_ERROR, "open failed");
        goto fail0;
    }
    
    // set fd non-blocking
    if (!badvpn_set_nonblocking(o->rfkill_fd)) {
        BLog(BLOG_ERROR, "badvpn_set_nonblocking failed");
        goto fail1;
    }
    
    // init BFileDescriptor
    BFileDescriptor_Init(&o->bfd, o->rfkill_fd, (BFileDescriptor_handler)rfkill_fd_handler, o);
    if (!BReactor_AddFileDescriptor(o->reactor, &o->bfd)) {
        BLog(BLOG_ERROR, "BReactor_AddFileDescriptor failed");
        goto fail1;
    }
    BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, BREACTOR_READ);
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail1:
    ASSERT_FORCE(close(o->rfkill_fd) == 0)
fail0:
    return 0;
}

void NCDRfkillMonitor_Free (NCDRfkillMonitor *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free BFileDescriptor
    BReactor_RemoveFileDescriptor(o->reactor, &o->bfd);
    
    // close rfkill
    ASSERT_FORCE(close(o->rfkill_fd) == 0)
}
