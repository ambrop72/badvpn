/**
 * @file BEventLock.c
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

#include <misc/offset.h>
#include <system/BEventLock.h>

static void exec_job_handler (BEventLock *o)
{
    ASSERT(!LinkedList2_IsEmpty(&o->jobs))
    DebugObject_Access(&o->d_obj);
    
    // get job
    BEventLockJob *j = UPPER_OBJECT(LinkedList2_GetFirst(&o->jobs), BEventLockJob, pending_node);
    ASSERT(j->pending)
    
    // call handler
    j->handler(j->user);
    return;
}

void BEventLock_Init (BEventLock *o, BPendingGroup *pg)
{
    // init jobs list
    LinkedList2_Init(&o->jobs);
    
    // init exec job
    BPending_Init(&o->exec_job, pg, (BPending_handler)exec_job_handler, o);
    
    DebugObject_Init(&o->d_obj);
    DebugCounter_Init(&o->pending_ctr);
}

void BEventLock_Free (BEventLock *o)
{
    ASSERT(LinkedList2_IsEmpty(&o->jobs))
    DebugCounter_Free(&o->pending_ctr);
    DebugObject_Free(&o->d_obj);
    
    // free exec jobs
    BPending_Free(&o->exec_job);
}

void BEventLockJob_Init (BEventLockJob *o, BEventLock *l, BEventLock_handler handler, void *user)
{
    // init arguments
    o->l = l;
    o->handler = handler;
    o->user = user;
    
    // set not pending
    o->pending = 0;
    
    DebugObject_Init(&o->d_obj);
    DebugCounter_Increment(&l->pending_ctr);
}

void BEventLockJob_Free (BEventLockJob *o)
{
    BEventLock *l = o->l;
    
    DebugCounter_Decrement(&l->pending_ctr);
    DebugObject_Free(&o->d_obj);
    
    if (o->pending) {
        int was_first = (&o->pending_node == LinkedList2_GetFirst(&l->jobs));
        
        // remove from jobs list
        LinkedList2_Remove(&l->jobs, &o->pending_node);
        
        // schedule/unschedule job
        if (was_first) {
            if (LinkedList2_IsEmpty(&l->jobs)) {
                BPending_Unset(&l->exec_job);
            } else {
                BPending_Set(&l->exec_job);
            }
        }
    }
}

void BEventLockJob_Wait (BEventLockJob *o)
{
    BEventLock *l = o->l;
    ASSERT(!o->pending)
    
    // append to jobs
    LinkedList2_Append(&l->jobs, &o->pending_node);
    
    // set pending
    o->pending = 1;
    
    // schedule next job if needed
    if (&o->pending_node == LinkedList2_GetFirst(&l->jobs)) {
        BPending_Set(&l->exec_job);
    }
}

void BEventLockJob_Release (BEventLockJob *o)
{
    BEventLock *l = o->l;
    ASSERT(o->pending)
    
    int was_first = (&o->pending_node == LinkedList2_GetFirst(&l->jobs));
    
    // remove from jobs list
    LinkedList2_Remove(&l->jobs, &o->pending_node);
    
    // set not pending
    o->pending = 0;
    
    // schedule/unschedule job
    if (was_first) {
        if (LinkedList2_IsEmpty(&l->jobs)) {
            BPending_Unset(&l->exec_job);
        } else {
            BPending_Set(&l->exec_job);
        }
    }
}
