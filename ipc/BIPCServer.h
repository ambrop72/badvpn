/**
 * @file BIPCServer.h
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
 * 
 * @section DESCRIPTION
 * 
 * Server part of {@link BIPC}, an abstraction of a reliable, sequenced,
 * message-oriented two-way IPC.
 */

#ifndef BADVPN_IPC_BIPCSERVER_H
#define BADVPN_IPC_BIPCSERVER_H

#include <misc/debug.h>
#include <system/BSocket.h>
#include <system/Listener.h>
#include <system/DebugObject.h>

/**
 * Handler function called when a client may be accepted
 * (using {@link BIPC_InitAccept}).
 * 
 * @param user as in {@link BIPCServer_Init}
 */
typedef void (*BIPCServer_handler) (void *user);

/**
 * Server part of {@link BIPC}, an abstraction of a reliable, sequenced,
 * message-oriented two-way IPC.
 */
typedef struct {
    BSocket sock;
    Listener listener;
    BIPCServer_handler handler;
    void *user;
    DebugObject d_obj;
} BIPCServer;

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param path path of the IPC object. On *nix path of the unix socket, on Windows
 *             path of the named pipe.
 * @param handler handler function called when a client may be accepted
 *                (using {@link BIPC_InitAccept})
 * @param user value to pass to handler function
 * @param reactor reactor we live in
 * @return 1 on success, 0 on failure
 */
int BIPCServer_Init (BIPCServer *o, const char *path, BIPCServer_handler handler, void *user, BReactor *reactor) WARN_UNUSED;

/**
 * Frees the object.
 * 
 * @param o the object
 */
void BIPCServer_Free (BIPCServer *o);

#endif
