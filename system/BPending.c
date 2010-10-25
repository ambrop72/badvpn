/**
 * @file BPending.c
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

#include <stddef.h>

#include <misc/debug.h>
#include <misc/offset.h>
#include <system/BPending.h>

void BPendingGroup_Init (BPendingGroup *g)
{
    // init jobs list
    LinkedList2_Init(&g->jobs);
    
    // init pending counter
    DebugCounter_Init(&g->pending_ctr);
    
    // init debug object
    DebugObject_Init(&g->d_obj);
}

void BPendingGroup_Free (BPendingGroup *g)
{
    DebugCounter_Free(&g->pending_ctr);
    ASSERT(LinkedList2_IsEmpty(&g->jobs))
    DebugObject_Free(&g->d_obj);
}

int BPendingGroup_HasJobs (BPendingGroup *g)
{
    DebugObject_Access(&g->d_obj);
    
    return !LinkedList2_IsEmpty(&g->jobs);
}

void BPendingGroup_ExecuteJob (BPendingGroup *g)
{
    ASSERT(!LinkedList2_IsEmpty(&g->jobs))
    DebugObject_Access(&g->d_obj);
    
    // get a job
    LinkedList2Node *node = LinkedList2_GetFirst(&g->jobs);
    BPending *p = UPPER_OBJECT(node, BPending, pending_node);
    ASSERT(p->pending)
    
    // remove from jobs list
    LinkedList2_Remove(&g->jobs, &p->pending_node);
    
    // set not pending
    p->pending = 0;
    
    // execute job
    p->handler(p->user);
    return;
}

void BPending_Init (BPending *o, BPendingGroup *g, BPending_handler handler, void *user)
{
    // init arguments
    o->g = g;
    o->handler = handler;
    o->user = user;
    
    // set not pending
    o->pending = 0;
    
    // increment pending counter
    DebugCounter_Increment(&o->g->pending_ctr);
    
    // init debug object
    DebugObject_Init(&o->d_obj);
}

void BPending_Free (BPending *o)
{
    DebugCounter_Decrement(&o->g->pending_ctr);
    DebugObject_Free(&o->d_obj);
    
    // remove from jobs list
    if (o->pending) {
        LinkedList2_Remove(&o->g->jobs, &o->pending_node);
    }
}

void BPending_Set (BPending *o)
{
    DebugObject_Access(&o->d_obj);
    
    // remove from jobs list
    if (o->pending) {
        LinkedList2_Remove(&o->g->jobs, &o->pending_node);
    }
    
    // insert to jobs list
    LinkedList2_Append(&o->g->jobs, &o->pending_node);
    
    // set pending
    o->pending = 1;
}

void BPending_Unset (BPending *o)
{
    DebugObject_Access(&o->d_obj);
    
    if (o->pending) {
        // remove from jobs list
        LinkedList2_Remove(&o->g->jobs, &o->pending_node);
        
        // set not pending
        o->pending = 0;
    }
}
