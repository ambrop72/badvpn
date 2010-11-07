/**
 * @file SinglePacketSender.h
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
 * A {@link PacketPassInterface} source which sends a single packet.
 */

#ifndef BADVPN_FLOW_SINGLEPACKETSENDER_H
#define BADVPN_FLOW_SINGLEPACKETSENDER_H

#include <stdint.h>

#include <misc/dead.h>
#include <system/DebugObject.h>
#include <flow/PacketPassInterface.h>

/**
 * Handler function called after the packet is sent.
 * The object must be freed from within this handler.
 * 
 * @param user as in {@link SinglePacketSender_Init}.
 */
typedef void (*SinglePacketSender_handler) (void *user);

/**
 * A {@link PacketPassInterface} source which sends a single packet.
 */
typedef struct {
    PacketPassInterface *output;
    SinglePacketSender_handler handler;
    void *user;
    DebugObject d_obj;
    #ifndef NDEBUG
    dead_t d_dead;
    #endif
} SinglePacketSender;

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param packet packet to be sent. Must be available as long as the object exists.
 * @param packet_len length of the packet. Must be >=0 and <=(MTU of output).
 * @param output output interface
 * @param handler handler to call when the packet is sent
 * @param user value to pass to handler
 * @param pg pending group
 */
void SinglePacketSender_Init (SinglePacketSender *o, uint8_t *packet, int packet_len, PacketPassInterface *output, SinglePacketSender_handler handler, void *user, BPendingGroup *pg);

/**
 * Frees the object.
 * 
 * @param o the object
 */
void SinglePacketSender_Free (SinglePacketSender *o);

#endif
