/**
 * @file Listener.c
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

#include <stddef.h>

#include <misc/debug.h>
#include <system/BLog.h>

#include <listener/Listener.h>

#include <generated/blog_channel_Listener.h>

static void socket_handler (Listener *o, int event)
{
    ASSERT(event == BSOCKET_ACCEPT)
    
    o->accepted = 0;
    
    DebugIn_GoIn(&o->d_in_handler);
    DEAD_ENTER(o->dead)
    o->handler(o->user);
    if (DEAD_LEAVE(o->dead)) {
        return;
    }
    DebugIn_GoOut(&o->d_in_handler);
    
    // if there was no attempt to accept, do it now, discarding the client
    if (!o->accepted) {
        if (BSocket_Accept(&o->sock, NULL, NULL) < 0) {
            BLog(BLOG_ERROR, "BSocket_Accept failed (%d)", BSocket_GetError(&o->sock));
        }
    }
}

int Listener_Init (Listener *o, BReactor *reactor, BAddr addr, Listener_handler handler, void *user)
{
    ASSERT(!BAddr_IsInvalid(&addr))
    
    // init arguments
    o->reactor = reactor;
    o->handler = handler;
    o->user = user;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // create socket
    if (BSocket_Init(&o->sock, o->reactor, addr.type, BSOCKET_TYPE_STREAM) < 0) {
        BLog(BLOG_ERROR, "BSocket_Init failed");
        goto fail0;
    }
    
    // bind socket
    if (BSocket_Bind(&o->sock, &addr) < 0) {
        BLog(BLOG_ERROR, "BSocket_Bind failed (%d)", BSocket_GetError(&o->sock));
        goto fail1;
    }
    
    // listen socket
    if (BSocket_Listen(&o->sock, -1) < 0) {
        BLog(BLOG_ERROR, "BSocket_Listen failed (%d)", BSocket_GetError(&o->sock));
        goto fail1;
    }
    
    // register socket event handler
    BSocket_AddEventHandler(&o->sock, BSOCKET_ACCEPT, (BSocket_handler)socket_handler, o);
    BSocket_EnableEvent(&o->sock, BSOCKET_ACCEPT);
    
    // init debug in handler
    DebugIn_Init(&o->d_in_handler);
    
    // init debug object
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    BSocket_Free(&o->sock);
fail0:
    return 0;
}

void Listener_Free (Listener *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);
    
    // free socket
    BSocket_Free(&o->sock);
    
    // free dead var
    DEAD_KILL(o->dead);
}

int Listener_Accept (Listener *o, BSocket *sockout, BAddr *addrout)
{
    ASSERT(sockout)
    ASSERT(DebugIn_In(&o->d_in_handler))
    
    o->accepted = 1;
    
    if (BSocket_Accept(&o->sock, sockout, addrout) < 0) {
        BLog(BLOG_ERROR, "BSocket_Accept failed (%d)", BSocket_GetError(&o->sock));
        return 0;
    }
    
    return 1;
}
