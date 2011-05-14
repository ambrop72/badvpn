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
#include <base/BLog.h>

#include <system/Listener.h>

#include <generated/blog_channel_Listener.h>

static void socket_handler (Listener *o, int event)
{
    ASSERT(event == BSOCKET_ACCEPT)
    DebugObject_Access(&o->d_obj);
    
    // schedule accept job (to accept and close the connection in case the handler doesn't)
    BPending_Set(&o->accept_job);
    
    // call handler
    o->handler(o->user);
    return;
}

static void accept_job_handler (Listener *o)
{
    DebugObject_Access(&o->d_obj);
    
    // accept and discard the connection
    if (BSocket_Accept(o->sock, NULL, NULL) < 0) {
        BLog(BLOG_ERROR, "BSocket_Accept failed (%d)", BSocket_GetError(o->sock));
    }
}

int Listener_Init (Listener *o, BReactor *reactor, BAddr addr, Listener_handler handler, void *user)
{
    ASSERT(!BAddr_IsInvalid(&addr))
    
    // init arguments
    o->reactor = reactor;
    o->handler = handler;
    o->user = user;
    
    // set not existing
    o->existing = 0;
    
    // create socket
    if (BSocket_Init(&o->our_sock, o->reactor, addr.type, BSOCKET_TYPE_STREAM) < 0) {
        BLog(BLOG_ERROR, "BSocket_Init failed");
        goto fail0;
    }
    
    // set socket
    o->sock = &o->our_sock;
    
    // bind socket
    if (BSocket_Bind(o->sock, &addr) < 0) {
        BLog(BLOG_ERROR, "BSocket_Bind failed (%d)", BSocket_GetError(o->sock));
        goto fail1;
    }
    
    // listen socket
    if (BSocket_Listen(o->sock, -1) < 0) {
        BLog(BLOG_ERROR, "BSocket_Listen failed (%d)", BSocket_GetError(o->sock));
        goto fail1;
    }
    
    // register socket event handler
    BSocket_AddEventHandler(o->sock, BSOCKET_ACCEPT, (BSocket_handler)socket_handler, o);
    BSocket_EnableEvent(o->sock, BSOCKET_ACCEPT);
    
    // init accept job
    BPending_Init(&o->accept_job, BReactor_PendingGroup(o->reactor), (BPending_handler)accept_job_handler, o);
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    BSocket_Free(&o->our_sock);
fail0:
    return 0;
}

void Listener_InitExisting (Listener *o, BReactor *reactor, BSocket *sock, Listener_handler handler, void *user)
{
    // init arguments
    o->reactor = reactor;
    o->handler = handler;
    o->user = user;
    o->sock = sock;
    
    // set existing
    o->existing = 1;
    
    // register socket event handler
    BSocket_AddEventHandler(o->sock, BSOCKET_ACCEPT, (BSocket_handler)socket_handler, o);
    BSocket_EnableEvent(o->sock, BSOCKET_ACCEPT);
    
    // init accept job
    BPending_Init(&o->accept_job, BReactor_PendingGroup(o->reactor), (BPending_handler)accept_job_handler, o);
    
    DebugObject_Init(&o->d_obj);
}

void Listener_Free (Listener *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free accept job
    BPending_Free(&o->accept_job);
    
    // remove socket event handler
    BSocket_RemoveEventHandler(o->sock, BSOCKET_ACCEPT);
    
    if (!o->existing) {
        // free socket
        BSocket_Free(&o->our_sock);
    }
}

int Listener_Accept (Listener *o, BSocket *sockout, BAddr *addrout)
{
    ASSERT(sockout)
    ASSERT(BPending_IsSet(&o->accept_job))
    DebugObject_Access(&o->d_obj);
    
    // unset accept job
    BPending_Unset(&o->accept_job);
    
    if (BSocket_Accept(o->sock, sockout, addrout) < 0) {
        BLog(BLOG_ERROR, "BSocket_Accept failed (%d)", BSocket_GetError(o->sock));
        return 0;
    }
    
    return 1;
}
