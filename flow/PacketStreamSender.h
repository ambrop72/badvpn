/**
 * @file PacketStreamSender.h
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
 * Object which forwards packets obtained with {@link PacketPassInterface}
 * as a stream with {@link StreamPassInterface} (i.e. it concatenates them).
 */

#ifndef BADVPN_FLOW_PACKETSTREAMSENDER_H
#define BADVPN_FLOW_PACKETSTREAMSENDER_H

#include <stdint.h>

#include <misc/dead.h>
#include <system/DebugObject.h>
#include <flow/PacketPassInterface.h>
#include <flow/StreamPassInterface.h>

/**
 * Object which forwards packets obtained with {@link PacketPassInterface}
 * as a stream with {@link StreamPassInterface} (i.e. it concatenates them).
 */
typedef struct {
    DebugObject d_obj;
    dead_t dead;
    PacketPassInterface input;
    StreamPassInterface *output;
    int in_len;
    uint8_t *in;
    int in_used;
} PacketStreamSender;

/**
 * Initializes the object.
 *
 * @param s the object
 * @param output output interface
 * @param mtu input MTU. Must be >=0.
 */
void PacketStreamSender_Init (PacketStreamSender *s, StreamPassInterface *output, int mtu);

/**
 * Frees the object.
 *
 * @param s the object
 */
void PacketStreamSender_Free (PacketStreamSender *s);

/**
 * Returns the input interface.
 * Its MTU will be as in {@link PacketStreamSender_Init}.
 *
 * @param s the object
 * @return input interface
 */
PacketPassInterface * PacketStreamSender_GetInput (PacketStreamSender *s);

#endif
