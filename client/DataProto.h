/**
 * @file DataProto.h
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
 * Mudule for frame sending used in the VPN client program.
 */

#ifndef BADVPN_CLIENT_DATAPROTO_H
#define BADVPN_CLIENT_DATAPROTO_H

#include <stdint.h>

#include <misc/debugcounter.h>
#include <misc/debug.h>
#include <system/DebugObject.h>
#include <system/BReactor.h>
#include <flow/PacketPassFairQueue.h>
#include <flow/PacketPassInactivityMonitor.h>
#include <flow/PacketPassNotifier.h>
#include <flow/DataProtoKeepaliveSource.h>
#include <flow/PacketRecvBlocker.h>
#include <flow/SinglePacketBuffer.h>
#include <flow/PacketPassConnector.h>
#include <flow/PacketRouter.h>

typedef void (*DataProtoSink_handler) (void *user, int up);
typedef void (*DataProtoSource_handler) (void *user, const uint8_t *frame, int frame_len);
typedef void (*DataProtoFlow_handler_inactivity) (void *user);

/**
 * Frame destination.
 * Represents a peer as a destination for sending frames to.
 */
typedef struct {
    BReactor *reactor;
    int frame_mtu;
    PacketPassFairQueue queue;
    PacketPassInactivityMonitor monitor;
    PacketPassNotifier notifier;
    DataProtoKeepaliveSource ka_source;
    PacketRecvBlocker ka_blocker;
    SinglePacketBuffer ka_buffer;
    PacketPassFairQueueFlow ka_qflow;
    BTimer receive_timer;
    int up;
    int up_report;
    DataProtoSink_handler handler;
    void *user;
    BPending keepalive_job;
    BPending up_job;
    int freeing;
    DebugObject d_obj;
    DebugCounter d_ctr;
} DataProtoSink;

/**
 * Object that receives frames from a device and routes
 * them to buffers in {@link DataProtoFlow} objects.
 */
typedef struct {
    DataProtoSource_handler handler;
    void *user;
    BReactor *reactor;
    int frame_mtu;
    PacketRouter router;
    uint8_t *current_buf;
    int current_recv_len;
    DebugObject d_obj;
    DebugCounter d_ctr;
} DataProtoSource;

/**
 * Local frame source.
 * Buffers frames received from the TAP device, addressed to a particular peer.
 */
typedef struct {
    DataProtoSource *device;
    peerid_t source_id;
    peerid_t dest_id;
    int inactivity_time;
    RouteBuffer rbuf;
    PacketPassInactivityMonitor monitor;
    PacketPassConnector connector;
    DataProtoSink *dp;
    PacketPassFairQueueFlow dp_qflow;
    DebugObject d_obj;
} DataProtoFlow;

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param reactor reactor we live in
 * @param output output interface. Must support cancel functionality. Its MTU must be
 *               >=DATAPROTO_MAX_OVERHEAD.
 * @param keepalive_time keepalive time
 * @param tolerance_time after how long of not having received anything from the peer
 *                       to consider the link down
 * @param handler up state handler
 * @param user value to pass to handler
 * @return 1 on success, 0 on failure
 */
int DataProtoSink_Init (DataProtoSink *o, BReactor *reactor, PacketPassInterface *output, btime_t keepalive_time, btime_t tolerance_time, DataProtoSink_handler handler, void *user) WARN_UNUSED;

/**
 * Frees the object.
 * There must be no local sources attached.
 * 
 * @param o the object
 */
void DataProtoSink_Free (DataProtoSink *o);

/**
 * Prepares for freeing the object by allowing freeing of local sources.
 * The object enters freeing state.
 * The object must be freed before returning control to the reactor,
 * and before any further I/O (output or submitting frames).
 * 
 * @param o the object
 */
void DataProtoSink_PrepareFree (DataProtoSink *o);

/**
 * Notifies the object that a packet was received from the peer.
 * Must not be in freeing state.
 * 
 * @param o the object
 * @param peer_receiving whether the DATAPROTO_FLAGS_RECEIVING_KEEPALIVES flag was set in the packet.
 *                       Must be 0 or 1.
 */
void DataProtoSink_Received (DataProtoSink *o, int peer_receiving);

/**
 * Initiazes the object.
 * 
 * @param o the object
 * @param input device input. Its input MTU must be <= INT_MAX - DATAPROTO_MAX_OVERHEAD.
 * @param handler handler called when a packet arrives to allow the user to route it to
 *                appropriate {@link DataProtoFlow} objects.
 * @param user value passed to handler
 * @param reactor reactor we live in
 * @return 1 on success, 0 on failure
 */
int DataProtoSource_Init (DataProtoSource *o, PacketRecvInterface *input, DataProtoSource_handler handler, void *user, BReactor *reactor) WARN_UNUSED;

/**
 * Frees the object.
 * There must be no {@link DataProtoFlow} objects referring to this device.
 * 
 * @param o the object
 */
void DataProtoSource_Free (DataProtoSource *o);

/**
 * Initializes the object.
 * The object is initialized in not attached state.
 * 
 * @param o the object
 * @param device device to receive frames from
 * @param source_id source peer ID to encode in the headers (i.e. our ID)
 * @param dest_id destination peer ID to encode in the headers (i.e. ID if the peer this
 *                object belongs to)
 * @param num_packets number of packets the buffer should hold. Must be >0.
 * @param inactivity_time milliseconds of output inactivity after which to call the
 *                        inactivity handler; <0 to disable. Note that the object is considered
 *                        active as long as its buffer is non-empty, even if is not attached to
 *                        a {@link DataProtoSink}.
 * @param handler_inactivity inactivity handler, if inactivity_time >=0
 * @param user value to pass to handler
 * @return 1 on success, 0 on failure
 */
int DataProtoFlow_Init (
    DataProtoFlow *o, DataProtoSource *device, peerid_t source_id, peerid_t dest_id, int num_packets,
    int inactivity_time, DataProtoFlow_handler_inactivity handler_inactivity, void *user
) WARN_UNUSED;

/**
 * Frees the object.
 * The object must be in not attached state.
 * 
 * @param o the object
 */
void DataProtoFlow_Free (DataProtoFlow *o);

/**
 * Routes a frame from the device to this object.
 * Must be called from within the job context of the {@link DataProtoSource_handler} handler.
 * Must not be called after this has been called with more=0 for the current frame.
 * 
 * @param o the object
 * @param more whether the current frame may have to be routed to more
 *             objects. If 0, must not be called again until the handler is
 *             called for the next frame. Must be 0 or 1.
 */
void DataProtoFlow_Route (DataProtoFlow *o, int more);

/**
 * Attaches the object to a destination.
 * The object must be in not attached state.
 * 
 * @param o the object
 * @param dp destination to attach to. This object's frame_mtu must be <=
 *           (output MTU of dp) - DATAPROTO_MAX_OVERHEAD.
 */
void DataProtoFlow_Attach (DataProtoFlow *o, DataProtoSink *dp);

/**
 * Detaches the object from a destination.
 * The object must be in attached state.
 * 
 * @param o the object
 */
void DataProtoFlow_Detach (DataProtoFlow *o);

#endif
