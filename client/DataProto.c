/**
 * @file DataProto.c
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
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <protocol/dataproto.h>
#include <misc/byteorder.h>
#include <misc/debug.h>
#include <system/BLog.h>

#include <client/DataProto.h>

#include <generated/blog_channel_DataProto.h>

static void monitor_handler (DataProtoSink *o);
static void send_keepalive (DataProtoSink *o);
static void refresh_up_job (DataProtoSink *o);
static void receive_timer_handler (DataProtoSink *o);
static void notifier_handler (DataProtoSink *o, uint8_t *data, int data_len);
static void keepalive_job_handler (DataProtoSink *o);
static void up_job_handler (DataProtoSink *o);

void monitor_handler (DataProtoSink *o)
{
    ASSERT(!o->freeing)
    DebugObject_Access(&o->d_obj);
    
    send_keepalive(o);
}

void send_keepalive (DataProtoSink *o)
{
    ASSERT(!o->freeing)
    
    PacketRecvBlocker_AllowBlockedPacket(&o->ka_blocker);
}

void refresh_up_job (DataProtoSink *o)
{
    if (o->up != o->up_report) {
        BPending_Set(&o->up_job);
    } else {
        BPending_Unset(&o->up_job);
    }
}

void receive_timer_handler (DataProtoSink *o)
{
    DebugObject_Access(&o->d_obj);
    
    // consider down
    o->up = 0;
    
    refresh_up_job(o);
}

void notifier_handler (DataProtoSink *o, uint8_t *data, int data_len)
{
    ASSERT(data_len >= sizeof(struct dataproto_header))
    DebugObject_Access(&o->d_obj);
    
    // modify existing packet here
    struct dataproto_header *header = (struct dataproto_header *)data;
    header->flags = 0;
    
    // if we are receiving keepalives, set the flag
    if (BTimer_IsRunning(&o->receive_timer)) {
        header->flags |= DATAPROTO_FLAGS_RECEIVING_KEEPALIVES;
    }
}

void keepalive_job_handler (DataProtoSink *o)
{
    ASSERT(!o->freeing)
    DebugObject_Access(&o->d_obj);
    
    send_keepalive(o);
}

void up_job_handler (DataProtoSink *o)
{
    ASSERT(o->up != o->up_report)
    ASSERT(!o->freeing)
    DebugObject_Access(&o->d_obj);
    
    o->up_report = o->up;
    
    o->handler(o->user, o->up);
    return;
}

static void device_router_handler (DataProtoSource *o, uint8_t *buf, int recv_len)
{
    ASSERT(buf)
    ASSERT(recv_len >= 0)
    ASSERT(recv_len <= o->frame_mtu)
    DebugObject_Access(&o->d_obj);
    
    // remember packet
    o->current_buf = buf;
    o->current_recv_len = recv_len;
    
    // call handler
    o->handler(o->user, buf + DATAPROTO_MAX_OVERHEAD, recv_len);
    return;
}

int DataProtoSink_Init (DataProtoSink *o, BReactor *reactor, PacketPassInterface *output, btime_t keepalive_time, btime_t tolerance_time, DataProtoSink_handler handler, void *user)
{
    ASSERT(PacketPassInterface_HasCancel(output))
    ASSERT(PacketPassInterface_GetMTU(output) >= DATAPROTO_MAX_OVERHEAD)
    
    // init arguments
    o->reactor = reactor;
    o->handler = handler;
    o->user = user;
    
    // set frame MTU
    o->frame_mtu = PacketPassInterface_GetMTU(output) - DATAPROTO_MAX_OVERHEAD;
    
    // schedule keep-alive (needs to be before the buffer)
    BPending_Init(&o->keepalive_job, BReactor_PendingGroup(o->reactor), (BPending_handler)keepalive_job_handler, o);
    BPending_Set(&o->keepalive_job);
    
    // init notifier
    PacketPassNotifier_Init(&o->notifier, output, BReactor_PendingGroup(o->reactor));
    PacketPassNotifier_SetHandler(&o->notifier, (PacketPassNotifier_handler_notify)notifier_handler, o);
    
    // init monitor
    PacketPassInactivityMonitor_Init(&o->monitor, PacketPassNotifier_GetInput(&o->notifier), o->reactor, keepalive_time, (PacketPassInactivityMonitor_handler)monitor_handler, o);
    
    // init queue
    PacketPassFairQueue_Init(&o->queue, PacketPassInactivityMonitor_GetInput(&o->monitor), BReactor_PendingGroup(o->reactor), 1, 1);
    
    // init keepalive queue flow
    PacketPassFairQueueFlow_Init(&o->ka_qflow, &o->queue);
    
    // init keepalive source
    DataProtoKeepaliveSource_Init(&o->ka_source, BReactor_PendingGroup(o->reactor));
    
    // init keepalive blocker
    PacketRecvBlocker_Init(&o->ka_blocker, DataProtoKeepaliveSource_GetOutput(&o->ka_source), BReactor_PendingGroup(o->reactor));
    
    // init keepalive buffer
    if (!SinglePacketBuffer_Init(&o->ka_buffer, PacketRecvBlocker_GetOutput(&o->ka_blocker), PacketPassFairQueueFlow_GetInput(&o->ka_qflow), BReactor_PendingGroup(o->reactor))) {
        BLog(BLOG_ERROR, "SinglePacketBuffer_Init failed");
        goto fail1;
    }
    
    // init receive timer
    BTimer_Init(&o->receive_timer, tolerance_time, (BTimer_handler)receive_timer_handler, o);
    
    // init handler job
    BPending_Init(&o->up_job, BReactor_PendingGroup(o->reactor), (BPending_handler)up_job_handler, o);
    
    // set not up
    o->up = 0;
    o->up_report = 0;
    
    // set not freeing
    o->freeing = 0;
    
    DebugCounter_Init(&o->d_ctr);
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    PacketRecvBlocker_Free(&o->ka_blocker);
    DataProtoKeepaliveSource_Free(&o->ka_source);
    PacketPassFairQueueFlow_Free(&o->ka_qflow);
    PacketPassFairQueue_Free(&o->queue);
    PacketPassInactivityMonitor_Free(&o->monitor);
    PacketPassNotifier_Free(&o->notifier);
    BPending_Free(&o->keepalive_job);
    return 0;
}

void DataProtoSink_Free (DataProtoSink *o)
{
    DebugCounter_Free(&o->d_ctr);
    DebugObject_Free(&o->d_obj);
    
    // free handler job
    BPending_Free(&o->up_job);
    
    // allow freeing queue flows
    PacketPassFairQueue_PrepareFree(&o->queue);
    
    // free receive timer
    BReactor_RemoveTimer(o->reactor, &o->receive_timer);
    
    // free keepalive buffer
    SinglePacketBuffer_Free(&o->ka_buffer);
    
    // free keepalive blocker
    PacketRecvBlocker_Free(&o->ka_blocker);
    
    // free keepalive source
    DataProtoKeepaliveSource_Free(&o->ka_source);
    
    // free keepalive queue flow
    PacketPassFairQueueFlow_Free(&o->ka_qflow);
    
    // free queue
    PacketPassFairQueue_Free(&o->queue);
    
    // free monitor
    PacketPassInactivityMonitor_Free(&o->monitor);
    
    // free notifier
    PacketPassNotifier_Free(&o->notifier);
    
    // free keepalive job
    BPending_Free(&o->keepalive_job);
}

void DataProtoSink_PrepareFree (DataProtoSink *o)
{
    DebugObject_Access(&o->d_obj);
    
    // allow freeing queue flows
    PacketPassFairQueue_PrepareFree(&o->queue);
    
    // set freeing
    o->freeing = 1;
}

void DataProtoSink_Received (DataProtoSink *o, int peer_receiving)
{
    ASSERT(peer_receiving == 0 || peer_receiving == 1)
    ASSERT(!o->freeing)
    DebugObject_Access(&o->d_obj);
    
    // reset receive timer
    BReactor_SetTimer(o->reactor, &o->receive_timer);
    
    if (!peer_receiving) {
        // peer reports not receiving, consider down
        o->up = 0;
        // send keep-alive to converge faster
        send_keepalive(o);
    } else {
        // consider up
        o->up = 1;
    }
    
    refresh_up_job(o);
}

int DataProtoSource_Init (DataProtoSource *o, PacketRecvInterface *input, DataProtoSource_handler handler, void *user, BReactor *reactor)
{
    ASSERT(PacketRecvInterface_GetMTU(input) <= INT_MAX - DATAPROTO_MAX_OVERHEAD)
    
    // init arguments
    o->handler = handler;
    o->user = user;
    o->reactor = reactor;
    
    // remember frame MTU
    o->frame_mtu = PacketRecvInterface_GetMTU(input);
    
    // init router
    if (!PacketRouter_Init(&o->router, DATAPROTO_MAX_OVERHEAD + o->frame_mtu, DATAPROTO_MAX_OVERHEAD, input, (PacketRouter_handler)device_router_handler, o, BReactor_PendingGroup(reactor))) {
        goto fail1;
    }
    
    DebugObject_Init(&o->d_obj);
    DebugCounter_Init(&o->d_ctr);
    
    return 1;
    
fail1:
    return 0;
}

void DataProtoSource_Free (DataProtoSource *o)
{
    DebugCounter_Free(&o->d_ctr);
    DebugObject_Free(&o->d_obj);
    
    // free router
    PacketRouter_Free(&o->router);
}

int DataProtoFlow_Init (
    DataProtoFlow *o, DataProtoSource *device, peerid_t source_id, peerid_t dest_id, int num_packets,
    int inactivity_time, DataProtoFlow_handler_inactivity handler_inactivity, void *user
)
{
    ASSERT(num_packets > 0)
    
    // init arguments
    o->device = device;
    o->source_id = source_id;
    o->dest_id = dest_id;
    o->inactivity_time = inactivity_time;
    
    // init connector
    PacketPassConnector_Init(&o->connector, DATAPROTO_MAX_OVERHEAD + device->frame_mtu, BReactor_PendingGroup(device->reactor));
    
    // init inactivity monitor
    PacketPassInterface *buf_out = PacketPassConnector_GetInput(&o->connector);
    if (o->inactivity_time >= 0) {
        PacketPassInactivityMonitor_Init(&o->monitor, buf_out, device->reactor, o->inactivity_time, handler_inactivity, user);
        buf_out = PacketPassInactivityMonitor_GetInput(&o->monitor);
    }
    
    // init route buffer
    if (!RouteBuffer_Init(&o->rbuf, DATAPROTO_MAX_OVERHEAD + device->frame_mtu, buf_out, num_packets)) {
        BLog(BLOG_ERROR, "RouteBuffer_Init failed");
        goto fail1;
    }
    
    // set no DataProto
    o->dp = NULL;
    
    DebugObject_Init(&o->d_obj);
    DebugCounter_Increment(&device->d_ctr);
    
    return 1;
    
fail1:
    if (o->inactivity_time >= 0) {
        PacketPassInactivityMonitor_Free(&o->monitor);
    }
    PacketPassConnector_Free(&o->connector);
fail0:
    return 0;
}

void DataProtoFlow_Free (DataProtoFlow *o)
{
    ASSERT(!o->dp)
    DebugCounter_Decrement(&o->device->d_ctr);
    DebugObject_Free(&o->d_obj);
    
    // free route buffer
    RouteBuffer_Free(&o->rbuf);
    
    // free inactivity monitor
    if (o->inactivity_time >= 0) {
        PacketPassInactivityMonitor_Free(&o->monitor);
    }
    
    // free connector
    PacketPassConnector_Free(&o->connector);
}

void DataProtoFlow_Route (DataProtoFlow *o, int more)
{
    ASSERT(more == 0 || more == 1)
    PacketRouter_AssertRoute(&o->device->router);
    ASSERT(o->device->current_buf)
    ASSERT(!o->dp || !o->dp->freeing)
    DebugObject_Access(&o->d_obj);
    
    // write header
    struct dataproto_header *header = (struct dataproto_header *)o->device->current_buf;
    // don't set flags, it will be set in notifier_handler
    header->from_id = htol16(o->source_id);
    header->num_peer_ids = htol16(1);
    struct dataproto_peer_id *id = (struct dataproto_peer_id *)(header + 1);
    id->id = htol16(o->dest_id);
    
    // route
    uint8_t *next_buf;
    if (!PacketRouter_Route(
        &o->device->router, DATAPROTO_MAX_OVERHEAD + o->device->current_recv_len, &o->rbuf,
        &next_buf, DATAPROTO_MAX_OVERHEAD, (more ? o->device->current_recv_len : 0)
    )) {
        BLog(BLOG_NOTICE, "buffer full: %d->%d", (int)o->source_id, (int)o->dest_id);
        return;
    }
    
    o->device->current_buf = (more ? next_buf : NULL);
}

void DataProtoFlow_Attach (DataProtoFlow *o, DataProtoSink *dp)
{
    ASSERT(dp)
    ASSERT(!o->dp)
    ASSERT(o->device->frame_mtu <= dp->frame_mtu)
    ASSERT(!dp->freeing)
    DebugObject_Access(&o->d_obj);
    DebugObject_Access(&dp->d_obj);
    
    // set DataProto
    o->dp = dp;
    
    // init queue flow
    PacketPassFairQueueFlow_Init(&o->dp_qflow, &dp->queue);
    
    // connect to queue flow
    PacketPassConnector_ConnectOutput(&o->connector, PacketPassFairQueueFlow_GetInput(&o->dp_qflow));
    
    DebugCounter_Increment(&dp->d_ctr);
}

void DataProtoFlow_Detach (DataProtoFlow *o)
{
    ASSERT(o->dp)
    DebugObject_Access(&o->d_obj);
    
    DataProtoSink *dp = o->dp;
    
    // release flow if needed
    if (!o->dp->freeing && PacketPassFairQueueFlow_IsBusy(&o->dp_qflow)) {
        PacketPassFairQueueFlow_Release(&o->dp_qflow);
    }
    
    // disconnect from queue flow
    PacketPassConnector_DisconnectOutput(&o->connector);
    
    // free queue flow
    PacketPassFairQueueFlow_Free(&o->dp_qflow);
    
    // set no DataProto
    o->dp = NULL;
    
    DebugCounter_Decrement(&dp->d_ctr);
}
