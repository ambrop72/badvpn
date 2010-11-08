/**
 * @file BPRFileDesc.c
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

#include <misc/offset.h>
#include <misc/debug.h>

#include <nspr_support/BPRFileDesc.h>

#define HANDLER_READ 0
#define HANDLER_WRITE 1
#define NUM_EVENTS 2

static int get_event_index (PRInt16 event);
static void init_handlers (BPRFileDesc *obj);
static int get_bsocket_events (PRUint16 pr_events);
static void init_bottom (BPRFileDesc *obj);
static void free_bottom (BPRFileDesc *obj);
static void update_bottom (BPRFileDesc *obj);
static void set_bottom_events (BPRFileDesc *obj, PRInt16 new_events);
static void socket_handler (BPRFileDesc *obj, int event);

int get_event_index (PRInt16 event)
{
    switch (event) {
        case PR_POLL_READ:
            return HANDLER_READ;
        case PR_POLL_WRITE:
            return HANDLER_WRITE;
        default:
            ASSERT(0)
            return 0;
    }
}

static PRInt16 handler_events[] = {
    [HANDLER_READ] = PR_POLL_READ,
    [HANDLER_WRITE] = PR_POLL_WRITE
};

void init_handlers (BPRFileDesc *obj)
{
    int i;
    for (i = 0; i < NUM_EVENTS; i++) {
        obj->handlers[i] = NULL;
    }
}

void work_events (BPRFileDesc *o)
{
    ASSERT(o->dispatching)
    ASSERT(o->current_event_index >= 0)
    ASSERT(o->current_event_index <= NUM_EVENTS)
    ASSERT(((o->ready_events)&~(o->waitEvents)) == 0)
    
    while (o->current_event_index < NUM_EVENTS) {
        // get event
        int ev_index = o->current_event_index;
        PRInt16 ev_mask = handler_events[ev_index];
        int ev_dispatch = (o->ready_events&ev_mask);
        
        // jump to next event, clear this event
        o->current_event_index++;
        o->ready_events &= ~ev_mask;
        
        if (ev_dispatch) {
            // schedule job that will call further handlers, or update bottom events at the end
            BPending_Set(&o->job);
            
            // disable event before dispatching it
            BPRFileDesc_DisableEvent(o, ev_mask);
            
            // dispatch this event
            o->handlers[ev_index](o->handlers_user[ev_index], ev_mask);
            return;
        }
    }
    
    ASSERT(!o->ready_events)
    
    o->dispatching = 0;
    
    // recalculate bottom events
    update_bottom(o);
}

void job_handler (BPRFileDesc *o)
{
    ASSERT(o->dispatching)
    ASSERT(o->current_event_index >= 0)
    ASSERT(o->current_event_index <= NUM_EVENTS)
    ASSERT(((o->ready_events)&~(o->waitEvents)) == 0) // BPRFileDesc_DisableEvent clears events from ready_events
    DebugObject_Access(&o->d_obj);
    
    work_events(o);
    return;
}

void dispatch_events (BPRFileDesc *o, PRInt16 events)
{
    ASSERT(!o->dispatching)
    ASSERT((events&~(o->waitEvents)) == 0)
    
    o->dispatching = 1;
    o->ready_events = events;
    o->current_event_index = 0;
    
    work_events(o);
    return;
}

int get_bsocket_events (PRUint16 pr_events)
{
    int res = 0;
    
    if (pr_events&PR_POLL_READ) {
        res |= BSOCKET_READ;
    }
    if (pr_events&PR_POLL_WRITE) {
        res |= BSOCKET_WRITE;
    }
    
    return res;
}

void init_bottom (BPRFileDesc *obj)
{
    PRFileDesc *layer = obj->prfd;
    do {
        if (layer->identity == bsocketprfiledesc_identity) {
            obj->bottom_type = BPRFILEDESC_BOTTOM_BSOCKET;
            obj->bottom = layer;
            BSocket_AddGlobalEventHandler((BSocket *)obj->bottom->secret, (BSocket_handler)socket_handler, obj);
            return;
        }
        layer = layer->lower;
    } while (layer);
    
    ASSERT(0)
}

void free_bottom (BPRFileDesc *obj)
{
    switch (obj->bottom_type) {
        case BPRFILEDESC_BOTTOM_BSOCKET:
            BSocket_RemoveGlobalEventHandler((BSocket *)obj->bottom->secret);
            break;
        default:
            ASSERT(0)
            break;
    }
}

void update_bottom (BPRFileDesc *obj)
{
    // calculate bottom events
    PRInt16 new_bottom_events = 0;
    PRInt16 new_flags;
    PRInt16 out_flags;
    if (obj->waitEvents&PR_POLL_READ) {
        new_flags = obj->prfd->methods->poll(obj->prfd, PR_POLL_READ, &out_flags);
        if ((new_flags&out_flags) == 0) {
            new_bottom_events |= new_flags;
        }
    }
    if (obj->waitEvents&PR_POLL_WRITE) {
        new_flags = obj->prfd->methods->poll(obj->prfd, PR_POLL_WRITE, &out_flags);
        if ((new_flags&out_flags) == 0) {
            new_bottom_events |= new_flags;
        }
    }
    
    switch (obj->bottom_type) {
        case BPRFILEDESC_BOTTOM_BSOCKET:
            BSocket_SetGlobalEvents((BSocket *)obj->bottom->secret, get_bsocket_events(new_bottom_events));
            break;
        default:
            ASSERT(0)
            break;
    }
}

void socket_handler (BPRFileDesc *obj, int events)
{
    ASSERT(!obj->dispatching)
    DebugObject_Access(&obj->d_obj);
    
    // dispatch all events the user is waiting for, as there is
    // no way to know which of those are ready
    dispatch_events(obj, obj->waitEvents);
    return;
}

void BPRFileDesc_Init (BPRFileDesc *obj, PRFileDesc *prfd)
{
    obj->prfd = prfd;
    init_handlers(obj);
    obj->waitEvents = 0;
    obj->dispatching = 0;
    obj->ready_events = 0; // just initialize it so we can clear them safely from BPRFileDesc_DisableEvent
    
    // init bottom
    init_bottom(obj);
    
    // init job
    BPending_Init(&obj->job, BReactor_PendingGroup(((BSocket *)obj->bottom->secret)->bsys), (BPending_handler)job_handler, obj);
    
    DebugObject_Init(&obj->d_obj);
}

void BPRFileDesc_Free (BPRFileDesc *obj)
{
    DebugObject_Free(&obj->d_obj);
    
    // free job
    BPending_Free(&obj->job);
    
    // free bottom
    free_bottom(obj);
}

void BPRFileDesc_AddEventHandler (BPRFileDesc *obj, PRInt16 event, BPRFileDesc_handler handler, void *user)
{
    ASSERT(handler)
    DebugObject_Access(&obj->d_obj);
    
    // get index
    int index = get_event_index(event);
    
    // event must not have handler
    ASSERT(!obj->handlers[index])
    
    // change handler
    obj->handlers[index] = handler;
    obj->handlers_user[index] = user;
}

void BPRFileDesc_RemoveEventHandler (BPRFileDesc *obj, PRInt16 event)
{
    DebugObject_Access(&obj->d_obj);
    
    // get index
    int index = get_event_index(event);
    
    // event must have handler
    ASSERT(obj->handlers[index])
    
    // disable event if enabled
    if (obj->waitEvents&event) {
        BPRFileDesc_DisableEvent(obj, event);
    }
    
    // change handler
    obj->handlers[index] = NULL;
}

void BPRFileDesc_EnableEvent (BPRFileDesc *obj, PRInt16 event)
{
    DebugObject_Access(&obj->d_obj);
    
    // get index
    int index = get_event_index(event);
    
    // event must have handler
    ASSERT(obj->handlers[index])
    
    // event must not be enabled
    ASSERT(!(obj->waitEvents&event))
    
    // update events
    obj->waitEvents |= event;
    
    // update bottom
    if (!obj->dispatching) {
        update_bottom(obj);
    }
}

void BPRFileDesc_DisableEvent (BPRFileDesc *obj, PRInt16 event)
{
    DebugObject_Access(&obj->d_obj);
    
    // get index
    int index = get_event_index(event);
    
    // event must have handler
    ASSERT(obj->handlers[index])
    
    // event must be enabled
    ASSERT(obj->waitEvents&event)
    
    // update events
    obj->waitEvents &= ~event;
    obj->ready_events &= ~event;
    
    // update bottom
    if (!obj->dispatching) {
        update_bottom(obj);
    }
}
