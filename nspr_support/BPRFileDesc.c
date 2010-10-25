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

static void init_handlers (BPRFileDesc *obj);
static int get_event_index (int event);
static int get_bsocket_events (PRUint16 pr_events);
static void init_bottom (BPRFileDesc *obj);
static void free_bottom (BPRFileDesc *obj);
static void update_bottom (BPRFileDesc *obj);
static void set_bottom_events (BPRFileDesc *obj, PRInt16 new_events);
static void socket_handler (BPRFileDesc *obj, int event);

void init_handlers (BPRFileDesc *obj)
{
    int i;
    for (i = 0; i < 2; i++) {
        obj->handlers[i] = NULL;
    }
}

int get_event_index (int event)
{
    switch (event) {
        case PR_POLL_READ:
            return 0;
        case PR_POLL_WRITE:
            return 1;
        default:
            ASSERT(0)
            return 0;
    }
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
    // make sure bottom events are not recalculated whenever an event
    // is disabled or enabled, it's faster to do it only once
    obj->in_handler = 1;
    
    // call handlers for all enabled events
    
    if (obj->waitEvents&PR_POLL_READ) {
        BPRFileDesc_DisableEvent(obj, PR_POLL_READ);
        DEAD_ENTER(obj->dead)
        obj->handlers[0](obj->handlers_user[0], PR_POLL_READ);
        if (DEAD_LEAVE(obj->dead)) {
            return;
        }
    }
    
    if (obj->waitEvents&PR_POLL_WRITE) {
        BPRFileDesc_DisableEvent(obj, PR_POLL_WRITE);
        DEAD_ENTER(obj->dead)
        obj->handlers[1](obj->handlers_user[1], PR_POLL_WRITE);
        if (DEAD_LEAVE(obj->dead)) {
            return;
        }
    }
    
    // recalculate bottom events
    obj->in_handler = 0;
    update_bottom(obj);
}

void BPRFileDesc_Init (BPRFileDesc *obj, PRFileDesc *prfd)
{
    DEAD_INIT(obj->dead);
    obj->prfd = prfd;
    init_handlers(obj);
    obj->waitEvents = 0;
    init_bottom(obj);
    obj->in_handler = 0;
    
    // init debug object
    DebugObject_Init(&obj->d_obj);
}

void BPRFileDesc_Free (BPRFileDesc *obj)
{
    // free debug object
    DebugObject_Free(&obj->d_obj);

    free_bottom(obj);
    DEAD_KILL(obj->dead);
}

void BPRFileDesc_AddEventHandler (BPRFileDesc *obj, PRInt16 event, BPRFileDesc_handler handler, void *user)
{
    ASSERT(handler)
    
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
    // get index
    int index = get_event_index(event);
    
    // event must have handler
    ASSERT(obj->handlers[index])
    
    // event must not be enabled
    ASSERT(!(obj->waitEvents&event))
    
    // update events
    obj->waitEvents |= event;
    
    // update bottom
    if (!obj->in_handler) {
        update_bottom(obj);
    }
}

void BPRFileDesc_DisableEvent (BPRFileDesc *obj, PRInt16 event)
{
    // get index
    int index = get_event_index(event);
    
    // event must have handler
    ASSERT(obj->handlers[index])
    
    // event must be enabled
    ASSERT(obj->waitEvents&event)
    
    // update events
    obj->waitEvents &= ~event;
    
    // update bottom
    if (!obj->in_handler) {
        update_bottom(obj);
    }
}
