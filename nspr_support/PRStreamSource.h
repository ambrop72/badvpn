/**
 * @file PRStreamSource.h
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
 * A {@link StreamRecvInterface} source for a NSPR file descriptor (PRFileDesc) via {@link BPRFileDesc}.
 */

#ifndef BADVPN_NSPRSUPPORT_PRSTREAMSOURCE_H
#define BADVPN_NSPRSUPPORT_PRSTREAMSOURCE_H

#include <stdint.h>

#include <misc/dead.h>
#include <system/DebugObject.h>
#include <flow/StreamRecvInterface.h>
#include <flow/error.h>
#include <nspr_support/BPRFileDesc.h>

#define PRSTREAMSOURCE_ERROR_CLOSED 0
#define PRSTREAMSOURCE_ERROR_NSPR 1

/**
 * A {@link StreamRecvInterface} source for a NSPR file descriptor (PRFileDesc) via {@link BPRFileDesc}.
 */
typedef struct {
    FlowErrorReporter rep;
    BPRFileDesc *bprfd;
    StreamRecvInterface output;
    int out_avail;
    uint8_t *out;
    DebugObject d_obj;
    #ifndef NDEBUG
    dead_t d_dead;
    #endif
} PRStreamSource;

/**
 * Initializes the object.
 *
 * @param s the object
 * @param rep error reporting data. Error code is an int. Possible error codes:
 *              - PRSTREAMSOURCE_ERROR_CLOSED: {@link PR_Read} returned 0
 *              - PRSTREAMSOURCE_ERROR_NSPR: {@link PR_Read} failed
 *                with an unhandled error code
 *            The object must be freed from the error handler.
 * @param bprfd the {@link BPRFileDesc} object to read data from. Registers a
 *              PR_POLL_READ handler which must not be registered.
 * @param pg pending group
 */
void PRStreamSource_Init (PRStreamSource *s, FlowErrorReporter rep, BPRFileDesc *bprfd, BPendingGroup *pg);

/**
 * Frees the object.
 *
 * @param s the object
 */
void PRStreamSource_Free (PRStreamSource *s);

/**
 * Returns the output interface.
 *
 * @param s the object
 * @return output interface
 */
StreamRecvInterface * PRStreamSource_GetOutput (PRStreamSource *s);

#endif
