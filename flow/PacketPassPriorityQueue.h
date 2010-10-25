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

#include <misc/dead.h>
#include <misc/debugin.h>
#include <system/DebugObject.h>
#include <misc/debugcounter.h>
#include <structure/BHeap.h>
#include <system/BPending.h>
#include <flow/PacketPassInterface.h>

typedef void (*PacketPassPriorityQueue_handler_busy) (void *user);

struct PacketPassPriorityQueueFlow_s;

/**
 * Priority queue using {@link PacketPassInterface}.
 */
typedef struct {
    dead_t dead;
    PacketPassInterface *output;
    struct PacketPassPriorityQueueFlow_s *sending_flow;
    int sending_len;
    BHeap queued_heap;
    int freeing;
    int use_cancel;
    BPending continue_job;
    DebugCounter d_ctr;
    DebugIn in_output;
    DebugObject d_obj;
} PacketPassPriorityQueue;

typedef struct PacketPassPriorityQueueFlow_s {
    dead_t dead;
    PacketPassPriorityQueue *m;
    PacketPassPriorityQueue_handler_busy handler_busy;
    void *user;
    PacketPassInterface input;
    int priority;
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
 */
void PacketPassPriorityQueue_Init (PacketPassPriorityQueue *m, PacketPassInterface *output, BPendingGroup *pg);

/**
 * Frees the queue.
 * All flows must have been freed.
 *
 * @param m the object
 */
void PacketPassPriorityQueue_Free (PacketPassPriorityQueue *m);

/**
 * Enables cancel functionality.
 * This allows freeing flows even if they're busy by releasing them.
 * Output must support {@link PacketPassInterface} cancel functionality.
 * May only be called once.
 */
void PacketPassPriorityQueue_EnableCancel (PacketPassPriorityQueue *m);

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
 * Determines if the flow is busy. If the flow is considered busy, it must not
 * be freed.
 * Queue must not be in freeing state.
 * Must not be called from queue calls to output.
 *
 * @param flow the object
 * @return 0 if not busy, 1 is busy
 */
int PacketPassPriorityQueueFlow_IsBusy (PacketPassPriorityQueueFlow *flow);

/**
 * Cancels the packet that is currently being sent to output in order
 * to allow freeing the flow.
 * Cancel functionality must be enabled for the queue.
 * The flow must be busy as indicated by {@link PacketPassPriorityQueueFlow_IsBusy}.
 * Queue must not be in freeing state.
 * Must not be called from queue calls to output.
 * Will call Cancel on output. Will not invoke any input I/O.
 * After this, {@link PacketPassPriorityQueueFlow_IsBusy} will report the flow as not busy.
 * The flow's input's Done will never be called (the flow will become inoperable).
 * 
 * @param flow the object
 */
void PacketPassPriorityQueueFlow_Release (PacketPassPriorityQueueFlow *flow);

/**
 * Sets up a callback to be called when the flow is no longer busy.
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
