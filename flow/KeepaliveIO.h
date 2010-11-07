/**
 * @file KeepaliveIO.h
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
 * A {@link PacketPassInterface} layer for sending keep-alive packets.
 */

#ifndef BADVPN_FLOW_KEEPALIVEIO
#define BADVPN_FLOW_KEEPALIVEIO

#include <misc/debug.h>
#include <system/DebugObject.h>
#include <system/BReactor.h>
#include <flow/PacketPassInterface.h>
#include <flow/PacketRecvInterface.h>
#include <flow/PacketPassPriorityQueue.h>
#include <flow/SinglePacketBuffer.h>
#include <flow/PacketRecvBlocker.h>
#include <flow/PacketPassInactivityMonitor.h>

/**
 * A {@link PacketPassInterface} layer for sending keep-alive packets.
 */
typedef struct {
    BReactor *reactor;
    PacketPassInactivityMonitor kasender;
    PacketPassPriorityQueue queue;
    PacketPassPriorityQueueFlow user_qflow;
    PacketPassPriorityQueueFlow ka_qflow;
    SinglePacketBuffer ka_buffer;
    PacketRecvBlocker ka_blocker;
    DebugObject d_obj;
} KeepaliveIO;

/**
 * Initializes the object.
 *
 * @param o the object
 * @param reactor reactor we live in
 * @param output output interface
 * @param keepalive_input keepalive input interface. Its MTU must be <= MTU of output.
 * @param keepalive_interval_ms keepalive interval in milliseconds. Must be >0.
 * @return 1 on success, 0 on failure
 */
int KeepaliveIO_Init (KeepaliveIO *o, BReactor *reactor, PacketPassInterface *output, PacketRecvInterface *keepalive_input, btime_t keepalive_interval_ms) WARN_UNUSED;

/**
 * Frees the object.
 *
 * @param o the object
 */
void KeepaliveIO_Free (KeepaliveIO *o);

/**
 * Returns the input interface.
 *
 * @param o the object
 * @return input interface
 */
PacketPassInterface * KeepaliveIO_GetInput (KeepaliveIO *o);

#endif
