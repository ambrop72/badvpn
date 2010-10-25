/**
 * @file BSignal.h
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
 * A global object for catching program termination requests.
 */

#ifndef BADVPN_SYSTEM_BSIGNAL_H
#define BADVPN_SYSTEM_BSIGNAL_H

#include <misc/debug.h>
#include <system/BReactor.h>

typedef void (*BSignal_handler) (void *user);

/**
 * Initializes signal handling.
 * The object is created in not capturing state.
 * {@link BLog_Init} must have been done.
 *
 * @return 1 on success, 0 on failure
 */
int BSignal_Init (void) WARN_UNUSED;

/**
 * Starts capturing signals.
 * The object must be in not capturing state.
 * The object enters capturing state.
 */
void BSignal_Capture (void);

/**
 * Stops capturing signals.
 * The object must be in capturing state.
 * A signal handler must not be configured.
 * The object enters not capturing state.
 */
void BSignal_Uncapture (void);

/**
 * Configures a reactor and a handler for signals.
 * The object must be in capturing state.
 * A handler must not be already configured.
 *
 * @param reactor {@link BReactor} from which the handler will be called
 * @param handler callback function invoked from the reactor
 * @param user value passed to callback function
 * @return 1 on success, 0 on failure.
 */
int BSignal_SetHandler (BReactor *reactor, BSignal_handler handler, void *user) WARN_UNUSED;

/**
 * Deconfigures a signal reactor and handler.
 * A handler must be configured.
 */
void BSignal_RemoveHandler (void);

#endif
