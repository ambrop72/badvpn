/**
 * @file PacketRouter.h
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
 * Object which simplifies routing packets to {@link RouteBuffer}'s from a
 * {@link PacketRecvInterface} input.
 */

#ifndef BADVPN_FLOW_PACKETROUTER_H
#define BADVPN_FLOW_PACKETROUTER_H

#include <system/DebugObject.h>
#include <system/BPending.h>
#include <flow/PacketRecvInterface.h>
#include <flow/RouteBuffer.h>

/**
 * Handler called when a packet is received, allowing the user to route it
 * to one or more buffers using {@link PacketRouter_Route}.
 * 
 * @param user as in {@link PacketRouter_Init}
 * @param buf the buffer for the packet. May be modified by the user.
 *            Will have space for mtu bytes. Only valid in the job context of
 *            this handler, until {@link PacketRouter_Route} is called successfully.
 * @param recv_len length of the input packet (located at recv_offset bytes offset)
 */
typedef void (*PacketRouter_handler) (void *user, uint8_t *buf, int recv_len);

/**
 * Object which simplifies routing packets to {@link RouteBuffer}'s from a
 * {@link PacketRecvInterface} input.
 * 
 * Packets are routed by calling {@link PacketRouter_Route} (possibly multiple times)
 * from the job context of the {@link PacketRouter_handler} handler.
 */
typedef struct {
    int mtu;
    int recv_offset;
    PacketRecvInterface *input;
    PacketRouter_handler handler;
    void *user;
    RouteBufferSource rbs;
    BPending next_job;
    DebugObject d_obj;
} PacketRouter;

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param mtu maximum packet size. Must be >=0. It will only be possible to route packets to
 *            {@link RouteBuffer}'s with the same MTU.
 * @param recv_offset offset from the beginning for receiving input packets.
 *                    Must be >=0 and <=mtu. The leading space should be initialized
 *                    by the user before routing a packet.
 * @param input input interface. Its MTU must be <= mtu - recv_offset.
 * @param handler handler called when a packet is received to allow the user to route it
 * @param user value passed to handler
 * @param pg pending group
 * @return 1 on success, 0 on failure
 */
int PacketRouter_Init (PacketRouter *o, int mtu, int recv_offset, PacketRecvInterface *input, PacketRouter_handler handler, void *user, BPendingGroup *pg) WARN_UNUSED;

/**
 * Frees the object.
 * 
 * @param o the object
 */
void PacketRouter_Free (PacketRouter *o);

/**
 * Routes the current packet to the given buffer.
 * Must be called from the job context of the {@link PacketRouter_handler} handler.
 * On success, copies part of the current packet to next one (regardless if next_buf
 * is provided or not; if not, copies before receiving another packet).
 * 
 * @param o the object
 * @param len total packet length (e.g. recv_offset + (recv_len from handler)).
 *            Must be >=0 and <=mtu.
 * @param output buffer to route to. Its MTU must be the same as of this object.
 * @param next_buf if not NULL, on success, will be set to the address of a new current
 *                 packet that can be routed. The pointer will be valid in the job context of
 *                 the calling handler, until this function is called successfully again
 *                 (as for the original pointer provided by the handler).
 * @param copy_offset Offset from the beginning for copying to the next packet.
 *                    Must be >=0 and <=mtu.
 * @param copy_len Number of bytes to copy from the old current
 *                 packet to the next one. Must be >=0 and <= mtu - copy_offset.
 * @return 1 on success, 0 on failure (buffer full)
 */
int PacketRouter_Route (PacketRouter *o, int len, RouteBuffer *output, uint8_t **next_buf, int copy_offset, int copy_len);

/**
 * Asserts that {@link PacketRouter_Route} can be called.
 * 
 * @param o the object
 */
void PacketRouter_AssertRoute (PacketRouter *o);

#endif
