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
 * @param reactor {@link BReactor} from which the handler will be called
 * @param handler callback function invoked from the reactor
 * @param user value passed to callback function
 * @return 1 on success, 0 on failure
 */
int BSignal_Init (BReactor *reactor, BSignal_handler handler, void *user) WARN_UNUSED;

/**
 * Finishes signal handling.
 * {@link BSignal_Init} must not be called again.
 */
void BSignal_Finish (void);

#endif
