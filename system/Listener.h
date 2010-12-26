/**
 * @file Listener.h
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
 * Object used to listen on a socket and accept clients.
 */

#ifndef BADVPN_SYSTEM_LISTENER_H
#define BADVPN_SYSTEM_LISTENER_H

#include <system/DebugObject.h>
#include <system/BSocket.h>

/**
 * Handler function called when it may be possible to accept a client.
 * The user can call {@link Listener_Accept} from this handler to accept
 * clients.
 * If the user does not call {@link Listener_Accept}, a newly connected
 * client may be disconnected.
 * 
 * @param user as in {@link Listener_Init}
 */
typedef void (*Listener_handler) (void *user);

/**
 * Object used to listen on a socket and accept clients.
 */
typedef struct {
    BReactor *reactor;
    int existing;
    BSocket our_sock;
    BSocket *sock;
    Listener_handler handler;
    void *user;
    BPending accept_job;
    DebugObject d_obj;
} Listener;

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param reactor reactor we live in
 * @param addr address to listen on. Must not be invalid.
 * @param handler handler function called when a connection should be accepted
 * @param user value to pass to handler function
 * @return 1 on success, 0 on failure
 */
int Listener_Init (Listener *o, BReactor *reactor, BAddr addr, Listener_handler handler, void *user) WARN_UNUSED;

/**
 * Initializes the object for listening on an existing socket.
 * The socket should be already bound and listened.
 * 
 * @param o the object
 * @param reactor reactor we live in
 * @param sock socket to listen on
 * @param handler handler function called when a connection should be accepted
 * @param user value to pass to handler function
 * @return 1 on success, 0 on failure
 */
void Listener_InitExisting (Listener *o, BReactor *reactor, BSocket *sock, Listener_handler handler, void *user);

/**
 * Frees the object.
 * 
 * @param o the object
 */
void Listener_Free (Listener *o);

/**
 * Accepts a connection.
 * Must be called from within the {@link Listener_handler} handler or its jobs, and
 * at most once.
 * 
 * @param o the object
 * @param sockout uninitialized {@link BSocket} structure to put the new socket in.
 *                Must not be NULL.
 * @param addrout if not NULL, the address of the client will be returned here
 *                on success
 * @return 1 on success, 0 on failure
 */
int Listener_Accept (Listener *o, BSocket *sockout, BAddr *addrout) WARN_UNUSED;

#endif
