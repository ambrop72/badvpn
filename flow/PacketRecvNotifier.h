/**
 * @file PacketRecvNotifier.h
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
 * A {@link PacketRecvInterface} layer that calls a handler function before
 * providing a packet to output.
 */

#ifndef BADVPN_FLOW_PACKETRECVNOTIFIER_H
#define BADVPN_FLOW_PACKETRECVNOTIFIER_H

#include <stdint.h>

#include <system/DebugObject.h>
#include <flow/PacketRecvInterface.h>

/**
 * Handler function called when input has provided a packet (i.e. by returning
 * 1 from Recv or calling Done), but before passing the packet on to output.
 * 
 * @param user value specified in {@link PacketRecvNotifier_SetHandler}
 * @param data packet provided by output (buffer provided by input)
 * @param data_len size of the packet
 */
typedef void (*PacketRecvNotifier_handler_notify) (void *user, uint8_t *data, int data_len);

/**
 * A {@link PacketRecvInterface} layer that calls a handler function before
 * providing a packet to output.
 */
typedef struct {
    PacketRecvInterface output;
    PacketRecvInterface *input;
    PacketRecvNotifier_handler_notify handler;
    void *handler_user;
    uint8_t *out;
    DebugObject d_obj;
} PacketRecvNotifier;

/**
 * Initializes the object.
 *
 * @param o the object
 * @param input input interface
 * @param pg pending group
 */
void PacketRecvNotifier_Init (PacketRecvNotifier *o, PacketRecvInterface *input, BPendingGroup *pg);

/**
 * Frees the object.
 *
 * @param o the object
 */
void PacketRecvNotifier_Free (PacketRecvNotifier *o);

/**
 * Returns the output interface.
 * The MTU of the output interface will be the same as of the input interface.
 *
 * @param o the object
 * @return output interface
 */
PacketRecvInterface * PacketRecvNotifier_GetOutput (PacketRecvNotifier *o);

/**
 * Configures a handler function to invoke before passing output packets to input.
 *
 * @param o the object
 * @param handler handler function, or NULL to disable.
 * @param user value to pass to handler function. Ignored if handler is NULL.
 */
void PacketRecvNotifier_SetHandler (PacketRecvNotifier *o, PacketRecvNotifier_handler_notify handler, void *user);

#endif
