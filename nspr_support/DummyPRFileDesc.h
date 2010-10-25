/**
 * @file DummyPRFileDesc.h
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
 * Dummy NSPR file descriptor (PRFileDesc).
 * Used for creating a model SSL file descriptor to cache various stuff
 * to improve performance.
 */

#ifndef BADVPN_NSPRSUPPORT_DUMMYPRFILEDESC_H
#define BADVPN_NSPRSUPPORT_DUMMYPRFILEDESC_H

#include <prio.h>

#include <misc/debug.h>

extern PRDescIdentity dummyprfiledesc_identity;

/**
 * Globally initialize the dummy NSPR file descriptor backend.
 * Must not have been called successfully.
 *
 * @return 1 on success, 0 on failure
 */
int DummyPRFileDesc_GlobalInit (void) WARN_UNUSED;

/**
 * Creates a dummy NSPR file descriptor.
 * {@link DummyPRFileDesc_GlobalInit} must have been done.
 *
 * @param prfd uninitialized PRFileDesc structure
 */
void DummyPRFileDesc_Create (PRFileDesc *prfd);

#endif
