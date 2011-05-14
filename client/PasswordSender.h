/**
 * @file PasswordSender.h
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
 * Object used to send a password to a {@link PasswordListener} server.
 */

#ifndef BADVPN_CLIENT_PASSWORDSENDER_H
#define BADVPN_CLIENT_PASSWORDSENDER_H

#include <stdint.h>

#include <misc/debugerror.h>
#include <system/BSocket.h>
#include <base/DebugObject.h>
#include <flow/SinglePacketSender.h>
#include <flow/PacketStreamSender.h>
#include <flowextra/StreamSocketSink.h>
#include <nspr_support/BPRFileDesc.h>
#include <nspr_support/PRStreamSink.h>

/**
 * Handler function called when the password is sent, or an error occurs
 * on the socket.
 * The object must be freed from within this handler.
 * 
 * @param user as in {@link PasswordSender_Init}
 * @param is_error whether the password was sent successfuly, or an error
 *                 occured on the socket. 0 means password sent, 1 means error.
 */
typedef void (*PasswordSender_handler) (void *user, int is_error);

/**
 * Object used to send a password to a {@link PasswordListener} server.
 */
typedef struct {
    uint64_t password;
    int ssl;
    union {
        BSocket *plain_sock;
        BPRFileDesc *ssl_bprfd;
    };
    PasswordSender_handler handler;
    void *user;
    FlowErrorDomain domain;
    SinglePacketSender sps;
    PacketStreamSender pss;
    union {
        StreamSocketSink plain;
        PRStreamSink ssl;
    } sink;
    DebugObject d_obj;
    DebugError d_err;
} PasswordSender;

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param password password to send
 * @param ssl whether we are connected to the server using TLS. Must be 1 or 0.
 * @param plain_sock if not using TLS, the socket to send the password through. Nothing else
 *                   must be using this socket for sending.
 * @param ssl_bprfd if using TLS, the {@link BPRFileDesc} object for the SSL file descriptor
 *                  to send the password through. Nothing else must be using this SSL socket
 *                  for sending.
 * @param handler handler to call when the password is sent or an error occurs
 * @param user value to pass to handler
 * @param reactor reactor we live in
 */
void PasswordSender_Init (PasswordSender *o, uint64_t password, int ssl, BSocket *plain_sock, BPRFileDesc *ssl_bprfd, PasswordSender_handler handler, void *user, BReactor *reactor);

/**
 * Frees the object.
 * 
 * @param o the object
 */
void PasswordSender_Free (PasswordSender *o);

#endif
