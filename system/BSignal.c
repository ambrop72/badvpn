/**
 * @file BSignal.c
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

#ifdef BADVPN_USE_WINAPI
#include <windows.h>
#else
#include <signal.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#endif

#include <misc/debug.h>
#include <system/BLog.h>

#include <system/BSignal.h>

#include <generated/blog_channel_BSignal.h>

#define EMPTY_SIGSET(_set) sigemptyset(&_set);
#define FILL_SIGSET(_set) sigemptyset(&_set); sigaddset(&_set, SIGTERM); sigaddset(&_set, SIGINT);

struct {
    int initialized;
    int capturing;
    BSignal_handler handler;
    void *handler_user;
    BReactor *handler_reactor;
    #ifdef BADVPN_USE_WINAPI
    CRITICAL_SECTION handler_mutex; // mutex to make sure only one handler is working at a time
    CRITICAL_SECTION state_mutex; // mutex for capturing and signal_pending
    int signal_pending;
    HANDLE signal_sem1;
    HANDLE signal_sem2;
    BHandle bhandle;
    #else
    int signal_fd;
    BFileDescriptor bfd;
    #endif
} bsignal_global = {
    .initialized = 0,
};

#ifdef BADVPN_USE_WINAPI

static void signal_handle_handler (void *user)
{
    ASSERT(bsignal_global.initialized)
    ASSERT(bsignal_global.capturing)
    ASSERT(bsignal_global.handler)
    
    ASSERT(bsignal_global.signal_pending)
    bsignal_global.signal_pending = 0;
    ASSERT_FORCE(ReleaseSemaphore(bsignal_global.signal_sem2, 1, NULL))
    
    BLog(BLOG_DEBUG, "Dispatching signal");
    bsignal_global.handler(bsignal_global.handler_user);
}

static BOOL ctrl_handler (DWORD type)
{
    ASSERT(bsignal_global.initialized)
    
    EnterCriticalSection(&bsignal_global.handler_mutex);
    
    EnterCriticalSection(&bsignal_global.state_mutex);
    if (bsignal_global.capturing) {
        ASSERT(!bsignal_global.signal_pending)
        bsignal_global.signal_pending = 1;
        ASSERT_FORCE(ReleaseSemaphore(bsignal_global.signal_sem1, 1, NULL))
        LeaveCriticalSection(&bsignal_global.state_mutex);
        
        ASSERT_FORCE(WaitForSingleObject(bsignal_global.signal_sem2, INFINITE) == WAIT_OBJECT_0)
    } else {
        LeaveCriticalSection(&bsignal_global.state_mutex);
    }
    
    LeaveCriticalSection(&bsignal_global.handler_mutex);
    
    return TRUE;
}

#else

static void signal_fd_handler (void *user, int events)
{
    ASSERT(bsignal_global.initialized)
    ASSERT(bsignal_global.capturing)
    ASSERT(bsignal_global.handler)
    
    do {
        struct signalfd_siginfo siginfo;
        int bytes = read(bsignal_global.signal_fd, &siginfo, sizeof(siginfo));
        if (bytes < 0) {
            int error = errno;
            if (error == EAGAIN || error == EWOULDBLOCK) {
                break;
            }
            ASSERT_FORCE(0)
        }
        ASSERT_FORCE(bytes == sizeof(siginfo))
        
        BLog(BLOG_DEBUG, "Dispatching signal");
        bsignal_global.handler(bsignal_global.handler_user);
    } while (bsignal_global.capturing && bsignal_global.handler);
}

#endif

int BSignal_Init (void)
{
    ASSERT(!bsignal_global.initialized)
    
    BLog(BLOG_DEBUG, "BSignal initializing");
    
    #ifdef BADVPN_USE_WINAPI
    
    InitializeCriticalSection(&bsignal_global.handler_mutex);
    InitializeCriticalSection(&bsignal_global.state_mutex);
    
    bsignal_global.signal_pending = 0;
    
    if (!(bsignal_global.signal_sem1 = CreateSemaphore(NULL, 0, 1, NULL))) {
        BLog(BLOG_ERROR, "CreateSemaphore failed");
        goto fail1;
    }
    
    if (!(bsignal_global.signal_sem2 = CreateSemaphore(NULL, 0, 1, NULL))) {
        BLog(BLOG_ERROR, "CreateSemaphore failed");
        goto fail2;
    }
    
    BHandle_Init(&bsignal_global.bhandle, bsignal_global.signal_sem1, signal_handle_handler, NULL);
    
    #else
    
    // create signalfd fd
    sigset_t emptyset;
    FILL_SIGSET(emptyset)
    if ((bsignal_global.signal_fd = signalfd(-1, &emptyset, 0)) < 0) {
        BLog(BLOG_ERROR, "signalfd failed");
        goto fail0;
    }
    
    // set non-blocking
    if (fcntl(bsignal_global.signal_fd, F_SETFL, O_NONBLOCK) < 0) {
        DEBUG("cannot set non-blocking");
        goto fail1;
    }
    
    // init BFileDescriptor
    BFileDescriptor_Init(&bsignal_global.bfd, bsignal_global.signal_fd, signal_fd_handler, NULL);
    
    #endif
    
    bsignal_global.capturing = 0;
    bsignal_global.initialized = 1;
    
    return 1;
    
    #ifdef BADVPN_USE_WINAPI
fail2:
    ASSERT_FORCE(CloseHandle(bsignal_global.signal_sem1))
fail1:
    DeleteCriticalSection(&bsignal_global.state_mutex);
    DeleteCriticalSection(&bsignal_global.handler_mutex);
    #else
fail1:
    ASSERT_FORCE(close(bsignal_global.signal_fd) == 0)
    #endif
    
fail0:
    return 0;
}

void BSignal_Capture (void)
{
    ASSERT(bsignal_global.initialized)
    ASSERT(!bsignal_global.capturing)
    
    BLog(BLOG_DEBUG, "BSignal capturing");
    
    #ifdef BADVPN_USE_WINAPI
    
    ASSERT(!bsignal_global.signal_pending)
    
    EnterCriticalSection(&bsignal_global.state_mutex);
    bsignal_global.capturing = 1;
    LeaveCriticalSection(&bsignal_global.state_mutex);
    ASSERT_FORCE(SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrl_handler, TRUE))
    
    #else
    
    sigset_t signals;
    FILL_SIGSET(signals)
    ASSERT_FORCE(sigprocmask(SIG_BLOCK, &signals, NULL) == 0)
    
    bsignal_global.capturing = 1;
    
    #endif
    
    bsignal_global.handler = NULL;
}

void BSignal_Uncapture (void)
{
    ASSERT(bsignal_global.initialized)
    ASSERT(bsignal_global.capturing)
    ASSERT(!bsignal_global.handler)
    
    BLog(BLOG_DEBUG, "BSignal uncapturing");
    
    #ifdef BADVPN_USE_WINAPI
    
    ASSERT_FORCE(SetConsoleCtrlHandler(NULL, FALSE))
    EnterCriticalSection(&bsignal_global.state_mutex);
    
    if (bsignal_global.signal_pending) {
        bsignal_global.signal_pending = 0;
        // consume sem1 which the handler incremented to prevent
        // the event loop from reacting to the signal
        ASSERT_FORCE(WaitForSingleObject(bsignal_global.signal_sem1, INFINITE) == WAIT_OBJECT_0)
        // allow the handler to return
        // no need to wait for it; it only waits on sem2, which nothing else
        // needs until it returns
        ASSERT_FORCE(ReleaseSemaphore(bsignal_global.signal_sem2, 1, NULL))
    }
    
    #else
    
    sigset_t signals;
    FILL_SIGSET(signals)
    ASSERT_FORCE(sigprocmask(SIG_UNBLOCK, &signals, NULL) == 0)
    
    #endif
    
    bsignal_global.capturing = 0;
    
    #ifdef BADVPN_USE_WINAPI
    
    LeaveCriticalSection(&bsignal_global.state_mutex);
    
    #endif
}

int BSignal_SetHandler (BReactor *reactor, BSignal_handler handler, void *user)
{
    ASSERT(bsignal_global.initialized)
    ASSERT(bsignal_global.capturing)
    ASSERT(!bsignal_global.handler)
    ASSERT(handler)
    
    #ifdef BADVPN_USE_WINAPI
    
    if (!BReactor_AddHandle(reactor, &bsignal_global.bhandle)) {
        BLog(BLOG_ERROR, "BReactor_AddHandle failed");
        return 0;
    }
    
    BReactor_EnableHandle(reactor, &bsignal_global.bhandle);
    
    #else
    
    if (!BReactor_AddFileDescriptor(reactor, &bsignal_global.bfd)) {
        BLog(BLOG_ERROR, "BReactor_AddFileDescriptor failed");
        return 0;
    }
    
    BReactor_SetFileDescriptorEvents(reactor, &bsignal_global.bfd, BREACTOR_READ);
    
    #endif
    
    bsignal_global.handler = handler;
    bsignal_global.handler_user = user;
    bsignal_global.handler_reactor = reactor;
    
    return 1;
}

void BSignal_RemoveHandler (void)
{
    ASSERT(bsignal_global.initialized)
    ASSERT(bsignal_global.capturing)
    ASSERT(bsignal_global.handler)
    
    #ifdef BADVPN_USE_WINAPI
    
    BReactor_RemoveHandle(bsignal_global.handler_reactor, &bsignal_global.bhandle);
    
    #else
    
    BReactor_RemoveFileDescriptor(bsignal_global.handler_reactor, &bsignal_global.bfd);
    
    #endif
    
    bsignal_global.handler = NULL;
}
