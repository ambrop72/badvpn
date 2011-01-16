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
#include <misc/minmax.h>

#include <flow/PacketPassFairQueue.h>

// reduce this to test time overflow handling
#define FAIRQUEUE_MAX_TIME UINT64_MAX

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
    ASSERT(iamount <= FAIRQUEUE_MAX_TIME)
    ASSERT(!flow->is_queued)
    ASSERT(!flow->m->sending_flow)
    
    PacketPassFairQueue *m = flow->m;
    uint64_t amount = iamount;
    
    // does time overflow?
    if (amount > FAIRQUEUE_MAX_TIME - flow->time) {
        // get time to subtract
        uint64_t subtract;
        BHeapNode *heap_node = BHeap_GetFirst(&m->queued_heap);
        if (!heap_node) {
            subtract = flow->time;
        } else {
            PacketPassFairQueueFlow *first_flow = UPPER_OBJECT(heap_node, PacketPassFairQueueFlow, queued.heap_node);
            ASSERT(first_flow->is_queued)
            subtract = first_flow->time;
        }
        
        // subtract time from all flows
        LinkedList2Iterator it;
        LinkedList2Iterator_InitForward(&it, &m->flows_list);
        LinkedList2Node *list_node;
        while (list_node = LinkedList2Iterator_Next(&it)) {
            PacketPassFairQueueFlow *someflow = UPPER_OBJECT(list_node, PacketPassFairQueueFlow, list_node);
            
            // don't subtract more time than there is, except for the just finished flow,
            // where we allow time to underflow and then overflow to the correct value after adding to it
            if (subtract > someflow->time && someflow != flow) {
                ASSERT(!someflow->is_queued)
                someflow->time = 0;
            } else {
                someflow->time -= subtract;
            }
        }
    }
    
    // add time to flow
    flow->time += amount;
}

static void schedule (PacketPassFairQueue *m)
{
    ASSERT(!m->sending_flow)
    ASSERT(!m->previous_flow)
    ASSERT(!m->freeing)
    ASSERT(BHeap_GetFirst(&m->queued_heap))
    
    // get first queued flow
    PacketPassFairQueueFlow *qflow = UPPER_OBJECT(BHeap_GetFirst(&m->queued_heap), PacketPassFairQueueFlow, queued.heap_node);
    ASSERT(qflow->is_queued)
    
    // remove flow from queue
    BHeap_Remove(&m->queued_heap, &qflow->queued.heap_node);
    qflow->is_queued = 0;
    
    // schedule send
    PacketPassInterface_Sender_Send(m->output, qflow->queued.data, qflow->queued.data_len);
    m->sending_flow = qflow;
    m->sending_len = qflow->queued.data_len;
}

static void schedule_job_handler (PacketPassFairQueue *m)
{
    ASSERT(!m->sending_flow)
    ASSERT(!m->freeing)
    DebugObject_Access(&m->d_obj);
    
    // remove previous flow
    m->previous_flow = NULL;
    
    if (BHeap_GetFirst(&m->queued_heap)) {
        schedule(m);
    }
}

static void input_handler_send (PacketPassFairQueueFlow *flow, uint8_t *data, int data_len)
{
    PacketPassFairQueue *m = flow->m;
    
    ASSERT(flow != m->sending_flow)
    ASSERT(!flow->is_queued)
    ASSERT(!m->freeing)
    DebugObject_Access(&flow->d_obj);
    
    if (flow == m->previous_flow) {
        // remove from previous flow
        m->previous_flow = NULL;
    } else {
        // raise time
        flow->time = BMAX(flow->time, get_current_time(m));
    }
    
    // queue flow
    flow->queued.data = data;
    flow->queued.data_len = data_len;
    BHeap_Insert(&m->queued_heap, &flow->queued.heap_node);
    flow->is_queued = 1;
    
    if (!m->sending_flow && !BPending_IsSet(&m->schedule_job)) {
        schedule(m);
    }
}

static void output_handler_done (PacketPassFairQueue *m)
{
    ASSERT(m->sending_flow)
    ASSERT(!m->previous_flow)
    ASSERT(!BPending_IsSet(&m->schedule_job))
    ASSERT(!m->freeing)
    ASSERT(!m->sending_flow->is_queued)
    
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
    ASSERT(PacketPassInterface_GetMTU(output) <= FAIRQUEUE_MAX_TIME)
    ASSERT(use_cancel == 0 || use_cancel == 1)
    ASSERT(!use_cancel || PacketPassInterface_HasCancel(output))
    
    // init arguments
    m->output = output;
    m->pg = pg;
    m->use_cancel = use_cancel;
    
    // init output
    PacketPassInterface_Sender_Init(m->output, (PacketPassInterface_handler_done)output_handler_done, m);
    
    // not sending
    m->sending_flow = NULL;
    
    // no previous flow
    m->previous_flow = NULL;
    
    // init queued heap
    BHeap_Init(&m->queued_heap, OFFSET_DIFF(PacketPassFairQueueFlow, time, queued.heap_node), (BHeap_comparator)time_comparator, NULL);
    
    // init flows list
    LinkedList2_Init(&m->flows_list);
    
    // not freeing
    m->freeing = 0;
    
    // init schedule job
    BPending_Init(&m->schedule_job, m->pg, (BPending_handler)schedule_job_handler, m);
    
    DebugObject_Init(&m->d_obj);
    DebugCounter_Init(&m->d_ctr);
}

void PacketPassFairQueue_Free (PacketPassFairQueue *m)
{
    ASSERT(LinkedList2_IsEmpty(&m->flows_list))
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
    
    // set freeing
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
    
    // set time
    flow->time = 0;
    
    // add to flows list
    LinkedList2_Append(&m->flows_list, &flow->list_node);
    
    // is not queued
    flow->is_queued = 0;
    
    DebugObject_Init(&flow->d_obj);
    DebugCounter_Increment(&m->d_ctr);
}

void PacketPassFairQueueFlow_Free (PacketPassFairQueueFlow *flow)
{
    PacketPassFairQueue *m = flow->m;
    
    ASSERT(m->freeing || flow != m->sending_flow)
    DebugCounter_Decrement(&m->d_ctr);
    DebugObject_Free(&flow->d_obj);
    
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
    }
    
    // remove from flows list
    LinkedList2_Remove(&m->flows_list, &flow->list_node);
    
    // free input
    PacketPassInterface_Free(&flow->input);
}

void PacketPassFairQueueFlow_AssertFree (PacketPassFairQueueFlow *flow)
{
    PacketPassFairQueue *m = flow->m;
    
    ASSERT(m->freeing || flow != m->sending_flow)
    DebugObject_Access(&flow->d_obj);
}

int PacketPassFairQueueFlow_IsBusy (PacketPassFairQueueFlow *flow)
{
    PacketPassFairQueue *m = flow->m;
    
    ASSERT(!m->freeing)
    DebugObject_Access(&flow->d_obj);
    
    return (flow == m->sending_flow);
}

void PacketPassFairQueueFlow_Release (PacketPassFairQueueFlow *flow)
{
    PacketPassFairQueue *m = flow->m;
    
    ASSERT(flow == m->sending_flow)
    ASSERT(m->use_cancel)
    ASSERT(!m->freeing)
    ASSERT(!BPending_IsSet(&m->schedule_job))
    DebugObject_Access(&flow->d_obj);
    
    // set no sending flow
    m->sending_flow = NULL;
    
    // schedule schedule
    BPending_Set(&m->schedule_job);
    
    // cancel current packet
    PacketPassInterface_Sender_Cancel(m->output);
}

void PacketPassFairQueueFlow_SetBusyHandler (PacketPassFairQueueFlow *flow, PacketPassFairQueue_handler_busy handler, void *user)
{
    PacketPassFairQueue *m = flow->m;
    
    ASSERT(flow == m->sending_flow)
    ASSERT(!m->freeing)
    DebugObject_Access(&flow->d_obj);
    
    // set handler
    flow->handler_busy = handler;
    flow->user = user;
}

PacketPassInterface * PacketPassFairQueueFlow_GetInput (PacketPassFairQueueFlow *flow)
{
    DebugObject_Access(&flow->d_obj);
    
    return &flow->input;
}
