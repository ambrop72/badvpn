/**
 * @file debugerror.h
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
 * Mechanism for ensuring an object is destroyed from inside an error handler
 * or its jobs.
 */

#ifndef BADVPN_MISC_DEBUGERROR_H
#define BADVPN_MISC_DEBUGERROR_H

#include <misc/debug.h>
#include <base/BPending.h>

#ifndef NDEBUG
    #define DEBUGERROR(de, call) \
        { \
            ASSERT(!BPending_IsSet(&(de)->job)) \
            BPending_Set(&(de)->job); \
            (call); \
        }
#else
    #define DEBUGERROR(de, call) { (call); }
#endif

typedef struct {
    #ifndef NDEBUG
    BPending job;
    #endif
} DebugError;

static void DebugError_Init (DebugError *o, BPendingGroup *pg);
static void DebugError_Free (DebugError *o);
static void DebugError_AssertNoError (DebugError *o);

#ifndef NDEBUG
static void _DebugError_job_handler (DebugError *o)
{
    ASSERT(0);
}
#endif

void DebugError_Init (DebugError *o, BPendingGroup *pg)
{
    #ifndef NDEBUG
    BPending_Init(&o->job, pg, (BPending_handler)_DebugError_job_handler, o);
    #endif
}

void DebugError_Free (DebugError *o)
{
    #ifndef NDEBUG
    BPending_Free(&o->job);
    #endif
}

void DebugError_AssertNoError (DebugError *o)
{
    #ifndef NDEBUG
    ASSERT(!BPending_IsSet(&o->job))
    #endif
}

#endif
