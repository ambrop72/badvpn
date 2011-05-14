/**
 * @file DebugObject.h
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
 * Object used for detecting leaks.
 */

#ifndef BADVPN_DEBUGOBJECT_H
#define BADVPN_DEBUGOBJECT_H

#include <stdint.h>

#ifdef BADVPN_THREADWORK_USE_PTHREAD
#include <pthread.h>
#endif

#include <misc/debug.h>
#include <misc/debugcounter.h>

#define DEBUGOBJECT_VALID UINT32_C(0x31415926)

/**
 * Object used for detecting leaks.
 */
typedef struct {
    #ifndef NDEBUG
    uint32_t c;
    #endif
} DebugObject;

/**
 * Initializes the object.
 * 
 * @param obj the object
 */
static void DebugObject_Init (DebugObject *obj);

/**
 * Frees the object.
 * 
 * @param obj the object
 */
static void DebugObject_Free (DebugObject *obj);

/**
 * Does nothing.
 * 
 * @param obj the object
 */
static void DebugObject_Access (const DebugObject *obj);

/**
 * Does nothing.
 * There must be no {@link DebugObject}'s initialized.
 */
static void DebugObjectGlobal_Finish (void);

extern DebugCounter debugobject_counter;
#ifdef BADVPN_THREADWORK_USE_PTHREAD
extern pthread_mutex_t debugobject_mutex;
#endif

void DebugObject_Init (DebugObject *obj)
{
    #ifndef NDEBUG
    
    obj->c = DEBUGOBJECT_VALID;
    
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    ASSERT_FORCE(pthread_mutex_lock(&debugobject_mutex) == 0)
    #endif
    
    DebugCounter_Increment(&debugobject_counter);
    
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    ASSERT_FORCE(pthread_mutex_unlock(&debugobject_mutex) == 0)
    #endif
    
    #endif
}

void DebugObject_Free (DebugObject *obj)
{
    ASSERT(obj->c == DEBUGOBJECT_VALID)
    
    #ifndef NDEBUG
    
    obj->c = 0;
    
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    ASSERT_FORCE(pthread_mutex_lock(&debugobject_mutex) == 0)
    #endif
    
    DebugCounter_Decrement(&debugobject_counter);
    
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    ASSERT_FORCE(pthread_mutex_unlock(&debugobject_mutex) == 0)
    #endif
    
    #endif
}

void DebugObject_Access (const DebugObject *obj)
{
    ASSERT(obj->c == DEBUGOBJECT_VALID)
}

void DebugObjectGlobal_Finish (void)
{
    #ifndef NDEBUG
    DebugCounter_Free(&debugobject_counter);
    #endif
}

#endif
