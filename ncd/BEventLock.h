/**
 * @file BEventLock.h
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
 * A FIFO lock for events using the job queue ({@link BPending}).
 */

#ifndef BADVPN_BEVENTLOCK_H
#define BADVPN_BEVENTLOCK_H

#include <misc/debugcounter.h>
#include <structure/LinkedList2.h>
#include <base/DebugObject.h>
#include <base/BPending.h>

/**
 * Event context handler called when the lock job has acquired the lock
 * after requesting the lock with {@link BEventLockJob_Wait}.
 * The object was in waiting state.
 * The object enters locked state before the handler is called.
 * 
 * @param user as in {@link BEventLockJob_Init}
 */
typedef void (*BEventLock_handler) (void *user);

/**
 * A FIFO lock for events using the job queue ({@link BPending}).
 */
typedef struct {
    LinkedList2 jobs;
    BPending exec_job;
    DebugObject d_obj;
    DebugCounter pending_ctr;
} BEventLock;

/**
 * An object that can request a {@link BEventLock} lock.
 */
typedef struct {
    BEventLock *l;
    BEventLock_handler handler;
    void *user;
    int pending;
    LinkedList2Node pending_node;
    DebugObject d_obj;
} BEventLockJob;

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param pg pending group
 */
void BEventLock_Init (BEventLock *o, BPendingGroup *pg);

/**
 * Frees the object.
 * There must be no {@link BEventLockJob} objects using this lock
 * (regardless of their state).
 * 
 * @param o the object
 */
void BEventLock_Free (BEventLock *o);

/**
 * Initializes the object.
 * The object is initialized in idle state.
 * 
 * @param o the object
 * @param l the lock
 * @param handler handler to call when the lock is aquired
 * @param user value to pass to handler
 */
void BEventLockJob_Init (BEventLockJob *o, BEventLock *l, BEventLock_handler handler, void *user);

/**
 * Frees the object.
 * 
 * @param o the object
 */
void BEventLockJob_Free (BEventLockJob *o);

/**
 * Requests the lock.
 * The object must be in idle state.
 * The object enters waiting state.
 * 
 * @param o the object
 */
void BEventLockJob_Wait (BEventLockJob *o);

/**
 * Aborts the lock request or releases the lock.
 * The object must be in waiting or locked state.
 * The object enters idle state.
 * 
 * @param o the object
 */
void BEventLockJob_Release (BEventLockJob *o);

#endif
