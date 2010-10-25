/**
 * @file debugin.h
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
 * Object for detecting wrong call paths.
 */

#ifndef BADVPN_MISC_DEBUGIN_H
#define BADVPN_MISC_DEBUGIN_H

#include <misc/debug.h>

/**
 * Object for detecting wrong call paths.
 */
typedef struct {
    #ifndef NDEBUG
    int in;
    #endif
} DebugIn;

/**
 * Initializes the object.
 * The object is initialized in not in state.
 * 
 * @param o the object
 */
static void DebugIn_Init (DebugIn *o);

/**
 * Puts the object into in state.
 * The object must be in not in state.
 * The object enters in state.
 * 
 * @param o the object
 */
static void DebugIn_GoIn (DebugIn *o);

/**
 * Puts the object into not in state.
 * The object must be in in state.
 * The object enters not in state.
 * 
 * @param o the object
 */
static void DebugIn_GoOut (DebugIn *o);

/**
 * Does nothing.
 * The object must be in in state.
 * 
 * @param o the object
 */
static void DebugIn_AmIn (DebugIn *o);

/**
 * Does nothing.
 * The object must be in not in state.
 * 
 * @param o the object
 */
static void DebugIn_AmOut (DebugIn *o);

#ifndef NDEBUG

/**
 * Checks if the object is in in state.
 * Only available if NDEBUG is not defined.
 * 
 * @param o the object
 * @return 1 if in in state, 0 if in not in state
 */
static int DebugIn_In (DebugIn *o);

#endif

void DebugIn_Init (DebugIn *o)
{
    #ifndef NDEBUG
    o->in = 0;
    #endif
}

void DebugIn_GoIn (DebugIn *o)
{
    ASSERT(o->in == 0)
    
    #ifndef NDEBUG
    o->in = 1;
    #endif
}

void DebugIn_GoOut (DebugIn *o)
{
    ASSERT(o->in == 1)
    
    #ifndef NDEBUG
    o->in = 0;
    #endif
}

void DebugIn_AmIn (DebugIn *o)
{
    ASSERT(o->in == 1)
}

void DebugIn_AmOut (DebugIn *o)
{
    ASSERT(o->in == 0)
}

#ifndef NDEBUG

int DebugIn_In (DebugIn *o)
{
    ASSERT(o->in == 0 || o->in == 1)
    
    return o->in;
}

#endif

#endif
