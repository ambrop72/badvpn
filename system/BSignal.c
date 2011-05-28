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
#include <system/BUnixSignal.h>
#endif

#include <misc/debug.h>
#include <base/BLog.h>

#include <system/BSignal.h>

#include <generated/blog_channel_BSignal.h>

struct {
    int initialized;
    int finished;
    BReactor *reactor;
    BSignal_handler handler;
    void *user;
    #ifdef BADVPN_USE_WINAPI
    BReactorIOCPOverlapped olap;
    CRITICAL_SECTION iocp_handle_mutex;
    HANDLE iocp_handle;
    #else
    BUnixSignal signal;
    #endif
} bsignal_global = {
    .initialized = 0,
};

#ifdef BADVPN_USE_WINAPI

static void olap_handler (void *user, int event, DWORD bytes)
{
    ASSERT(bsignal_global.initialized)
    ASSERT(!(event == BREACTOR_IOCP_EVENT_EXITING) || bsignal_global.finished)
    
    if (event == BREACTOR_IOCP_EVENT_EXITING) {
        BReactorIOCPOverlapped_Free(&bsignal_global.olap);
        return;
    }
    
    if (!bsignal_global.finished) {
        // call handler
        bsignal_global.handler(bsignal_global.user);
        return;
    }
}

static BOOL WINAPI ctrl_handler (DWORD type)
{
    EnterCriticalSection(&bsignal_global.iocp_handle_mutex);
    
    if (bsignal_global.iocp_handle) {
        PostQueuedCompletionStatus(bsignal_global.iocp_handle, 0, 0, &bsignal_global.olap.olap);
    }
    
    LeaveCriticalSection(&bsignal_global.iocp_handle_mutex);
    
    return TRUE;
}

#else

static void unix_signal_handler (void *user, int signo)
{
    ASSERT(signo == SIGTERM || signo == SIGINT)
    ASSERT(bsignal_global.initialized)
    ASSERT(!bsignal_global.finished)
    
    BLog(BLOG_DEBUG, "Dispatching signal");
    
    // call handler
    bsignal_global.handler(bsignal_global.user);
    return;
}

#endif

int BSignal_Init (BReactor *reactor, BSignal_handler handler, void *user) 
{
    ASSERT(!bsignal_global.initialized)
    
    // init arguments
    bsignal_global.reactor = reactor;
    bsignal_global.handler = handler;
    bsignal_global.user = user;
    
    BLog(BLOG_DEBUG, "BSignal initializing");
    
    #ifdef BADVPN_USE_WINAPI
    
    // init olap
    BReactorIOCPOverlapped_Init(&bsignal_global.olap, bsignal_global.reactor, NULL, olap_handler);
    
    // init handler mutex
    InitializeCriticalSection(&bsignal_global.iocp_handle_mutex);
    
    // remember IOCP handle
    bsignal_global.iocp_handle = BReactor_GetIOCPHandle(bsignal_global.reactor);
    
    // configure ctrl handler
    if (!SetConsoleCtrlHandler(ctrl_handler, TRUE)) {
        BLog(BLOG_ERROR, "SetConsoleCtrlHandler failed");
        goto fail1;
    }
    
    #else
    
    sigset_t sset;
    ASSERT_FORCE(sigemptyset(&sset) == 0)
    ASSERT_FORCE(sigaddset(&sset, SIGTERM) == 0)
    ASSERT_FORCE(sigaddset(&sset, SIGINT) == 0)
    
    // init BUnixSignal
    if (!BUnixSignal_Init(&bsignal_global.signal, bsignal_global.reactor, sset, unix_signal_handler, NULL)) {
        BLog(BLOG_ERROR, "BUnixSignal_Init failed");
        goto fail0;
    }
    
    #endif
    
    bsignal_global.initialized = 1;
    bsignal_global.finished = 0;
    
    return 1;
    
    #ifdef BADVPN_USE_WINAPI
fail1:
    DeleteCriticalSection(&bsignal_global.iocp_handle_mutex);
    BReactorIOCPOverlapped_Free(&bsignal_global.olap);
    #endif
    
fail0:
    return 0;
}

void BSignal_Finish (void)
{
    ASSERT(bsignal_global.initialized)
    ASSERT(!bsignal_global.finished)
    
    #ifdef BADVPN_USE_WINAPI
    
    // forget IOCP handle
    EnterCriticalSection(&bsignal_global.iocp_handle_mutex);
    bsignal_global.iocp_handle = NULL;
    LeaveCriticalSection(&bsignal_global.iocp_handle_mutex);
    
    #else
    
    // free BUnixSignal
    BUnixSignal_Free(&bsignal_global.signal, 0);
    
    #endif
    
    bsignal_global.finished = 1;
}
