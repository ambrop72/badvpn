/**
 * @file BUnixSignal.c
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

#include <inttypes.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <sys/signalfd.h>
#include <fcntl.h>

#include <system/BLog.h>

#include <system/BUnixSignal.h>

#include <generated/blog_channel_BUnixSignal.h>

static void signalfd_handler (BUnixSignal *o, int events)
{
    DebugObject_Access(&o->d_obj);
    
    // read a signal
    struct signalfd_siginfo siginfo;
    int bytes = read(o->signalfd_fd, &siginfo, sizeof(siginfo));
    if (bytes < 0) {
        int error = errno;
        if (error == EAGAIN || error == EWOULDBLOCK) {
            return;
        }
        BLog(BLOG_ERROR, "read failed (%d)", error);
        return;
    }
    ASSERT_FORCE(bytes == sizeof(siginfo))
    
    // check signal
    if (siginfo.ssi_signo > INT_MAX) {
        BLog(BLOG_ERROR, "read returned out of int range signo (%"PRIu32")", siginfo.ssi_signo);
        return;
    }
    int signo = siginfo.ssi_signo;
    if (sigismember(&o->signals, signo) <= 0) {
        BLog(BLOG_ERROR, "read returned wrong signo (%d)", signo);
        return;
    }
    
    BLog(BLOG_DEBUG, "dispatching signal %d", signo);
    
    // call handler
    struct BUnixSignal_siginfo dispatch_siginfo = { .signo = signo, .pid = siginfo.ssi_pid };
    o->handler(o->user, dispatch_siginfo);
    return;
}

int BUnixSignal_Init (BUnixSignal *o, BReactor *reactor, sigset_t signals, BUnixSignal_handler handler, void *user)
{
    // init arguments
    o->reactor = reactor;
    o->signals = signals;
    o->handler = handler;
    o->user = user;
    
    // init signalfd fd
    if ((o->signalfd_fd = signalfd(-1, &o->signals, 0)) < 0) {
        BLog(BLOG_ERROR, "signalfd failed");
        goto fail0;
    }
    
    // set non-blocking
    if (fcntl(o->signalfd_fd, F_SETFL, O_NONBLOCK) < 0) {
        DEBUG("cannot set non-blocking");
        goto fail1;
    }
    
    // init signalfd BFileDescriptor
    BFileDescriptor_Init(&o->signalfd_bfd, o->signalfd_fd, (BFileDescriptor_handler)signalfd_handler, o);
    if (!BReactor_AddFileDescriptor(o->reactor, &o->signalfd_bfd)) {
        BLog(BLOG_ERROR, "BReactor_AddFileDescriptor failed");
        goto fail1;
    }
    BReactor_SetFileDescriptorEvents(o->reactor, &o->signalfd_bfd, BREACTOR_READ);
    
    // block signals
    if (sigprocmask(SIG_BLOCK, &o->signals, 0) < 0) {
        BLog(BLOG_ERROR, "sigprocmask block failed");
        goto fail2;
    }
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail2:
    BReactor_RemoveFileDescriptor(o->reactor, &o->signalfd_bfd);
fail1:
    ASSERT_FORCE(close(o->signalfd_fd) == 0)
fail0:
    return 0;
}

void BUnixSignal_Free (BUnixSignal *o, int unblock)
{
    ASSERT(unblock == 0 || unblock == 1)
    DebugObject_Free(&o->d_obj);
    
    if (unblock) {
        // unblock signals
        ASSERT_FORCE(sigprocmask(SIG_UNBLOCK, &o->signals, 0) == 0)
    }
    
    // free signalfd BFileDescriptor
    BReactor_RemoveFileDescriptor(o->reactor, &o->signalfd_bfd);
    
    // free signalfd fd
    ASSERT_FORCE(close(o->signalfd_fd) == 0)
}
