/**
 * @file BTime.h
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
 * System time abstraction used by {@link BReactor}.
 */

#ifndef BADVPN_SYSTEM_BTIME_H
#define BADVPN_SYSTEM_BTIME_H

#ifdef BADVPN_USE_WINAPI
#include <windows.h>
#else
#include <time.h>
#endif

#include <stdint.h>

#include <misc/debug.h>

typedef int64_t btime_t;

struct _BTime_global {
    #ifndef NDEBUG
    int initialized; // initialized statically
    #endif
    #ifdef BADVPN_USE_WINAPI
    LARGE_INTEGER start_time;
    #else
    btime_t start_time;
    #endif
};

extern struct _BTime_global btime_global;

static void BTime_Init (void)
{
    ASSERT(!btime_global.initialized)
    
    #ifdef BADVPN_USE_WINAPI
    ASSERT_FORCE(QueryPerformanceCounter(&btime_global.start_time))
    #else
    struct timespec ts;
    ASSERT_FORCE(clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
    btime_global.start_time = (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec/1000000;
    #endif
    
    #ifndef NDEBUG
    btime_global.initialized = 1;
    #endif
}

static btime_t btime_gettime ()
{
    ASSERT(btime_global.initialized)
    
    #ifdef BADVPN_USE_WINAPI
    
    LARGE_INTEGER count;
    LARGE_INTEGER freq;
    ASSERT_FORCE(QueryPerformanceCounter(&count))
    ASSERT_FORCE(QueryPerformanceFrequency(&freq))
    return (((count.QuadPart - btime_global.start_time.QuadPart) * 1000) / freq.QuadPart);
    
    #else
    
    struct timespec ts;
    ASSERT_FORCE(clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
    return (((int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec/1000000) - btime_global.start_time);
    
    #endif
}

#endif
