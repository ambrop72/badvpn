/**
 * @file DPRelay.c
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

#include <misc/offset.h>
#include <system/BLog.h>

#include <client/DPRelay.h>

#include <generated/blog_channel_DPRelay.h>

static void flow_inactivity_handler (struct DPRelay_flow *flow);

static struct DPRelay_flow * create_flow (DPRelaySource *src, DPRelaySink *sink, int num_packets, int inactivity_time)
{
    ASSERT(num_packets > 0)
    
    // allocate structure
    struct DPRelay_flow *flow = malloc(sizeof(*flow));
    if (!flow) {
        BLog(BLOG_ERROR, "relay flow %d->%d: malloc failed", (int)src->source_id, (int)sink->dest_id);
        goto fail0;
    }
    
    // set src and sink
    flow->src = src;
    flow->sink = sink;
    
    // init DataProtoFlow
    if (!DataProtoFlow_Init(&flow->dpls, &src->router->device, src->source_id, sink->dest_id, num_packets, inactivity_time, (DataProtoFlow_handler_inactivity)flow_inactivity_handler, flow)) {
        BLog(BLOG_ERROR, "relay flow %d->%d: DataProtoFlow_Init failed", (int)src->source_id, (int)sink->dest_id);
        goto fail1;
    }
    
    // insert to source list
    LinkedList1_Append(&src->flows_list, &flow->src_list_node);
    
    // insert to sink list
    LinkedList1_Append(&sink->flows_list, &flow->sink_list_node);
    
    // attach flow if needed
    if (sink->dest) {
        DataProtoFlow_Attach(&flow->dpls, sink->dest);
    }
    
    BLog(BLOG_INFO, "relay flow %d->%d: created", (int)src->source_id, (int)sink->dest_id);
    
    return flow;
    
fail1:
    free(flow);
fail0:
    return NULL;
}

static void free_flow (struct DPRelay_flow *flow)
{
    // detach flow if needed
    if (flow->sink->dest) {
        DataProtoFlow_Detach(&flow->dpls);
    }
    
    // remove posible router reference
    if (flow->src->router->current_flow == flow) {
        flow->src->router->current_flow = NULL;
    }
    
    // remove from sink list
    LinkedList1_Remove(&flow->sink->flows_list, &flow->sink_list_node);
    
    // remove from source list
    LinkedList1_Remove(&flow->src->flows_list, &flow->src_list_node);
    
    // free DataProtoFlow
    DataProtoFlow_Free(&flow->dpls);
    
    // free structore
    free(flow);
}

static void flow_inactivity_handler (struct DPRelay_flow *flow)
{
    BLog(BLOG_INFO, "relay flow %d->%d: timed out", (int)flow->src->source_id, (int)flow->sink->dest_id);
    
    free_flow(flow);
}

static struct DPRelay_flow * source_find_flow (DPRelaySource *o, DPRelaySink *sink)
{
    LinkedList1Node *node = LinkedList1_GetFirst(&o->flows_list);
    while (node) {
        struct DPRelay_flow *flow = UPPER_OBJECT(node, struct DPRelay_flow, src_list_node);
        ASSERT(flow->src == o)
        if (flow->sink == sink) {
            return flow;
        }
        node = LinkedList1Node_Next(node);
    }
    
    return NULL;
}

static void source_device_handler (DPRelayRouter *o, const uint8_t *frame, int frame_len)
{
    DebugObject_Access(&o->d_obj);
    
    if (!o->current_flow) {
        return;
    }
    
    // route frame to current flow
    DataProtoFlow_Route(&o->current_flow->dpls, 0);
    
    // set no current flow
    o->current_flow = NULL;
}

int DPRelayRouter_Init (DPRelayRouter *o, int frame_mtu, BReactor *reactor)
{
    ASSERT(frame_mtu >= 0)
    ASSERT(frame_mtu <= INT_MAX - DATAPROTO_MAX_OVERHEAD)
    
    // init arguments
    o->frame_mtu = frame_mtu;
    
    // init BufferWriter
    BufferWriter_Init(&o->writer, frame_mtu, BReactor_PendingGroup(reactor));
    
    // init DataProtoSource
    if (!DataProtoSource_Init(&o->device, BufferWriter_GetOutput(&o->writer), (DataProtoSource_handler)source_device_handler, o, reactor)) {
        goto fail1;
    }
    
    // have no current flow
    o->current_flow = NULL;
    
    DebugObject_Init(&o->d_obj);
    DebugCounter_Init(&o->d_ctr);
    
    return 1;
    
fail1:
    BufferWriter_Free(&o->writer);
    return 0;
}

void DPRelayRouter_Free (DPRelayRouter *o)
{
    ASSERT(!o->current_flow) // have no sources
    DebugCounter_Free(&o->d_ctr);
    DebugObject_Free(&o->d_obj);
    
    // free DataProtoSource
    DataProtoSource_Free(&o->device);
    
    // free BufferWriter
    BufferWriter_Free(&o->writer);
}

void DPRelayRouter_SubmitFrame (DPRelayRouter *o, DPRelaySource *src, DPRelaySink *sink, uint8_t *data, int data_len, int num_packets, int inactivity_time)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= o->frame_mtu)
    ASSERT(num_packets > 0)
    ASSERT(!o->current_flow)
    ASSERT(src->router == o)
    DebugObject_Access(&o->d_obj);
    DebugObject_Access(&src->d_obj);
    DebugObject_Access(&sink->d_obj);
    
    // get memory location
    uint8_t *out;
    if (!BufferWriter_StartPacket(&o->writer, &out)) {
        BLog(BLOG_ERROR, "BufferWriter_StartPacket failed for frame %d->%d !?", (int)src->source_id, (int)sink->dest_id);
        return;
    }
    
    // write frame
    memcpy(out, data, data_len);
    
    // submit frame
    BufferWriter_EndPacket(&o->writer, data_len);
    
    // get a flow
    // this comes _after_ writing the packet, in case flow initialization schedules jobs
    struct DPRelay_flow *flow = source_find_flow(src, sink);
    if (!flow) {
        if (!(flow = create_flow(src, sink, num_packets, inactivity_time))) {
            return;
        }
    }
    
    // remember flow so we know where to route the frame in source_device_handler
    o->current_flow = flow;
}

void DPRelaySource_Init (DPRelaySource *o, DPRelayRouter *router, peerid_t source_id, BReactor *reactor)
{
    DebugObject_Access(&router->d_obj);
    
    // init arguments
    o->router = router;
    o->source_id = source_id;
    
    // init flows list
    LinkedList1_Init(&o->flows_list);
    
    DebugObject_Init(&o->d_obj);
    DebugCounter_Increment(&o->router->d_ctr);
}

void DPRelaySource_Free (DPRelaySource *o)
{
    DebugCounter_Decrement(&o->router->d_ctr);
    DebugObject_Free(&o->d_obj);
    
    // free flows, detaching them if needed
    LinkedList1Node *node;
    while (node = LinkedList1_GetFirst(&o->flows_list)) {
        struct DPRelay_flow *flow = UPPER_OBJECT(node, struct DPRelay_flow, src_list_node);
        free_flow(flow);
    }
}

void DPRelaySink_Init (DPRelaySink *o, peerid_t dest_id)
{
    // init arguments
    o->dest_id = dest_id;
    
    // have no dest
    o->dest = NULL;
    
    // init flows list
    LinkedList1_Init(&o->flows_list);
    
    DebugObject_Init(&o->d_obj);
}

void DPRelaySink_Free (DPRelaySink *o)
{
    ASSERT(!o->dest)
    DebugObject_Free(&o->d_obj);
    
    // free flows
    LinkedList1Node *node;
    while (node = LinkedList1_GetFirst(&o->flows_list)) {
        struct DPRelay_flow *flow = UPPER_OBJECT(node, struct DPRelay_flow, sink_list_node);
        free_flow(flow);
    }
}

void DPRelaySink_Attach (DPRelaySink *o, DataProtoSink *dest)
{
    ASSERT(!o->dest)
    DebugObject_Access(&o->d_obj);
    
    // set dest
    o->dest = dest;
    
    // attach flows
    LinkedList1Node *node = LinkedList1_GetFirst(&o->flows_list);
    while (node) {
        struct DPRelay_flow *flow = UPPER_OBJECT(node, struct DPRelay_flow, sink_list_node);
        DataProtoFlow_Attach(&flow->dpls, o->dest);
        node = LinkedList1Node_Next(node);
    }
}

void DPRelaySink_Detach (DPRelaySink *o)
{
    ASSERT(o->dest)
    DebugObject_Access(&o->d_obj);
    
    // set no dest
    o->dest = NULL;
    
    // detach flows
    LinkedList1Node *node = LinkedList1_GetFirst(&o->flows_list);
    while (node) {
        struct DPRelay_flow *flow = UPPER_OBJECT(node, struct DPRelay_flow, sink_list_node);
        DataProtoFlow_Detach(&flow->dpls);
        node = LinkedList1Node_Next(node);
    }
}
