/**
 * @file PRStreamSink.h
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
 * A {@link StreamPassInterface} sink for a NSPR file descriptor (PRFileDesc) via {@link BPRFileDesc}.
 */

#ifndef BADVPN_NSPRSUPPORT_PRSTREAMSINK_H
#define BADVPN_NSPRSUPPORT_PRSTREAMSINK_H

#include <stdint.h>

#include <misc/dead.h>
#include <system/DebugObject.h>
#include <flow/StreamPassInterface.h>
#include <flow/error.h>
#include <nspr_support/BPRFileDesc.h>

#define PRSTREAMSINK_ERROR_NSPR 1

/**
 * A {@link StreamPassInterface} sink for a NSPR file descriptor (PRFileDesc) via {@link BPRFileDesc}.
 */
typedef struct {
    FlowErrorReporter rep;
    BPRFileDesc *bprfd;
    StreamPassInterface input;
    int in_len;
    uint8_t *in;
    DebugObject d_obj;
    #ifndef NDEBUG
    dead_t d_dead;
    #endif
} PRStreamSink;

/**
 * Initializes the object.
 *
 * @param s the object
 * @param rep error reporting data. Error code is an int. Possible error codes:
 *              - PRSTREAMSINK_ERROR_NSPR: {@link PR_Write} failed
 *                with an unhandled error code
 *            The object must be freed from the error handler.
 * @param bprfd the {@link BPRFileDesc} object to write data to. Registers a
 *              PR_POLL_WRITE handler which must not be registered.
 * @param pg pending group
 */
void PRStreamSink_Init (PRStreamSink *s, FlowErrorReporter rep, BPRFileDesc *bprfd, BPendingGroup *pg);

/**
 * Frees the object.
 *
 * @param s the object
 */
void PRStreamSink_Free (PRStreamSink *s);

/**
 * Returns the input interface.
 *
 * @param s the object
 * @return input interface
 */
StreamPassInterface * PRStreamSink_GetInput (PRStreamSink *s);

#endif
