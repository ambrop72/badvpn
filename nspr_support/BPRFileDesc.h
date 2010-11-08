/**
 * @file BPRFileDesc.h
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
 * Object used for obtaining notifications for available I/O operations
 * on NSPR file descriptors (PRFileDesc) with supported bottom layers.
 * Currently only the {@link BSocketPRFileDesc} bottom layer is supported.
 */

#ifndef BADVPN_NSPRSUPPORT_BPRFILEDESC_H
#define BADVPN_NSPRSUPPORT_BPRFILEDESC_H

#include <system/BPending.h>
#include <system/DebugObject.h>
#include <nspr_support/BSocketPRFileDesc.h>

#define BPRFILEDESC_BOTTOM_BSOCKET 1

/**
 * Handler function called when an event occurs on the NSPR file descriptor.
 * It is guaranteed that the event had a handler and was enabled.
 * The event is disabled before the handler is called.
 * 
 * It is guaranteed that the handler returns control to the reactor immediately.
 * 
 * @param user as in {@link BPRFileDesc_AddEventHandler}
 * @param event event being reported
 */
typedef void (*BPRFileDesc_handler) (void *user, PRInt16 event);

/**
 * Object used for obtaining notifications for available I/O operations
 * on NSPR file descriptors (PRFileDesc) with supported bottom layers.
 * Currently only the {@link BSocketPRFileDesc} bottom layer is supported.
 */
typedef struct {
    DebugObject d_obj;
    BReactor *reactor;
    PRFileDesc *prfd;
    BPRFileDesc_handler handlers[2];
    void *handlers_user[2];
    PRInt16 waitEvents;
    
    // event dispatching
    int dispatching;
    PRInt16 ready_events;
    int current_event_index;
    BPending job;
    
    int bottom_type;
    PRFileDesc *bottom;
} BPRFileDesc;

/**
 * Initializes the object.
 * 
 * @param obj the object
 * @param prfd NSPR file descriptor for which notifications are needed.
 *             Its bottom layer must be a {@link BSocketPRFileDesc}.
 *             The bottom {@link BSocket} must not have any event handlers
 *             registered (socket-global or event-specific).
 *             This object registers a socket-global event handler for
 *             the bottom {@link BSocket}.
 */
void BPRFileDesc_Init (BPRFileDesc *obj, PRFileDesc *prfd);

/**
 * Frees the object.
 * @param obj the object
 */
void BPRFileDesc_Free (BPRFileDesc *obj);

/**
 * Registers a handler for an event.
 * The event must not already have a handler.
 * 
 * @param obj the object
 * @param event NSPR event to register the handler for. Must be
 *              PR_POLL_READ or PR_POLL_WRITE.
 * @param user value to pass to handler
 */
void BPRFileDesc_AddEventHandler (BPRFileDesc *obj, PRInt16 event, BPRFileDesc_handler handler, void *user);

/**
 * Unregisters a handler for an event.
 * The event must have a handler.
 * 
 * @param obj the object
 * @param event NSPR event to unregister the handler for
 */
void BPRFileDesc_RemoveEventHandler (BPRFileDesc *obj, PRInt16 event);

/**
 * Enables monitoring of an event.
 * The event must have a handler.
 * The event must not be enabled.
 * If the operation associated with the event can already be performed,
 * the handler for the event may never be called.
 * 
 * @param obj the object
 * @param event NSPR event to enable monitoring for
 */
void BPRFileDesc_EnableEvent (BPRFileDesc *obj, PRInt16 event);

/**
 * Disables monitoring of an event.
 * The event must have a handler.
 * The event must be enabled.
 * 
 * @param obj the object
 * @param event NSPR event to disable monitoring for
 */
void BPRFileDesc_DisableEvent (BPRFileDesc *obj, PRInt16 event);

#endif
