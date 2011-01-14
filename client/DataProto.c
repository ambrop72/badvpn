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
#include <misc/offset.h>
#include <misc/byteorder.h>
#include <misc/debug.h>
#include <system/BLog.h>

#include <client/DataProto.h>

#include <generated/blog_channel_DataProto.h>

#define DATAPROTO_TIMEOUT 30000

static int peerid_comparator (void *user, peerid_t *val1, peerid_t *val2);
static struct dp_relay_flow * create_relay_flow (DataProtoRelaySource *rs, DataProtoDest *dp, int num_packets, uint8_t *first_frame, int first_frame_len);
static void dealloc_relay_flow (struct dp_relay_flow *flow);
static void release_relay_flow (struct dp_relay_flow *flow);
static void flow_monitor_handler (struct dp_relay_flow *flow);
static void monitor_handler (DataProtoDest *o);
static void send_keepalive (DataProtoDest *o);
static void receive_timer_handler (DataProtoDest *o);
static void notifier_handler (DataProtoDest *o, uint8_t *data, int data_len);
static int pointer_comparator (void *user, void **val1, void **val2);
static void keepalive_job_handler (DataProtoDest *o);
static void relay_job_handler (struct dp_relay_flow *flow);
static void submit_relay_frame (struct dp_relay_flow *flow, uint8_t *frame, int frame_len);

int peerid_comparator (void *user, peerid_t *val1, peerid_t *val2)
{
    if (*val1 < *val2) {
        return -1;
    }
    if (*val1 > *val2) {
        return 1;
    }
    return 0;
}

struct dp_relay_flow * create_relay_flow (DataProtoRelaySource *rs, DataProtoDest *dp, int num_packets, uint8_t *first_frame, int first_frame_len)
{
    ASSERT(first_frame_len >= 0)
    ASSERT(first_frame_len <= dp->frame_mtu)
    ASSERT(!BAVL_LookupExact(&rs->relay_flows_tree, &dp))
    ASSERT(num_packets > 0)
    ASSERT(!dp->freeing)
    
    // allocate flow structure
    struct dp_relay_flow *flow = malloc(sizeof(struct dp_relay_flow));
    if (!flow) {
        BLog(BLOG_ERROR, "failed to allocate flow structure for relay flow from peer %d to %d", (int)rs->source_id, (int)dp->dest_id);
        goto fail0;
    }
    
    // init arguments
    flow->rs = rs;
    flow->dp = dp;
    flow->first_frame = first_frame;
    flow->first_frame_len = first_frame_len;
    
    // init first frame job
    BPending_Init(&flow->first_frame_job, BReactor_PendingGroup(dp->reactor), (BPending_handler)relay_job_handler, flow);
    BPending_Set(&flow->first_frame_job);
    
    // init queue flow
    PacketPassFairQueueFlow_Init(&flow->qflow, &dp->queue);
    
    // init inacitvity monitor
    PacketPassInactivityMonitor_Init(&flow->monitor, PacketPassFairQueueFlow_GetInput(&flow->qflow), dp->reactor, DATAPROTO_TIMEOUT, (PacketPassInactivityMonitor_handler)flow_monitor_handler, flow);
    
    // init async input
    BufferWriter_Init(&flow->ainput, dp->mtu, BReactor_PendingGroup(dp->reactor));
    
    // init buffer
    if (!PacketBuffer_Init(&flow->buffer, BufferWriter_GetOutput(&flow->ainput), PacketPassInactivityMonitor_GetInput(&flow->monitor), num_packets, BReactor_PendingGroup(dp->reactor))) {
        BLog(BLOG_ERROR, "PacketBuffer_Init failed for relay flow from peer %d to %d", (int)rs->source_id, (int)dp->dest_id);
        goto fail1;
    }
    
    // insert to source list
    LinkedList2_Append(&rs->relay_flows_list, &flow->source_list_node);
    
    // insert to source tree
    ASSERT_EXECUTE(BAVL_Insert(&rs->relay_flows_tree, &flow->source_tree_node, NULL))
    
    // insert to dp list
    LinkedList2_Append(&dp->relay_flows_list, &flow->dp_list_node);
    
    BLog(BLOG_NOTICE, "created relay flow from peer %d to %d", (int)rs->source_id, (int)dp->dest_id);
    
    return flow;
    
fail1:
    BufferWriter_Free(&flow->ainput);
    PacketPassInactivityMonitor_Free(&flow->monitor);
    PacketPassFairQueueFlow_Free(&flow->qflow);
    BPending_Free(&flow->first_frame_job);
    free(flow);
fail0:
    return NULL;
}

void dealloc_relay_flow (struct dp_relay_flow *flow)
{
    PacketPassFairQueueFlow_AssertFree(&flow->qflow);
    
    // remove from dp list
    LinkedList2_Remove(&flow->dp->relay_flows_list, &flow->dp_list_node);
    
    // remove from source tree
    BAVL_Remove(&flow->rs->relay_flows_tree, &flow->source_tree_node);
    
    // remove from source list
    LinkedList2_Remove(&flow->rs->relay_flows_list, &flow->source_list_node);
    
    // free buffer
    PacketBuffer_Free(&flow->buffer);
    
    // free async input
    BufferWriter_Free(&flow->ainput);
    
    // free inacitvity monitor
    PacketPassInactivityMonitor_Free(&flow->monitor);
    
    // free queue flow
    PacketPassFairQueueFlow_Free(&flow->qflow);
    
    // free first frame job
    BPending_Free(&flow->first_frame_job);
    
    // free flow structure
    free(flow);
}

void release_relay_flow (struct dp_relay_flow *flow)
{
    ASSERT(!flow->dp->freeing)
    
    // release it if it's busy
    if (PacketPassFairQueueFlow_IsBusy(&flow->qflow)) {
        PacketPassFairQueueFlow_Release(&flow->qflow);
    }
    
    // remove flow
    dealloc_relay_flow(flow);
}

void flow_monitor_handler (struct dp_relay_flow *flow)
{
    ASSERT(!flow->dp->freeing)
    
    BLog(BLOG_NOTICE, "relay flow from peer %d to %d timed out", (int)flow->rs->source_id, (int)flow->dp->dest_id);
    
    release_relay_flow(flow);
}

void monitor_handler (DataProtoDest *o)
{
    ASSERT(!o->freeing)
    DebugObject_Access(&o->d_obj);
    
    send_keepalive(o);
}

void send_keepalive (DataProtoDest *o)
{
    ASSERT(!o->freeing)
    
    BLog(BLOG_DEBUG, "sending keepalive to peer %d", (int)o->dest_id);
    
    PacketRecvBlocker_AllowBlockedPacket(&o->ka_blocker);
}

void receive_timer_handler (DataProtoDest *o)
{
    DebugObject_Access(&o->d_obj);
    
    BLog(BLOG_DEBUG, "receive timer triggered for peer %d", (int)o->dest_id);
    
    int prev_up = o->up;
    
    // consider down
    o->up = 0;
    
    // call handler if up state changed
    if (o->handler && o->up != prev_up) {
        o->handler(o->user, o->up);
        return;
    }
}

void notifier_handler (DataProtoDest *o, uint8_t *data, int data_len)
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

int pointer_comparator (void *user, void **val1, void **val2)
{
    if (*val1 < *val2) {
        return -1;
    }
    if (*val1 > *val2) {
        return 1;
    }
    return 0;
}

void keepalive_job_handler (DataProtoDest *o)
{
    ASSERT(!o->freeing)
    DebugObject_Access(&o->d_obj);
    
    send_keepalive(o);
}

void relay_job_handler (struct dp_relay_flow *flow)
{
    ASSERT(flow->first_frame_len >= 0)
    
    int frame_len = flow->first_frame_len;
    
    // set no first frame
    flow->first_frame_len = -1;
    
    // submit first frame
    submit_relay_frame(flow, flow->first_frame, frame_len);
}

void submit_relay_frame (struct dp_relay_flow *flow, uint8_t *frame, int frame_len)
{
    ASSERT(flow->first_frame_len == -1)
    
    // get a buffer
    uint8_t *out;
    // safe because of PacketBufferAsyncInput
    if (!BufferWriter_StartPacket(&flow->ainput, &out)) {
        BLog(BLOG_NOTICE, "out of buffer for relayed frame from peer %d to %d", (int)flow->rs->source_id, (int)flow->dp->dest_id);
        return;
    }
    
    // write header
    struct dataproto_header *header = (struct dataproto_header *)out;
    // don't set flags, it will be set in notifier_handler
    header->from_id = htol16(flow->rs->source_id);
    header->num_peer_ids = htol16(1);
    struct dataproto_peer_id *id = (struct dataproto_peer_id *)(out + sizeof(struct dataproto_header));
    id->id = htol16(flow->dp->dest_id);
    
    // write data
    memcpy(out + sizeof(struct dataproto_header) + sizeof(struct dataproto_peer_id), frame, frame_len);
    
    // submit it
    BufferWriter_EndPacket(&flow->ainput, sizeof(struct dataproto_header) + sizeof(struct dataproto_peer_id) + frame_len);
}

static void device_router_handler (DataProtoDevice *o, uint8_t *buf, int recv_len)
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

int DataProtoDest_Init (DataProtoDest *o, BReactor *reactor, peerid_t dest_id, PacketPassInterface *output, btime_t keepalive_time, btime_t tolerance_time, DataProtoDest_handler handler, void *user)
{
    ASSERT(PacketPassInterface_HasCancel(output))
    ASSERT(PacketPassInterface_GetMTU(output) >= sizeof(struct dataproto_header) + sizeof(struct dataproto_peer_id))
    
    // init arguments
    o->reactor = reactor;
    o->dest_id = dest_id;
    o->handler = handler;
    o->user = user;
    
    // set MTU
    o->mtu = PacketPassInterface_GetMTU(output);
    
    // set frame MTU
    o->frame_mtu = o->mtu - (sizeof(struct dataproto_header) + sizeof(struct dataproto_peer_id));
    
    // schedule keep-alive (needs to be before the buffer)
    BPending_Init(&o->keepalive_job, BReactor_PendingGroup(o->reactor), (BPending_handler)keepalive_job_handler, o);
    BPending_Set(&o->keepalive_job);
    
    // init notifier
    PacketPassNotifier_Init(&o->notifier, output, BReactor_PendingGroup(o->reactor));
    PacketPassNotifier_SetHandler(&o->notifier, (PacketPassNotifier_handler_notify)notifier_handler, o);
    
    // init monitor
    PacketPassInactivityMonitor_Init(&o->monitor, PacketPassNotifier_GetInput(&o->notifier), o->reactor, keepalive_time, (PacketPassInactivityMonitor_handler)monitor_handler, o);
    
    // init queue
    PacketPassFairQueue_Init(&o->queue, PacketPassInactivityMonitor_GetInput(&o->monitor), BReactor_PendingGroup(o->reactor), 1);
    
    // init keepalive queue flow
    PacketPassFairQueueFlow_Init(&o->ka_qflow, &o->queue);
    
    // init keepalive source
    DataProtoKeepaliveSource_Init(&o->ka_source, BReactor_PendingGroup(o->reactor));
    
    // init keepalive blocker
    PacketRecvBlocker_Init(&o->ka_blocker, DataProtoKeepaliveSource_GetOutput(&o->ka_source), BReactor_PendingGroup(o->reactor));
    
    // init keepalive buffer
    if (!SinglePacketBuffer_Init(&o->ka_buffer, PacketRecvBlocker_GetOutput(&o->ka_blocker), PacketPassFairQueueFlow_GetInput(&o->ka_qflow), BReactor_PendingGroup(o->reactor))) {
        BLog(BLOG_ERROR, "SinglePacketBuffer_Init failed");
        goto fail0;
    }
    
    // init receive timer
    BTimer_Init(&o->receive_timer, tolerance_time, (BTimer_handler)receive_timer_handler, o);
    
    // set not up
    o->up = 0;
    
    // init relay flows list
    LinkedList2_Init(&o->relay_flows_list);
    
    // set not freeing
    o->freeing = 0;
    
    DebugCounter_Init(&o->flows_counter);
    DebugObject_Init(&o->d_obj);
    
    #ifndef NDEBUG
    o->d_output = output;
    #endif
    
    return 1;
    
fail0:
    PacketRecvBlocker_Free(&o->ka_blocker);
    DataProtoKeepaliveSource_Free(&o->ka_source);
    PacketPassFairQueueFlow_Free(&o->ka_qflow);
    PacketPassFairQueue_Free(&o->queue);
    PacketPassInactivityMonitor_Free(&o->monitor);
    PacketPassNotifier_Free(&o->notifier);
    BPending_Free(&o->keepalive_job);
    return 0;
}

void DataProtoDest_Free (DataProtoDest *o)
{
    DebugCounter_Free(&o->flows_counter);
    DebugObject_Free(&o->d_obj);
    
    // allow freeing queue flows
    PacketPassFairQueue_PrepareFree(&o->queue);
    
    // free relay flows
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&o->relay_flows_list)) {
        struct dp_relay_flow *flow = UPPER_OBJECT(node, struct dp_relay_flow, dp_list_node);
        dealloc_relay_flow(flow);
    }
    
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

void DataProtoDest_PrepareFree (DataProtoDest *o)
{
    DebugObject_Access(&o->d_obj);
    
    // allow freeing queue flows
    PacketPassFairQueue_PrepareFree(&o->queue);
    
    // set freeing
    o->freeing = 1;
}

void DataProtoDest_SubmitRelayFrame (DataProtoDest *o, DataProtoRelaySource *rs, uint8_t *data, int data_len, int buffer_num_packets)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= o->frame_mtu)
    ASSERT(buffer_num_packets > 0)
    ASSERT(!o->freeing)
    DebugObject_Access(&rs->d_obj);
    DebugObject_Access(&o->d_obj);
    
    // lookup relay flow from source to this DataProto
    struct dp_relay_flow *flow;
    BAVLNode *node = BAVL_LookupExact(&rs->relay_flows_tree, &o);
    if (!node) {
        // create new flow
        create_relay_flow(rs, o, buffer_num_packets, data, data_len);
        return;
    }
    
    flow = UPPER_OBJECT(node, struct dp_relay_flow, source_tree_node);
    
    // submit frame
    submit_relay_frame(flow, data, data_len);
}

void DataProtoDest_Received (DataProtoDest *o, int peer_receiving)
{
    ASSERT(peer_receiving == 0 || peer_receiving == 1)
    ASSERT(!o->freeing)
    DebugObject_Access(&o->d_obj);
    
    int prev_up = o->up;
    
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
    
    // call handler if up state changed
    if (o->handler && o->up != prev_up) {
        o->handler(o->user, o->up);
        return;
    }
}

int DataProtoDevice_Init (DataProtoDevice *o, PacketRecvInterface *input, DataProtoDevice_handler handler, void *user, BReactor *reactor)
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

void DataProtoDevice_Free (DataProtoDevice *o)
{
    DebugCounter_Free(&o->d_ctr);
    DebugObject_Free(&o->d_obj);
    
    // free router
    PacketRouter_Free(&o->router);
}

int DataProtoLocalSource_Init (DataProtoLocalSource *o, DataProtoDevice *device, peerid_t source_id, peerid_t dest_id, int num_packets)
{
    ASSERT(num_packets > 0)
    
    // init arguments
    o->device = device;
    o->source_id = source_id;
    o->dest_id = dest_id;
    
    // init connector
    PacketPassConnector_Init(&o->connector, DATAPROTO_MAX_OVERHEAD + device->frame_mtu, BReactor_PendingGroup(device->reactor));
    
    // init route buffer
    if (!RouteBuffer_Init(&o->rbuf, DATAPROTO_MAX_OVERHEAD + device->frame_mtu, PacketPassConnector_GetInput(&o->connector), num_packets)) {
        BLog(BLOG_ERROR, "RouteBuffer_Init failed");
        goto fail1;
    }
    
    // set no DataProto
    o->dp = NULL;
    
    DebugObject_Init(&o->d_obj);
    DebugCounter_Increment(&device->d_ctr);
    
    return 1;
    
fail1:
    PacketPassConnector_Free(&o->connector);
fail0:
    return 0;
}

void DataProtoLocalSource_Free (DataProtoLocalSource *o)
{
    ASSERT(!o->dp)
    DebugCounter_Decrement(&o->device->d_ctr);
    DebugObject_Free(&o->d_obj);
    
    // free route buffer
    RouteBuffer_Free(&o->rbuf);
    
    // free connector
    PacketPassConnector_Free(&o->connector);
}

void DataProtoLocalSource_Route (DataProtoLocalSource *o, int more)
{
    ASSERT(more == 0 || more == 1)
    PacketRouter_AssertRoute(&o->device->router);
    ASSERT(o->device->current_buf)
    if (o->dp) {
        ASSERT(!o->dp->freeing)
    }
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
        BLog(BLOG_NOTICE, "out of buffer for frame from peer %d to %d", (int)o->source_id, (int)o->dest_id);
        return;
    }
    
    o->device->current_buf = (more ? next_buf : NULL);
}

void DataProtoLocalSource_Attach (DataProtoLocalSource *o, DataProtoDest *dp)
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
    
    // increment flows counter
    DebugCounter_Increment(&dp->flows_counter);
}

void DataProtoLocalSource_Detach (DataProtoLocalSource *o)
{
    #ifndef NDEBUG
    ASSERT(o->dp)
    #endif
    DebugObject_Access(&o->d_obj);
    
    DataProtoDest *dp = o->dp;
    
    // release flow if needed
    if (!o->dp->freeing && PacketPassFairQueueFlow_IsBusy(&o->dp_qflow)) {
        PacketPassFairQueueFlow_Release(&o->dp_qflow);
    }
    
    // decrement flows counter
    DebugCounter_Decrement(&dp->flows_counter);
    
    // disconnect from queue flow
    PacketPassConnector_DisconnectOutput(&o->connector);
    
    // free queue flow
    PacketPassFairQueueFlow_Free(&o->dp_qflow);
    
    // set no DataProto
    o->dp = NULL;
}

void DataProtoRelaySource_Init (DataProtoRelaySource *o, peerid_t source_id)
{
    // init arguments
    o->source_id = source_id;
    
    // init relay flows list
    LinkedList2_Init(&o->relay_flows_list);
    
    // init relay flows tree
    BAVL_Init(&o->relay_flows_tree, OFFSET_DIFF(struct dp_relay_flow, dp, source_tree_node), (BAVL_comparator)pointer_comparator, NULL);
    
    DebugObject_Init(&o->d_obj);
}

void DataProtoRelaySource_Free (DataProtoRelaySource *o)
{
    ASSERT(BAVL_IsEmpty(&o->relay_flows_tree))
    ASSERT(LinkedList2_IsEmpty(&o->relay_flows_list))
    DebugObject_Free(&o->d_obj);
}

int DataProtoRelaySource_IsEmpty (DataProtoRelaySource *o)
{
    DebugObject_Access(&o->d_obj);
    
    return LinkedList2_IsEmpty(&o->relay_flows_list);
}

void DataProtoRelaySource_Release (DataProtoRelaySource *o)
{
    DebugObject_Access(&o->d_obj);
    
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&o->relay_flows_list)) {
        struct dp_relay_flow *flow = UPPER_OBJECT(node, struct dp_relay_flow, source_list_node);
        
        release_relay_flow(flow);
    }
}

void DataProtoRelaySource_FreeRelease (DataProtoRelaySource *o)
{
    DebugObject_Access(&o->d_obj);
    
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&o->relay_flows_list)) {
        struct dp_relay_flow *flow = UPPER_OBJECT(node, struct dp_relay_flow, source_list_node);
        
        DataProtoDest_PrepareFree(flow->dp);
        dealloc_relay_flow(flow);
    }
}
