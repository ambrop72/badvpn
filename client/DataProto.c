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
static void up_job_handler (DataProtoSink *o);
static void flow_buffer_free (struct DataProtoFlow_buffer *b);
static void flow_buffer_attach (struct DataProtoFlow_buffer *b, DataProtoSink *dp);
static void flow_buffer_detach (struct DataProtoFlow_buffer *b);
static void flow_buffer_schedule_detach (struct DataProtoFlow_buffer *b);
static void flow_buffer_finish_detach (struct DataProtoFlow_buffer *b);
static void flow_buffer_qflow_handler_busy (struct DataProtoFlow_buffer *b);

void monitor_handler (DataProtoSink *o)
{
    DebugObject_Access(&o->d_obj);
    
    send_keepalive(o);
}

void send_keepalive (DataProtoSink *o)
{
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
    
    int flags = 0;
    
    // if we are receiving keepalives, set the flag
    if (BTimer_IsRunning(&o->receive_timer)) {
        flags |= DATAPROTO_FLAGS_RECEIVING_KEEPALIVES;
    }
    
    // modify existing packet here
    struct dataproto_header *header = (struct dataproto_header *)data;
    header->flags = htol8(flags);
}

void up_job_handler (DataProtoSink *o)
{
    ASSERT(o->up != o->up_report)
    DebugObject_Access(&o->d_obj);
    
    o->up_report = o->up;
    
    o->handler(o->user, o->up);
    return;
}

void device_router_handler (DataProtoSource *o, uint8_t *buf, int recv_len)
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

void flow_buffer_free (struct DataProtoFlow_buffer *b)
{
    ASSERT(!b->dp)
    
    // free route buffer
    RouteBuffer_Free(&b->rbuf);
    
    // free inactivity monitor
    if (b->inactivity_time >= 0) {
        PacketPassInactivityMonitor_Free(&b->monitor);
    }
    
    // free connector
    PacketPassConnector_Free(&b->connector);
    
    // free buffer structure
    free(b);
}

void flow_buffer_attach (struct DataProtoFlow_buffer *b, DataProtoSink *dp)
{
    ASSERT(!b->dp)
    
    // init queue flow
    PacketPassFairQueueFlow_Init(&b->dp_qflow, &dp->queue);
    
    // connect to queue flow
    PacketPassConnector_ConnectOutput(&b->connector, PacketPassFairQueueFlow_GetInput(&b->dp_qflow));
    
    // set DataProto
    b->dp = dp;
}

void flow_buffer_detach (struct DataProtoFlow_buffer *b)
{
    ASSERT(b->dp)
    PacketPassFairQueueFlow_AssertFree(&b->dp_qflow);
    
    // disconnect from queue flow
    PacketPassConnector_DisconnectOutput(&b->connector);
    
    // free queue flow
    PacketPassFairQueueFlow_Free(&b->dp_qflow);
    
    // clear reference to this buffer in the sink
    if (b->dp->detaching_buffer == b) {
        b->dp->detaching_buffer = NULL;
    }
    
    // set no DataProto
    b->dp = NULL;
}

void flow_buffer_schedule_detach (struct DataProtoFlow_buffer *b)
{
    ASSERT(b->dp)
    ASSERT(PacketPassFairQueueFlow_IsBusy(&b->dp_qflow))
    ASSERT(!b->dp->detaching_buffer || b->dp->detaching_buffer == b)
    
    if (b->dp->detaching_buffer == b) {
        return;
    }
    
    // request cancel
    PacketPassFairQueueFlow_RequestCancel(&b->dp_qflow);
    
    // set busy handler
    PacketPassFairQueueFlow_SetBusyHandler(&b->dp_qflow, (PacketPassFairQueue_handler_busy)flow_buffer_qflow_handler_busy, b);
    
    // remember this buffer in the sink so it can handle us if it goes away
    b->dp->detaching_buffer = b;
}

void flow_buffer_finish_detach (struct DataProtoFlow_buffer *b)
{
    ASSERT(b->dp)
    ASSERT(b->dp->detaching_buffer == b)
    PacketPassFairQueueFlow_AssertFree(&b->dp_qflow);
    
    // detach
    flow_buffer_detach(b);
    
    if (!b->flow) {
        // free
        flow_buffer_free(b);
    } else if (b->flow->dp_desired) {
        // attach
        flow_buffer_attach(b, b->flow->dp_desired);
    }
}

void flow_buffer_qflow_handler_busy (struct DataProtoFlow_buffer *b)
{
    ASSERT(b->dp)
    ASSERT(b->dp->detaching_buffer == b)
    PacketPassFairQueueFlow_AssertFree(&b->dp_qflow);
    
    flow_buffer_finish_detach(b);
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
    
    // init notifier
    PacketPassNotifier_Init(&o->notifier, output, BReactor_PendingGroup(o->reactor));
    PacketPassNotifier_SetHandler(&o->notifier, (PacketPassNotifier_handler_notify)notifier_handler, o);
    
    // init monitor
    PacketPassInactivityMonitor_Init(&o->monitor, PacketPassNotifier_GetInput(&o->notifier), o->reactor, keepalive_time, (PacketPassInactivityMonitor_handler)monitor_handler, o);
    PacketPassInactivityMonitor_Force(&o->monitor);
    
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
    
    // set no detaching buffer
    o->detaching_buffer = NULL;
    
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
    return 0;
}

void DataProtoSink_Free (DataProtoSink *o)
{
    DebugCounter_Free(&o->d_ctr);
    DebugObject_Free(&o->d_obj);
    
    // allow freeing queue flows
    PacketPassFairQueue_PrepareFree(&o->queue);
    
    // release detaching buffer
    if (o->detaching_buffer) {
        ASSERT(!o->detaching_buffer->flow || o->detaching_buffer->flow->dp_desired != o)
        flow_buffer_finish_detach(o->detaching_buffer);
    }
    
    // free handler job
    BPending_Free(&o->up_job);
    
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
}

void DataProtoSink_Received (DataProtoSink *o, int peer_receiving)
{
    ASSERT(peer_receiving == 0 || peer_receiving == 1)
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
    
    // set no desired sink
    o->dp_desired = NULL;
    
    // allocate buffer structure
    struct DataProtoFlow_buffer *b = malloc(sizeof(*b));
    if (!b) {
        BLog(BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    o->b = b;
    
    // set parent
    b->flow = o;
    
    // remember inactivity time
    b->inactivity_time = inactivity_time;
    
    // init connector
    PacketPassConnector_Init(&b->connector, DATAPROTO_MAX_OVERHEAD + device->frame_mtu, BReactor_PendingGroup(device->reactor));
    
    // init inactivity monitor
    PacketPassInterface *buf_out = PacketPassConnector_GetInput(&b->connector);
    if (b->inactivity_time >= 0) {
        PacketPassInactivityMonitor_Init(&b->monitor, buf_out, device->reactor, b->inactivity_time, handler_inactivity, user);
        buf_out = PacketPassInactivityMonitor_GetInput(&b->monitor);
    }
    
    // init route buffer
    if (!RouteBuffer_Init(&b->rbuf, DATAPROTO_MAX_OVERHEAD + device->frame_mtu, buf_out, num_packets)) {
        BLog(BLOG_ERROR, "RouteBuffer_Init failed");
        goto fail1;
    }
    
    // set no DataProto
    b->dp = NULL;
    
    DebugObject_Init(&o->d_obj);
    DebugCounter_Increment(&device->d_ctr);
    
    return 1;
    
fail1:
    if (b->inactivity_time >= 0) {
        PacketPassInactivityMonitor_Free(&b->monitor);
    }
    PacketPassConnector_Free(&b->connector);
    free(b);
fail0:
    return 0;
}

void DataProtoFlow_Free (DataProtoFlow *o)
{
    struct DataProtoFlow_buffer *b = o->b;
    ASSERT(!o->dp_desired)
    DebugCounter_Decrement(&o->device->d_ctr);
    DebugObject_Free(&o->d_obj);
    
    if (b->dp) {
        if (PacketPassFairQueueFlow_IsBusy(&b->dp_qflow)) {
            // schedule detach, free buffer after detach
            flow_buffer_schedule_detach(b);
            b->flow = NULL;
            
            // remove inactivity handler
            if (b->inactivity_time >= 0) {
                PacketPassInactivityMonitor_SetHandler(&b->monitor, NULL, NULL);
            }
        } else {
            // detach and free buffer now
            flow_buffer_detach(b);
            flow_buffer_free(b);
        }
    } else {
        // free buffer
        flow_buffer_free(b);
    }
}

void DataProtoFlow_Route (DataProtoFlow *o, int more)
{
    struct DataProtoFlow_buffer *b = o->b;
    ASSERT(more == 0 || more == 1)
    PacketRouter_AssertRoute(&o->device->router);
    ASSERT(o->device->current_buf)
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
        &o->device->router, DATAPROTO_MAX_OVERHEAD + o->device->current_recv_len, &b->rbuf,
        &next_buf, DATAPROTO_MAX_OVERHEAD, (more ? o->device->current_recv_len : 0)
    )) {
        BLog(BLOG_NOTICE, "buffer full: %d->%d", (int)o->source_id, (int)o->dest_id);
        return;
    }
    
    o->device->current_buf = (more ? next_buf : NULL);
}

void DataProtoFlow_Attach (DataProtoFlow *o, DataProtoSink *dp)
{
    struct DataProtoFlow_buffer *b = o->b;
    ASSERT(dp)
    ASSERT(!o->dp_desired)
    ASSERT(o->device->frame_mtu <= dp->frame_mtu)
    DebugObject_Access(&o->d_obj);
    DebugObject_Access(&dp->d_obj);
    
    if (b->dp) {
        if (PacketPassFairQueueFlow_IsBusy(&b->dp_qflow)) {
            // schedule detach and reattach
            flow_buffer_schedule_detach(b);
        } else {
            // detach and reattach now
            flow_buffer_detach(b);
            flow_buffer_attach(b, dp);
        }
    } else {
        // attach
        flow_buffer_attach(b, dp);
    }
    
    // set desired sink
    o->dp_desired = dp;
    
    DebugCounter_Increment(&dp->d_ctr);
}

void DataProtoFlow_Detach (DataProtoFlow *o)
{
    struct DataProtoFlow_buffer *b = o->b;
    ASSERT(o->dp_desired)
    ASSERT(b->dp)
    DebugObject_Access(&o->d_obj);
    
    DataProtoSink *dp = o->dp_desired;
    
    if (PacketPassFairQueueFlow_IsBusy(&b->dp_qflow)) {
        // schedule detach
        flow_buffer_schedule_detach(b);
    } else {
        // detach now
        flow_buffer_detach(b);
    }
    
    // set no desired sink
    o->dp_desired = NULL;
    
    DebugCounter_Decrement(&dp->d_ctr);
}
