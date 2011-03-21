/**
 * @file PacketPassPriorityQueue.h
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
 * Priority queue using {@link PacketPassInterface}.
 */

#ifndef BADVPN_FLOW_PACKETPASSPRIORITYQUEUE_H
#define BADVPN_FLOW_PACKETPASSPRIORITYQUEUE_H

#include <stdint.h>

#include <misc/debugcounter.h>
#include <structure/BHeap.h>
#include <system/DebugObject.h>
#include <system/BPending.h>
#include <flow/PacketPassInterface.h>

typedef void (*PacketPassPriorityQueue_handler_busy) (void *user);

struct PacketPassPriorityQueueFlow_s;

/**
 * Priority queue using {@link PacketPassInterface}.
 */
typedef struct {
    PacketPassInterface *output;
    BPendingGroup *pg;
    int use_cancel;
    struct PacketPassPriorityQueueFlow_s *sending_flow;
    BHeap queued_heap;
    int freeing;
    BPending schedule_job;
    DebugObject d_obj;
    DebugCounter d_ctr;
} PacketPassPriorityQueue;

typedef struct PacketPassPriorityQueueFlow_s {
    PacketPassPriorityQueue *m;
    int priority;
    PacketPassPriorityQueue_handler_busy handler_busy;
    void *user;
    PacketPassInterface input;
    int is_queued;
    struct {
        BHeapNode heap_node;
        uint8_t *data;
        int data_len;
    } queued;
    DebugObject d_obj;
} PacketPassPriorityQueueFlow;

/**
 * Initializes the queue.
 *
 * @param m the object
 * @param output output interface
 * @param pg pending group
 * @param use_cancel whether cancel functionality is required. Must be 0 or 1.
 *                   If 1, output must support cancel functionality.
 */
void PacketPassPriorityQueue_Init (PacketPassPriorityQueue *m, PacketPassInterface *output, BPendingGroup *pg, int use_cancel);

/**
 * Frees the queue.
 * All flows must have been freed.
 *
 * @param m the object
 */
void PacketPassPriorityQueue_Free (PacketPassPriorityQueue *m);

/**
 * Prepares for freeing the entire queue. Must be called to allow freeing
 * the flows in the process of freeing the entire queue.
 * After this function is called, flows and the queue must be freed
 * before any further I/O.
 * May be called multiple times.
 * The queue enters freeing state.
 *
 * @param m the object
 */
void PacketPassPriorityQueue_PrepareFree (PacketPassPriorityQueue *m);

/**
 * Initializes a queue flow.
 * Queue must not be in freeing state.
 * Must not be called from queue calls to output.
 *
 * @param flow the object
 * @param m queue to attach to
 * @param priority flow priority. Lower value means higher priority.
 */
void PacketPassPriorityQueueFlow_Init (PacketPassPriorityQueueFlow *flow, PacketPassPriorityQueue *m, int priority);

/**
 * Frees a queue flow.
 * Unless the queue is in freeing state:
 * - The flow must not be busy as indicated by {@link PacketPassPriorityQueueFlow_IsBusy}.
 * - Must not be called from queue calls to output.
 *
 * @param flow the object
 */
void PacketPassPriorityQueueFlow_Free (PacketPassPriorityQueueFlow *flow);

/**
 * Does nothing.
 * It must be possible to free the flow (see {@link PacketPassPriorityQueueFlow}).
 * 
 * @param flow the object
 */
void PacketPassPriorityQueueFlow_AssertFree (PacketPassPriorityQueueFlow *flow);

/**
 * Determines if the flow is busy. If the flow is considered busy, it must not
 * be freed. At any given time, at most one flow will be indicated as busy.
 * Queue must not be in freeing state.
 * Must not be called from queue calls to output.
 *
 * @param flow the object
 * @return 0 if not busy, 1 is busy
 */
int PacketPassPriorityQueueFlow_IsBusy (PacketPassPriorityQueueFlow *flow);

/**
 * Requests the output to stop processing the current packet as soon as possible.
 * Cancel functionality must be enabled for the queue.
 * The flow must be busy as indicated by {@link PacketPassPriorityQueueFlow_IsBusy}.
 * Queue must not be in freeing state.
 * 
 * @param flow the object
 */
void PacketPassPriorityQueueFlow_RequestCancel (PacketPassPriorityQueueFlow *flow);

/**
 * Sets up a callback to be called when the flow is no longer busy.
 * The handler will be called as soon as the flow is no longer busy, i.e. it is not
 * possible that this flow is no longer busy before the handler is called.
 * The flow must be busy as indicated by {@link PacketPassPriorityQueueFlow_IsBusy}.
 * Queue must not be in freeing state.
 * Must not be called from queue calls to output.
 *
 * @param flow the object
 * @param handler callback function. NULL to disable.
 * @param user value passed to callback function. Ignored if handler is NULL.
 */
void PacketPassPriorityQueueFlow_SetBusyHandler (PacketPassPriorityQueueFlow *flow, PacketPassPriorityQueue_handler_busy handler, void *user);

/**
 * Returns the input interface of the flow.
 *
 * @param flow the object
 * @return input interface
 */
PacketPassInterface * PacketPassPriorityQueueFlow_GetInput (PacketPassPriorityQueueFlow *flow);

#endif
