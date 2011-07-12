/**
 * @file PacketPassPriorityQueue.c
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

#include <flow/PacketPassPriorityQueue.h>

static int int_comparator (void *user, int *prio1, int *prio2)
{
    if (*prio1 < *prio2) {
        return -1;
    }
    if (*prio1 > *prio2) {
        return 1;
    }
    return 0;
}

static void schedule (PacketPassPriorityQueue *m)
{
    ASSERT(!m->sending_flow)
    ASSERT(!m->freeing)
    ASSERT(BHeap_GetFirst(&m->queued_heap))
    
    // get first queued flow
    PacketPassPriorityQueueFlow *qflow = UPPER_OBJECT(BHeap_GetFirst(&m->queued_heap), PacketPassPriorityQueueFlow, queued.heap_node);
    ASSERT(qflow->is_queued)
    
    // remove flow from queue
    BHeap_Remove(&m->queued_heap, &qflow->queued.heap_node);
    qflow->is_queued = 0;
    
    // schedule send
    PacketPassInterface_Sender_Send(m->output, qflow->queued.data, qflow->queued.data_len);
    m->sending_flow = qflow;
}

static void schedule_job_handler (PacketPassPriorityQueue *m)
{
    ASSERT(!m->sending_flow)
    ASSERT(!m->freeing)
    DebugObject_Access(&m->d_obj);
    
    if (BHeap_GetFirst(&m->queued_heap)) {
        schedule(m);
    }
}

static void input_handler_send (PacketPassPriorityQueueFlow *flow, uint8_t *data, int data_len)
{
    PacketPassPriorityQueue *m = flow->m;
    
    ASSERT(flow != m->sending_flow)
    ASSERT(!flow->is_queued)
    ASSERT(!m->freeing)
    DebugObject_Access(&flow->d_obj);
    
    // queue flow
    flow->queued.data = data;
    flow->queued.data_len = data_len;
    BHeap_Insert(&m->queued_heap, &flow->queued.heap_node);
    flow->is_queued = 1;
    
    if (!m->sending_flow && !BPending_IsSet(&m->schedule_job)) {
        schedule(m);
    }
}

static void output_handler_done (PacketPassPriorityQueue *m)
{
    ASSERT(m->sending_flow)
    ASSERT(!BPending_IsSet(&m->schedule_job))
    ASSERT(!m->freeing)
    ASSERT(!m->sending_flow->is_queued)
    
    PacketPassPriorityQueueFlow *flow = m->sending_flow;
    
    // sending finished
    m->sending_flow = NULL;
    
    // schedule schedule
    BPending_Set(&m->schedule_job);
    
    // finish flow packet
    PacketPassInterface_Done(&flow->input);
    
    // call busy handler if set
    if (flow->handler_busy) {
        // handler is one-shot, unset it before calling
        PacketPassPriorityQueue_handler_busy handler = flow->handler_busy;
        flow->handler_busy = NULL;
        
        // call handler
        handler(flow->user);
        return;
    }
}

void PacketPassPriorityQueue_Init (PacketPassPriorityQueue *m, PacketPassInterface *output, BPendingGroup *pg, int use_cancel)
{
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
    
    // init queued heap
    BHeap_Init(&m->queued_heap, OFFSET_DIFF(PacketPassPriorityQueueFlow, priority, queued.heap_node), (BHeap_comparator)int_comparator, NULL);
    
    // not freeing
    m->freeing = 0;
    
    // init schedule job
    BPending_Init(&m->schedule_job, m->pg, (BPending_handler)schedule_job_handler, m);
    
    DebugObject_Init(&m->d_obj);
    DebugCounter_Init(&m->d_ctr);
}

void PacketPassPriorityQueue_Free (PacketPassPriorityQueue *m)
{
    ASSERT(!BHeap_GetFirst(&m->queued_heap))
    ASSERT(!m->sending_flow)
    DebugCounter_Free(&m->d_ctr);
    DebugObject_Free(&m->d_obj);
    
    // free schedule job
    BPending_Free(&m->schedule_job);
}

void PacketPassPriorityQueue_PrepareFree (PacketPassPriorityQueue *m)
{
    DebugObject_Access(&m->d_obj);
    
    // set freeing
    m->freeing = 1;
}

int PacketPassPriorityQueue_GetMTU (PacketPassPriorityQueue *m)
{
    DebugObject_Access(&m->d_obj);
    
    return PacketPassInterface_GetMTU(m->output);
}

void PacketPassPriorityQueueFlow_Init (PacketPassPriorityQueueFlow *flow, PacketPassPriorityQueue *m, int priority)
{
    ASSERT(!m->freeing)
    DebugObject_Access(&m->d_obj);
    
    // init arguments
    flow->m = m;
    flow->priority = priority;
    
    // have no canfree handler
    flow->handler_busy = NULL;
    
    // init input
    PacketPassInterface_Init(&flow->input, PacketPassInterface_GetMTU(flow->m->output), (PacketPassInterface_handler_send)input_handler_send, flow, m->pg);
    
    // is not queued
    flow->is_queued = 0;
    
    DebugObject_Init(&flow->d_obj);
    DebugCounter_Increment(&m->d_ctr);
}

void PacketPassPriorityQueueFlow_Free (PacketPassPriorityQueueFlow *flow)
{
    PacketPassPriorityQueue *m = flow->m;
    
    ASSERT(m->freeing || flow != m->sending_flow)
    DebugCounter_Decrement(&m->d_ctr);
    DebugObject_Free(&flow->d_obj);
    
    // remove from current flow
    if (flow == m->sending_flow) {
        m->sending_flow = NULL;
    }
    
    // remove from queue
    if (flow->is_queued) {
        BHeap_Remove(&m->queued_heap, &flow->queued.heap_node);
    }
    
    // free input
    PacketPassInterface_Free(&flow->input);
}

void PacketPassPriorityQueueFlow_AssertFree (PacketPassPriorityQueueFlow *flow)
{
    PacketPassPriorityQueue *m = flow->m;
    
    ASSERT(m->freeing || flow != m->sending_flow)
    DebugObject_Access(&flow->d_obj);
}

int PacketPassPriorityQueueFlow_IsBusy (PacketPassPriorityQueueFlow *flow)
{
    PacketPassPriorityQueue *m = flow->m;
    
    ASSERT(!m->freeing)
    DebugObject_Access(&flow->d_obj);
    
    return (flow == m->sending_flow);
}

void PacketPassPriorityQueueFlow_RequestCancel (PacketPassPriorityQueueFlow *flow)
{
    PacketPassPriorityQueue *m = flow->m;
    
    ASSERT(flow == m->sending_flow)
    ASSERT(m->use_cancel)
    ASSERT(!m->freeing)
    ASSERT(!BPending_IsSet(&m->schedule_job))
    DebugObject_Access(&flow->d_obj);
    
    // request cancel
    PacketPassInterface_Sender_RequestCancel(m->output);
}

void PacketPassPriorityQueueFlow_SetBusyHandler (PacketPassPriorityQueueFlow *flow, PacketPassPriorityQueue_handler_busy handler, void *user)
{
    PacketPassPriorityQueue *m = flow->m;
    
    ASSERT(flow == m->sending_flow)
    ASSERT(!m->freeing)
    DebugObject_Access(&flow->d_obj);
    
    // set handler
    flow->handler_busy = handler;
    flow->user = user;
}

PacketPassInterface * PacketPassPriorityQueueFlow_GetInput (PacketPassPriorityQueueFlow *flow)
{
    DebugObject_Access(&flow->d_obj);
    
    return &flow->input;
}
