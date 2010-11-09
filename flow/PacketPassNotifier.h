/**
 * @file PacketPassNotifier.h
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
 * A {@link PacketPassInterface} layer which calles a handler function before
 * passing a packet from input to output.
 */

#ifndef BADVPN_FLOW_PACKETPASSNOTIFIER_H
#define BADVPN_FLOW_PACKETPASSNOTIFIER_H

#include <stdint.h>

#include <system/DebugObject.h>
#include <flow/PacketPassInterface.h>

/**
 * Handler function called when input calls Send, but before the call is passed on to output.
 * 
 * @param user value specified in {@link PacketPassNotifier_SetHandler}
 * @param data packet provided by input
 * @param data_len size of the packet
 */
typedef void (*PacketPassNotifier_handler_notify) (void *user, uint8_t *data, int data_len);

/**
 * A {@link PacketPassInterface} layer which calles a handler function before
 * passing a packet from input to output.
 */
typedef struct {
    PacketPassInterface input;
    PacketPassInterface *output;
    PacketPassNotifier_handler_notify handler;
    void *handler_user;
    DebugObject d_obj;
    #ifndef NDEBUG
    int d_in_have;
    #endif
} PacketPassNotifier;

/**
 * Initializes the object.
 *
 * @param o the object
 * @param output output interface
 * @param pg pending group
 */
void PacketPassNotifier_Init (PacketPassNotifier *o, PacketPassInterface *output, BPendingGroup *pg);

/**
 * Frees the object.
 *
 * @param o the object
 */
void PacketPassNotifier_Free (PacketPassNotifier *o);

/**
 * Returns the input interface.
 * The MTU of the interface will be the same as of the output interface.
 * The interface supports cancel functionality if the output interface supports it.
 *
 * @param o the object
 * @return input interface
 */
PacketPassInterface * PacketPassNotifier_GetInput (PacketPassNotifier *o);

/**
 * Configures a handler function to call before passing input packets to output.
 *
 * @param o the object
 * @param handler handler function, or NULL to disable.
 * @param user value to pass to handler function. Ignored if handler is NULL.
 */
void PacketPassNotifier_SetHandler (PacketPassNotifier *o, PacketPassNotifier_handler_notify handler, void *user);

#endif
