/**
 * @file BIPCServer.c
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

#include <system/BLog.h>

#include <ipc/BIPCServer.h>

#include <generated/blog_channel_BIPCServer.h>

static void listener_handler (BIPCServer *o)
{
    DebugObject_Access(&o->d_obj);
    
    o->handler(o->user);
    return;
}

int BIPCServer_Init (BIPCServer *o, const char *path, BIPCServer_handler handler, void *user, BReactor *reactor)
{
    // init arguments
    o->handler = handler;
    o->user = user;
    
    // init socket
    if (BSocket_Init(&o->sock, reactor, BADDR_TYPE_UNIX, BSOCKET_TYPE_STREAM) < 0) {
        BLog(BLOG_ERROR, "BSocket_Init failed");
        goto fail0;
    }
    
    // bind socket
    if (BSocket_BindUnix(&o->sock, path) < 0) {
        BLog(BLOG_ERROR, "BSocket_BindUnix failed (%d)", BSocket_GetError(&o->sock));
        goto fail1;
    }
    
    // listen socket
    if (BSocket_Listen(&o->sock, -1) < 0) {
        BLog(BLOG_ERROR, "BSocket_Listen failed (%d)", BSocket_GetError(&o->sock));
        goto fail1;
    }
    
    // init listener
    Listener_InitExisting(&o->listener, reactor, &o->sock, (Listener_handler)listener_handler, o);
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    BSocket_Free(&o->sock);
fail0:
    return 0;
}

void BIPCServer_Free (BIPCServer *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free listener
    Listener_Free(&o->listener);
    
    // free socket
    BSocket_Free(&o->sock);
}
