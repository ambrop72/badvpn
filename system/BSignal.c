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
#include <system/BLog.h>

#include <system/BSignal.h>

#include <generated/blog_channel_BSignal.h>

struct {
    int initialized;
    int finished;
    BReactor *reactor;
    BSignal_handler handler;
    void *user;
    #ifdef BADVPN_USE_WINAPI
    CRITICAL_SECTION handler_mutex; // mutex to make sure only one handler is working at a time
    HANDLE signal_sem1;
    HANDLE signal_sem2;
    BHandle bhandle;
    #else
    BUnixSignal signal;
    #endif
} bsignal_global = {
    .initialized = 0,
};

#ifdef BADVPN_USE_WINAPI

static void signal_handle_handler (void *user)
{
    ASSERT(bsignal_global.initialized)
    ASSERT(!bsignal_global.finished)
    
    ASSERT_FORCE(ReleaseSemaphore(bsignal_global.signal_sem2, 1, NULL))
    
    BLog(BLOG_DEBUG, "Dispatching signal");
    
    // call handler
    bsignal_global.handler(bsignal_global.user);
    return;
}

static BOOL WINAPI ctrl_handler (DWORD type)
{
    // don't check bsignal_global.initialized to avoid a race
    
    EnterCriticalSection(&bsignal_global.handler_mutex);
    
    ASSERT_FORCE(ReleaseSemaphore(bsignal_global.signal_sem1, 1, NULL))
    ASSERT_FORCE(WaitForSingleObject(bsignal_global.signal_sem2, INFINITE) == WAIT_OBJECT_0)
    
    LeaveCriticalSection(&bsignal_global.handler_mutex);
    
    return TRUE;
}

#else

static void unix_signal_handler (void *user, struct BUnixSignal_siginfo siginfo)
{
    ASSERT(siginfo.signo == SIGTERM || siginfo.signo == SIGINT)
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
    
    InitializeCriticalSection(&bsignal_global.handler_mutex);
    
    if (!(bsignal_global.signal_sem1 = CreateSemaphore(NULL, 0, 1, NULL))) {
        BLog(BLOG_ERROR, "CreateSemaphore failed");
        goto fail1;
    }
    
    if (!(bsignal_global.signal_sem2 = CreateSemaphore(NULL, 0, 1, NULL))) {
        BLog(BLOG_ERROR, "CreateSemaphore failed");
        goto fail2;
    }
    
    // init BHandle
    BHandle_Init(&bsignal_global.bhandle, bsignal_global.signal_sem1, signal_handle_handler, NULL);
    if (!BReactor_AddHandle(bsignal_global.reactor, &bsignal_global.bhandle)) {
        BLog(BLOG_ERROR, "BReactor_AddHandle failed");
        goto fail3;
    }
    BReactor_EnableHandle(bsignal_global.reactor, &bsignal_global.bhandle);
    
    // configure ctrl handler
    if (!SetConsoleCtrlHandler(ctrl_handler, TRUE)) {
        BLog(BLOG_ERROR, "SetConsoleCtrlHandler failed");
        goto fail4;
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
fail4:
    BReactor_RemoveHandle(bsignal_global.reactor, &bsignal_global.bhandle);
fail3:
    ASSERT_FORCE(CloseHandle(bsignal_global.signal_sem2))
fail2:
    ASSERT_FORCE(CloseHandle(bsignal_global.signal_sem1))
fail1:
    DeleteCriticalSection(&bsignal_global.handler_mutex);
    #endif
    
fail0:
    return 0;
}

void BSignal_Finish (void)
{
    ASSERT(bsignal_global.initialized)
    ASSERT(!bsignal_global.finished)
    
    #ifdef BADVPN_USE_WINAPI
    
    // free BHandle
    BReactor_RemoveHandle(bsignal_global.reactor, &bsignal_global.bhandle);
    
    #else
    
    // free BUnixSignal
    BUnixSignal_Free(&bsignal_global.signal, 0);
    
    #endif
    
    bsignal_global.finished = 1;
}
