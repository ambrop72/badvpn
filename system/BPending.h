/**
 * @file BPending.h
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
 * Module for managing a queue of jobs pending execution.
 */

#ifndef BADVPN_SYSTEM_BPENDING_H
#define BADVPN_SYSTEM_BPENDING_H

#include <misc/debugcounter.h>
#include <structure/LinkedList2.h>
#include <system/DebugObject.h>

/**
 * Job execution handler.
 * It is guaranteed that the associated {@link BPending} object was
 * in set state.
 * The {@link BPending} object enters not set state before the handler
 * is called.
 * 
 * @param user as in {@link BPending_Init}
 */
typedef void (*BPending_handler) (void *user);

/**
 * Object that contains a list of jobs pending execution.
 */
typedef struct {
    LinkedList2 jobs;
    DebugCounter pending_ctr;
    DebugObject d_obj;
} BPendingGroup;

/**
 * Object for queuing a job for execution.
 */
typedef struct {
    BPendingGroup *g;
    BPending_handler handler;
    void *user;
    int pending;
    LinkedList2Node pending_node;
    DebugObject d_obj;
} BPending;

/**
 * Initializes the object.
 * 
 * @param g the object
 */
void BPendingGroup_Init (BPendingGroup *g);

/**
 * Frees the object.
 * There must be no {@link BPending} objects using this group.
 * 
 * @param g the object
 */
void BPendingGroup_Free (BPendingGroup *g);

/**
 * Checks if there is at least one job in the queue.
 * 
 * @param g the object
 * @return 1 if there is at least one job, 0 if not
 */
int BPendingGroup_HasJobs (BPendingGroup *g);

/**
 * Executes the top job on the job list.
 * The job is removed from the list and enters
 * not set state before being executed.
 * There must be at least one job in job list.
 * 
 * @param g the object
 */
void BPendingGroup_ExecuteJob (BPendingGroup *g);

/**
 * Initializes the object.
 * The object is initialized in not set state.
 * 
 * @param o the object
 * @param g pending group to use
 * @param handler job execution handler
 * @param user value to pass to handler
 */
void BPending_Init (BPending *o, BPendingGroup *g, BPending_handler handler, void *user);

/**
 * Frees the object.
 * The execution handler will not be called after the object
 * is freed.
 * 
 * @param o the object
 */
void BPending_Free (BPending *o);

/**
 * Enables the job, pushing it to the top of the job list.
 * If the object was already in set state, the job is removed from its
 * current position in the list before being pushed.
 * The object enters set state.
 * 
 * @param o the object
 */
void BPending_Set (BPending *o);

/**
 * Disables the job, removing it from the job list.
 * If the object was not in set state, nothing is done.
 * The object enters not set state.
 * 
 * @param o the object
 */
void BPending_Unset (BPending *o);

/**
 * Checks if the job is in set state.
 * 
 * @param o the object
 * @return 1 if in set state, 0 if not
 */
int BPending_IsSet (BPending *o);

#endif
