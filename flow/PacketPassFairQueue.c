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

static int call_send (PacketPassFairQueue *m, uint8_t *data, int data_len)
{
    DebugIn_GoIn(&m->in_output);
    DEAD_ENTER(m->dead)
    int res = PacketPassInterface_Sender_Send(m->output, data, data_len);
    if (DEAD_LEAVE(m->dead)) {
        return -1;
    }
    DebugIn_GoOut(&m->in_output);
    
    ASSERT(!m->freeing)
    ASSERT(res == 0 || res == 1)
    
    return res;
}

static int call_cancel (PacketPassFairQueue *m)
{
    DebugIn_GoIn(&m->in_output);
    DEAD_ENTER(m->dead)
    PacketPassInterface_Sender_Cancel(m->output);
    if (DEAD_LEAVE(m->dead)) {
        return -1;
    }
    DebugIn_GoOut(&m->in_output);
    
    ASSERT(!m->freeing)
    
    return 0;
}

static int call_done (PacketPassFairQueue *m, PacketPassFairQueueFlow *flow)
{
    DEAD_ENTER_N(m, m->dead)
    DEAD_ENTER_N(flow, flow->dead)
    PacketPassInterface_Done(&flow->input);
    DEAD_LEAVE_N(m, m->dead);
    DEAD_LEAVE_N(flow, flow->dead);
    
    if (DEAD_KILLED_N(m)) {
        return -1;
    }
    
    ASSERT(!m->freeing)
    
    if (!DEAD_KILLED_N(flow)) {
        ASSERT(flow->have_time)
        if (flow != m->sending_flow && !flow->is_queued) {
            flow->have_time = 0;
        }
    }
    
    return 0;
}

static uint64_t get_current_time (PacketPassFairQueue *m)
{
    if (m->sending_flow) {
        return m->sending_flow->time;
    }
    
    BHeapNode *heap_node = BHeap_GetFirst(&m->queued_heap);
    if (!heap_node) {
        return 0;
    }
    
    PacketPassFairQueueFlow *first_flow = UPPER_OBJECT(heap_node, PacketPassFairQueueFlow, queued.heap_node);
    ASSERT(first_flow->is_queued)
    ASSERT(first_flow->have_time)
    
    return first_flow->time;
}

static void increment_sent_flow (PacketPassFairQueueFlow *flow, int iamount)
{
    ASSERT(iamount >= 0)
    ASSERT(iamount <= UINT64_MAX)
    ASSERT(flow->have_time)
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
            ASSERT(first_flow->have_time)
            // subtract lowest time from all queued flows
            uint64_t subtract = first_flow->time;
            LinkedList2Iterator it;
            LinkedList2Iterator_InitForward(&it, &m->queued_list);
            LinkedList2Node *list_node;
            while (list_node = LinkedList2Iterator_Next(&it)) {
                PacketPassFairQueueFlow *queue_flow = UPPER_OBJECT(list_node, PacketPassFairQueueFlow, queued.list_node);
                ASSERT(queue_flow->is_queued)
                ASSERT(queue_flow->have_time)
                queue_flow->time -= subtract;
            }
            // update the given flow's time; note we subtract because it isn't in the queue
            // TODO: prove this is correct
            flow->time = flow->time - subtract + amount;
        }
    }
}

static void process_queue (PacketPassFairQueue *m)
{
    ASSERT(!m->freeing)
    ASSERT(!m->sending_flow)
    
    do {
        // get first queued flow
        BHeapNode *heap_node = BHeap_GetFirst(&m->queued_heap);
        if (!heap_node) {
            return;
        }
        PacketPassFairQueueFlow *qflow = UPPER_OBJECT(heap_node, PacketPassFairQueueFlow, queued.heap_node);
        ASSERT(qflow->is_queued)
        ASSERT(qflow->have_time)
        
        // remove flow from queue
        BHeap_Remove(&m->queued_heap, &qflow->queued.heap_node);
        LinkedList2_Remove(&m->queued_list, &qflow->queued.list_node);
        qflow->is_queued = 0;
        
        // try to send the packet
        int res = call_send(m, qflow->queued.data, qflow->queued.data_len);
        if (res < 0) {
            return;
        }
        
        if (res == 0) {
            // sending in progress
            m->sending_flow = qflow;
            m->sending_len = qflow->queued.data_len;
            return;
        }
        
        // increment flow time
        increment_sent_flow(qflow, qflow->queued.data_len);
        
        // notify sender
        if (call_done(m, qflow) < 0) {
            return;
        }
    } while (!m->sending_flow);
}

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

static int input_handler_send (PacketPassFairQueueFlow *flow, uint8_t *data, int data_len)
{
    ASSERT(!flow->m->freeing)
    ASSERT(flow != flow->m->sending_flow)
    ASSERT(!flow->is_queued)
    DebugIn_AmOut(&flow->m->in_output);
    
    PacketPassFairQueue *m = flow->m;
    
    // assign time if needed
    int had_time = flow->have_time;
    if (!flow->have_time) {
        flow->time = get_current_time(m);
        flow->have_time = 1;
    }
    
    // if nothing is being sent and queue is empty, send immediately without queueing
    if (!m->sending_flow && !BHeap_GetFirst(&m->queued_heap)) {
        int res = call_send(m, data, data_len);
        if (res < 0) {
            return -1;
        }
        
        if (res == 0) {
            // output busy, continue in output_handler_done
            m->sending_flow = flow;
            m->sending_len = data_len;
            return 0;
        }
        
        // if flow had no time before it shouldn't have after
        if (!had_time) {
            flow->have_time = 0;
        }
        
        return 1;
    }
    
    // add flow to queue
    flow->queued.data = data;
    flow->queued.data_len = data_len;
    BHeap_Insert(&m->queued_heap, &flow->queued.heap_node);
    LinkedList2_Append(&m->queued_list, &flow->queued.list_node);
    flow->is_queued = 1;
    
    return 0;
}

static void output_handler_done (PacketPassFairQueue *m)
{
    ASSERT(!m->freeing)
    ASSERT(m->sending_flow)
    ASSERT(!m->sending_flow->is_queued)
    ASSERT(m->sending_flow->have_time)
    DebugIn_AmOut(&m->in_output);
    
    PacketPassFairQueueFlow *flow = m->sending_flow;
    
    // sending finished
    m->sending_flow = NULL;
    
    // update flow time by packet size
    increment_sent_flow(flow, m->sending_len);
    
    // call busy handler if set
    if (flow->handler_busy) {
        // handler is one-shot, unset it before calling
        PacketPassFairQueue_handler_busy handler = flow->handler_busy;
        flow->handler_busy = NULL;
        
        // call handler
        DEAD_ENTER_N(m, m->dead)
        DEAD_ENTER_N(flow, flow->dead)
        handler(flow->user);
        DEAD_LEAVE_N(m, m->dead);
        DEAD_LEAVE_N(flow, flow->dead);
        if (DEAD_KILLED_N(m)) {
            return;
        }
        if (DEAD_KILLED_N(flow)) {
            flow = NULL;
        }
        
        ASSERT(!m->freeing)
    }
    
    // report completion to sender
    if (flow) {
        if (call_done(m, flow) < 0) {
            return;
        }
    }
    
    // process queued flows
    if (!m->sending_flow) {
        process_queue(m);
        return;
    }
}

static void job_handler (PacketPassFairQueue *m)
{
    ASSERT(!m->freeing)
    
    if (!m->sending_flow) {
        process_queue(m);
        return;
    }
}

void PacketPassFairQueue_Init (PacketPassFairQueue *m, PacketPassInterface *output, BPendingGroup *pg)
{
    // init arguments
    m->output = output;
    
    // init dead var
    DEAD_INIT(m->dead);
    
    // init output
    PacketPassInterface_Sender_Init(m->output, (PacketPassInterface_handler_done)output_handler_done, m);
    
    // not sending
    m->sending_flow = NULL;
    
    // init queued heap
    BHeap_Init(&m->queued_heap, OFFSET_DIFF(PacketPassFairQueueFlow, time, queued.heap_node), (BHeap_comparator)time_comparator, NULL);
    
    // init queued list
    LinkedList2_Init(&m->queued_list);
    
    // not freeing
    m->freeing = 0;
    
    // not using cancel
    m->use_cancel = 0;
    
    // init continue job
    BPending_Init(&m->continue_job, pg, (BPending_handler)job_handler, m);
    
    // init debug counter
    DebugCounter_Init(&m->d_ctr);
    
    // init debug in output
    DebugIn_Init(&m->in_output);
    
    // init debug object
    DebugObject_Init(&m->d_obj);
}

void PacketPassFairQueue_Free (PacketPassFairQueue *m)
{
    ASSERT(!BHeap_GetFirst(&m->queued_heap))
    ASSERT(LinkedList2_IsEmpty(&m->queued_list))
    ASSERT(!m->sending_flow)
    DebugCounter_Free(&m->d_ctr);
    DebugObject_Free(&m->d_obj);
    
    // free continue job
    BPending_Free(&m->continue_job);
    
    // free dead var
    DEAD_KILL(m->dead);
}

void PacketPassFairQueue_EnableCancel (PacketPassFairQueue *m)
{
    ASSERT(!m->use_cancel)
    ASSERT(PacketPassInterface_HasCancel(m->output))
    
    // using cancel
    m->use_cancel = 1;
}

void PacketPassFairQueue_PrepareFree (PacketPassFairQueue *m)
{
    m->freeing = 1;
}

void PacketPassFairQueueFlow_Init (PacketPassFairQueueFlow *flow, PacketPassFairQueue *m)
{
    ASSERT(!m->freeing)
    DebugIn_AmOut(&m->in_output);
    
    // init arguments
    flow->m = m;
    
    // init dead var
    DEAD_INIT(flow->dead);
    
    // have no canfree handler
    flow->handler_busy = NULL;
    
    // init input
    PacketPassInterface_Init(&flow->input, PacketPassInterface_GetMTU(flow->m->output), (PacketPassInterface_handler_send)input_handler_send, flow);
    
    // doesn't have time
    flow->have_time = 0;
    
    // is not queued
    flow->is_queued = 0;
    
    // increment debug counter
    DebugCounter_Increment(&m->d_ctr);
    
    // init debug object
    DebugObject_Init(&flow->d_obj);
}

void PacketPassFairQueueFlow_Free (PacketPassFairQueueFlow *flow)
{
    if (!flow->m->freeing) {
        ASSERT(flow != flow->m->sending_flow)
        DebugIn_AmOut(&flow->m->in_output);
    }
    DebugCounter_Decrement(&flow->m->d_ctr);
    DebugObject_Free(&flow->d_obj);
    
    PacketPassFairQueue *m = flow->m;
    
    // remove current flow
    if (flow == flow->m->sending_flow) {
        flow->m->sending_flow = NULL;
    }
    
    // remove from queue
    if (flow->is_queued) {
        BHeap_Remove(&m->queued_heap, &flow->queued.heap_node);
        LinkedList2_Remove(&m->queued_list, &flow->queued.list_node);
    }
    
    // free input
    PacketPassInterface_Free(&flow->input);
    
    // free dead var
    DEAD_KILL(flow->dead);
}

int PacketPassFairQueueFlow_IsBusy (PacketPassFairQueueFlow *flow)
{
    ASSERT(!flow->m->freeing)
    DebugIn_AmOut(&flow->m->in_output);
    
    return (flow == flow->m->sending_flow);
}

void PacketPassFairQueueFlow_Release (PacketPassFairQueueFlow *flow)
{
    ASSERT(flow->m->use_cancel)
    ASSERT(flow == flow->m->sending_flow)
    ASSERT(!flow->m->freeing)
    DebugIn_AmOut(&flow->m->in_output);
    
    PacketPassFairQueue *m = flow->m;
    
    // cancel current packet
    if (call_cancel(m) < 0) {
        return;
    }
    
    // set no sending flow
    m->sending_flow = NULL;
    
    // set continue job
    BPending_Set(&m->continue_job);
}

void PacketPassFairQueueFlow_SetBusyHandler (PacketPassFairQueueFlow *flow, PacketPassFairQueue_handler_busy handler, void *user)
{
    ASSERT(flow == flow->m->sending_flow)
    ASSERT(!flow->m->freeing)
    DebugIn_AmOut(&flow->m->in_output);
    
    flow->handler_busy = handler;
    flow->user = user;
}

PacketPassInterface * PacketPassFairQueueFlow_GetInput (PacketPassFairQueueFlow *flow)
{
    return &flow->input;
}
