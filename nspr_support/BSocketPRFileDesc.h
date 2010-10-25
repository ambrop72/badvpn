/**
 * @file BSocketPRFileDesc.h
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
 * NSPR file descriptor (PRFileDesc) for {@link BSocket} stream sockets.
 */

#ifndef BADVPN_NSPRSUPPORT_BSOCKETPRFILEDESC_H
#define BADVPN_NSPRSUPPORT_BSOCKETPRFILEDESC_H

#include <prio.h>

#include <misc/debug.h>
#include <system/BSocket.h>

extern PRDescIdentity bsocketprfiledesc_identity;

/**
 * Globally initializes the {@link BSocket} NSPR file descriptor backend.
 * Must not have been called successfully.
 *
 * @return 1 on success, 0 on failure
 */
int BSocketPRFileDesc_GlobalInit (void) WARN_UNUSED;

/**
 * Creates a NSPR file descriptor using {@link BSocket} for I/O.
 * {@link BSocketPRFileDesc_GlobalInit} must have been done.
 *
 * @param prfd uninitialized PRFileDesc structure
 * @param bsock socket to use. The socket should be a stream socket.
 */
void BSocketPRFileDesc_Create (PRFileDesc *prfd, BSocket *bsock);

#endif
