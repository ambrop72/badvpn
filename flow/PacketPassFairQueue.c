/**
 * @file PacketPassFairQueue.c
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

#include <misc/debug.h>
#include <misc/offset.h>

#include <flow/PacketPassFairQueue.h>

static int time_comparator (void *user, uint64_t *time1, uint64_t *time2)
{
    if (*time1 < *time2) {
        return -1;
    }
    if (*time1 > *time2) {
        return 1;
    }
    return 0;
}

static uint64_t get_current_time (PacketPassFairQueue *m)
{
    if (m->sending_flow) {
        return m->sending_flow->time;
    }
    
    uint64_t time;
    int have = 0;
    
    BHeapNode *heap_node = BHeap_GetFirst(&m->queued_heap);
    if (heap_node) {
        PacketPassFairQueueFlow *first_flow = UPPER_OBJECT(heap_node, PacketPassFairQueueFlow, queued.heap_node);
        ASSERT(first_flow->is_queued)
        
        time = first_flow->time;
        have = 1;
    }
    
    if (m->previous_flow) {
        if (!have || m->previous_flow->time < time) {
            time = m->previous_flow->time;
            have = 1;
        }
    }
    
    return (have ? time : 0);
}

static void increment_sent_flow (PacketPassFairQueueFlow *flow, int iamount)
{
    ASSERT(iamount >= 0)
    ASSERT(iamount <= UINT64_MAX)
    ASSERT(!flow->is_queued)
    ASSERT(!flow->m->sending_flow)
    
    PacketPassFairQueue *m = flow->m;
    uint64_t amount = iamount;
    
    // does time overflow?
    if (!(flow->time + amount < flow->time)) {
        flow->time += amount;
    } else {
        // get flow with lowest time
        BHeapNode *heap_node = BHeap_GetFirst(&m->queued_heap);
        if (!heap_node) {
            flow->time = amount;
        } else {
            PacketPassFairQueueFlow *first_flow = UPPER_OBJECT(heap_node, PacketPassFairQueueFlow, queued.heap_node);
            ASSERT(first_flow->is_queued)
            // subtract lowest time from all queued flows
            uint64_t subtract = first_flow->time;
            LinkedList2Iterator it;
            LinkedList2Iterator_InitForward(&it, &m->queued_list);
            LinkedList2Node *list_node;
            while (list_node = LinkedList2Iterator_Next(&it)) {
                PacketPassFairQueueFlow *queue_flow = UPPER_OBJECT(list_node, PacketPassFairQueueFlow, queued.list_node);
                ASSERT(queue_flow->is_queued)
                queue_flow->time -= subtract;
            }
            // update the given flow's time; note we subtract because it isn't in the queue
            // TODO: prove this is correct
            flow->time = flow->time - subtract + amount;
        }
    }
}

static void schedule (PacketPassFairQueue *m)
{
    ASSERT(!m->freeing)
    ASSERT(!m->sending_flow)
    ASSERT(!m->previous_flow)
    ASSERT(BHeap_GetFirst(&m->queued_heap))
    
    // get first queued flow
    BHeapNode *heap_node = BHeap_GetFirst(&m->queued_heap);
    PacketPassFairQueueFlow *qflow = UPPER_OBJECT(heap_node, PacketPassFairQueueFlow, queued.heap_node);
    ASSERT(qflow->is_queued)
    
    // remove flow from queue
    BHeap_Remove(&m->queued_heap, &qflow->queued.heap_node);
    LinkedList2_Remove(&m->queued_list, &qflow->queued.list_node);
    qflow->is_queued = 0;
    
    // schedule send
    PacketPassInterface_Sender_Send(m->output, qflow->queued.data, qflow->queued.data_len);
    m->sending_flow = qflow;
    m->sending_len = qflow->queued.data_len;
}

static void schedule_job_handler (PacketPassFairQueue *m)
{
    ASSERT(!m->freeing)
    ASSERT(!m->sending_flow)
    DebugObject_Access(&m->d_obj);
    
    // remove previous flow
    m->previous_flow = NULL;
    
    // schedule next
    if (BHeap_GetFirst(&m->queued_heap)) {
        schedule(m);
    }
}

static void input_handler_send (PacketPassFairQueueFlow *flow, uint8_t *data, int data_len)
{
    ASSERT(!flow->m->freeing)
    ASSERT(flow != flow->m->sending_flow)
    ASSERT(!flow->is_queued)
    DebugObject_Access(&flow->d_obj);
    
    PacketPassFairQueue *m = flow->m;
    
    if (flow == m->previous_flow) {
        // remove from previous flow, its time persists as was updated by output_handler_done
        m->previous_flow = NULL;
    } else {
        // assign time
        flow->time = get_current_time(m);
    }
    
    // queue flow
    flow->queued.data = data;
    flow->queued.data_len = data_len;
    BHeap_Insert(&m->queued_heap, &flow->queued.heap_node);
    LinkedList2_Append(&m->queued_list, &flow->queued.list_node);
    flow->is_queued = 1;
    
    if (!m->sending_flow && !BPending_IsSet(&m->schedule_job)) {
        schedule(m);
    }
}

static void output_handler_done (PacketPassFairQueue *m)
{
    ASSERT(!m->freeing)
    ASSERT(m->sending_flow)
    ASSERT(!m->previous_flow)
    ASSERT(!m->sending_flow->is_queued)
    ASSERT(!BPending_IsSet(&m->schedule_job))
    
    PacketPassFairQueueFlow *flow = m->sending_flow;
    
    // sending finished
    m->sending_flow = NULL;
    
    // remember this flow so the schedule job can remove its time if it didn's send
    m->previous_flow = flow;
    
    // update flow time by packet size
    increment_sent_flow(flow, m->sending_len);
    
    // schedule schedule
    BPending_Set(&m->schedule_job);
    
    // finish flow packet
    PacketPassInterface_Done(&flow->input);
    
    // call busy handler if set
    if (flow->handler_busy) {
        // handler is one-shot, unset it before calling
        PacketPassFairQueue_handler_busy handler = flow->handler_busy;
        flow->handler_busy = NULL;
        
        // call handler
        handler(flow->user);
        return;
    }
}

void PacketPassFairQueue_Init (PacketPassFairQueue *m, PacketPassInterface *output, BPendingGroup *pg, int use_cancel)
{
    ASSERT(use_cancel == 0 || use_cancel == 1)
    if (use_cancel) {
        ASSERT(PacketPassInterface_HasCancel(output))
    }
    
    // init arguments
    m->output = output;
    m->pg = pg;
    
    // init output
    PacketPassInterface_Sender_Init(m->output, (PacketPassInterface_handler_done)output_handler_done, m);
    
    // not sending
    m->sending_flow = NULL;
    
    // no previous flow
    m->previous_flow = NULL;
    
    // init queued heap
    BHeap_Init(&m->queued_heap, OFFSET_DIFF(PacketPassFairQueueFlow, time, queued.heap_node), (BHeap_comparator)time_comparator, NULL);
    
    // init queued list
    LinkedList2_Init(&m->queued_list);
    
    // not freeing
    m->freeing = 0;
    
    // set if using cancel
    m->use_cancel = use_cancel;
    
    // init schedule job
    BPending_Init(&m->schedule_job, m->pg, (BPending_handler)schedule_job_handler, m);
    
    DebugCounter_Init(&m->d_ctr);
    DebugObject_Init(&m->d_obj);
}

void PacketPassFairQueue_Free (PacketPassFairQueue *m)
{
    ASSERT(LinkedList2_IsEmpty(&m->queued_list))
    ASSERT(!BHeap_GetFirst(&m->queued_heap))
    ASSERT(!m->previous_flow)
    ASSERT(!m->sending_flow)
    DebugCounter_Free(&m->d_ctr);
    DebugObject_Free(&m->d_obj);
    
    // free schedule job
    BPending_Free(&m->schedule_job);
}

void PacketPassFairQueue_PrepareFree (PacketPassFairQueue *m)
{
    DebugObject_Access(&m->d_obj);
    
    m->freeing = 1;
}

void PacketPassFairQueueFlow_Init (PacketPassFairQueueFlow *flow, PacketPassFairQueue *m)
{
    ASSERT(!m->freeing)
    DebugObject_Access(&m->d_obj);
    
    // init arguments
    flow->m = m;
    
    // have no canfree handler
    flow->handler_busy = NULL;
    
    // init input
    PacketPassInterface_Init(&flow->input, PacketPassInterface_GetMTU(flow->m->output), (PacketPassInterface_handler_send)input_handler_send, flow, m->pg);
    
    // is not queued
    flow->is_queued = 0;
    
    DebugCounter_Increment(&m->d_ctr);
    DebugObject_Init(&flow->d_obj);
}

void PacketPassFairQueueFlow_Free (PacketPassFairQueueFlow *flow)
{
    if (!flow->m->freeing) {
        ASSERT(flow != flow->m->sending_flow)
    }
    DebugCounter_Decrement(&flow->m->d_ctr);
    DebugObject_Free(&flow->d_obj);
    
    PacketPassFairQueue *m = flow->m;
    
    // remove from current flow
    if (flow == m->sending_flow) {
        m->sending_flow = NULL;
    }
    
    // remove from previous flow
    if (flow == m->previous_flow) {
        m->previous_flow = NULL;
    }
    
    // remove from queue
    if (flow->is_queued) {
        BHeap_Remove(&m->queued_heap, &flow->queued.heap_node);
        LinkedList2_Remove(&m->queued_list, &flow->queued.list_node);
    }
    
    // free input
    PacketPassInterface_Free(&flow->input);
}

void PacketPassFairQueueFlow_AssertFree (PacketPassFairQueueFlow *flow)
{
    if (!flow->m->freeing) {
        ASSERT(flow != flow->m->sending_flow)
    }
    DebugObject_Access(&flow->d_obj);
}

int PacketPassFairQueueFlow_IsBusy (PacketPassFairQueueFlow *flow)
{
    ASSERT(!flow->m->freeing)
    DebugObject_Access(&flow->d_obj);
    
    return (flow == flow->m->sending_flow);
}

void PacketPassFairQueueFlow_Release (PacketPassFairQueueFlow *flow)
{
    ASSERT(flow->m->use_cancel)
    ASSERT(flow == flow->m->sending_flow)
    ASSERT(!flow->m->freeing)
    ASSERT(!BPending_IsSet(&flow->m->schedule_job))
    DebugObject_Access(&flow->d_obj);
    
    PacketPassFairQueue *m = flow->m;
    
    // cancel current packet
    PacketPassInterface_Sender_Cancel(m->output);
    
    // set no sending flow
    m->sending_flow = NULL;
    
    // schedule schedule
    BPending_Set(&m->schedule_job);
}

void PacketPassFairQueueFlow_SetBusyHandler (PacketPassFairQueueFlow *flow, PacketPassFairQueue_handler_busy handler, void *user)
{
    ASSERT(flow == flow->m->sending_flow)
    ASSERT(!flow->m->freeing)
    DebugObject_Access(&flow->d_obj);
    
    flow->handler_busy = handler;
    flow->user = user;
}

PacketPassInterface * PacketPassFairQueueFlow_GetInput (PacketPassFairQueueFlow *flow)
{
    DebugObject_Access(&flow->d_obj);
    
    return &flow->input;
}
