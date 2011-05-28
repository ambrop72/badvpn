/**
 * @file BNetwork.c
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
#include <winsock2.h>
#include <mswsock.h>
#else
#include <string.h>
#include <signal.h>
#endif

#include <misc/debug.h>
#include <base/BLog.h>

#include <system/BNetwork.h>

#include <generated/blog_channel_BNetwork.h>

int bnetwork_initialized = 0;

int BNetwork_GlobalInit (void)
{
    ASSERT(!bnetwork_initialized)
    
#ifdef BADVPN_USE_WINAPI
    
    WORD requested = MAKEWORD(2, 2);
    WSADATA wsadata;
    if (WSAStartup(requested, &wsadata) != 0) {
        BLog(BLOG_ERROR, "WSAStartup failed");
        goto fail0;
    }
    if (wsadata.wVersion != requested) {
        BLog(BLOG_ERROR, "WSAStartup returned wrong version");
        goto fail1;
    }
    
#else
    
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(SIGPIPE, &act, NULL) < 0) {
        BLog(BLOG_ERROR, "sigaction failed");
        goto fail0;
    }
    
#endif
    
    bnetwork_initialized = 1;
    
    return 1;
    
#ifdef BADVPN_USE_WINAPI
fail1:
    WSACleanup();
#endif
    
fail0:
    return 0;
}

void BNetwork_Assert (void)
{
    ASSERT(bnetwork_initialized)
}
