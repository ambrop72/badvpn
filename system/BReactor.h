/**
 * @file BReactor.h
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
 * Event loop that supports file desciptor (Linux) or HANDLE (Windows) events
 * and timers.
 */

#ifndef BADVPN_SYSTEM_BREACTOR_H
#define BADVPN_SYSTEM_BREACTOR_H

#ifdef BADVPN_USE_WINAPI
#include <windows.h>
#else
#include <sys/epoll.h>
#endif

#include <stdint.h>

#include <misc/debug.h>
#include <misc/debugcounter.h>
#include <system/DebugObject.h>
#include <structure/LinkedList2.h>
#include <structure/BHeap.h>
#include <system/BTime.h>
#include <system/BPending.h>

struct BTimer_t;

/**
 * Handler function invoked when the timer expires.
 * The timer was in running state.
 * The timer enters not running state before this function is invoked.
 * This function is being called from within the timer's previosly
 * associated reactor.
 *
 * @param user value passed to {@link BTimer_Init}
 */
typedef void (*BTimer_handler) (void *user);

/**
 * Timer object used with {@link BReactor}.
 */
typedef struct BTimer_t {
    btime_t msTime;
    BTimer_handler handler;
    void *handler_pointer;

    uint8_t active;
    uint8_t expired;
    btime_t absTime;
    union {
        BHeapNode heap_node;
        LinkedList2Node list_node;
    };
} BTimer;

/**
 * Initializes the timer object.
 * The timer object is initialized in not running state.
 *
 * @param bt the object
 * @param msTime default timeout in milliseconds
 * @param handler handler function invoked when the timer expires
 * @param user value to pass to the handler function
 */
void BTimer_Init (BTimer *bt, btime_t msTime, BTimer_handler handler, void *user);

/**
 * Checks if the timer is running.
 *
 * @param bt the object
 * @return 1 if running, 0 if not running
 */
int BTimer_IsRunning (BTimer *bt);

#ifdef BADVPN_USE_WINAPI

/**
 * Handler function invoked by the reactor when the handle is signalled
 * The handle object is in active state, is being called from within
 * the associated reactor, and is in monitored state.
 *
 * @param user value passed to {@link BHandle_Init}
 */
typedef void (*BHandle_handler) (void *user);

struct BHandle_t;

/**
 * Windows handle object used with {@link BReactor}.
 */
typedef struct BHandle_t {
    HANDLE h;
    BHandle_handler handler;
    void *user;
    int active;
    int position;
} BHandle;

/**
 * Initializes the handle object.
 * The handle object is initialized in not active state.
 *
 * @param bh the object
 * @param handle underlying Windows handle
 * @param handler handler function invoked when the handle is signalled
 * @param user value to pass to the handler function
 */
void BHandle_Init (BHandle *bh, HANDLE handle, BHandle_handler handler, void *user);

#else

struct BFileDescriptor_t;

#define BREACTOR_READ (1 << 0)
#define BREACTOR_WRITE (1 << 1)
#define BREACTOR_ERROR (1 << 2)

/**
 * Handler function invoked by the reactor when one or more events are detected.
 * The events argument will contain a subset of the monitored events (BREACTOR_READ, BREACTOR_WRITE),
 * plus possibly the error event (BREACTOR_ERROR).
 * The file descriptor object is in active state, being called from within
 * the associated reactor.
 *
 * @param user value passed to {@link BFileDescriptor_Init}
 * @param events bitmask composed of a subset of monitored events (BREACTOR_READ, BREACTOR_WRITE),
 *               and possibly the error event (BREACTOR_ERROR).
 *               Will be nonzero.
 */
typedef void (*BFileDescriptor_handler) (void *user, int events);

/**
 * File descriptor object used with {@link BReactor}.
 */
typedef struct BFileDescriptor_t {
    int fd;
    BFileDescriptor_handler handler;
    void *user;
    int active;
    int waitEvents;
    struct BFileDescriptor_t **epoll_returned_ptr;
} BFileDescriptor;

/**
 * Intializes the file descriptor object.
 * The object is initialized in not active state.
 *
 * @param bs file descriptor object to initialize
 * @param fb file descriptor to represent
 * @param handler handler function invoked by the reactor when a monitored event is detected
 * @param user value passed to the handler functuon
 */
void BFileDescriptor_Init (BFileDescriptor *bs, int fd, BFileDescriptor_handler handler, void *user);

#endif

// BReactor

#define BSYSTEM_MAX_RESULTS 64
#define BSYSTEM_MAX_HANDLES 64

/**
 * Event loop that supports file desciptor (Linux) or HANDLE (Windows) events
 * and timers.
 */
typedef struct {
    DebugObject d_obj;
    
    int exiting;
    int exit_code;
    
    // jobs
    BPendingGroup pending_jobs;
    
    // timers
    BHeap timers_heap;
    LinkedList2 timers_expired_list;
    
    #ifdef BADVPN_USE_WINAPI
    
    int num_handles; // number of user handles
    int enabled_num; // number of user handles in the enabled array
    HANDLE enabled_handles[BSYSTEM_MAX_HANDLES]; // enabled user handles
    BHandle *enabled_objects[BSYSTEM_MAX_HANDLES]; // objects corresponding to enabled handles
    BHandle *returned_object;
    
    #else
    
    int efd; // epoll fd
    struct epoll_event epoll_results[BSYSTEM_MAX_RESULTS]; // epoll returned events buffer
    int epoll_results_num; // number of events in the array
    int epoll_results_pos; // number of events processed so far
    
    DebugCounter d_fds_counter;
    
    #endif
} BReactor;

/**
 * Initializes the reactor.
 * {@link BLog_Init} must have been done.
 * {@link BTime_Init} must have been done.
 *
 * @param bsys the object
 * @return 1 on success, 0 on failure
 */
int BReactor_Init (BReactor *bsys) WARN_UNUSED;

/**
 * Frees the reactor.
 * Must not be called from within the event loop ({@link BReactor_Exec}).
 * There must be no {@link BPending} objects using the pending group
 * returned by {@link BReactor_PendingGroup}.
 * There must be no running timers in this reactor.
 * There must be no file descriptors or handles registered
 * with this reactor.
 *
 * @param bsys the object
 */
void BReactor_Free (BReactor *bsys);

/**
 * Runs the event loop.
 *
 * @param bsys the object
 * @return value passed to {@link BReactor_Quit}
 */
int BReactor_Exec (BReactor *bsys);

/**
 * Causes the event loop ({@link BReactor_Exec}) to cease
 * dispatching events and return.
 * Any further calls of {@link BReactor_Exec} will return immediately.
 *
 * @param bsys the object
 * @param code value {@link BReactor_Exec} should return. If this is
 *             called more than once, it will return the last code.
 */
void BReactor_Quit (BReactor *bsys, int code);

/**
 * Starts a timer to expire after its default time.
 * The timer must have been initialized with {@link BTimer_Init}.
 * If the timer is in running state, it must be associated with this reactor.
 * The timer enters running state, associated with this reactor.
 *
 * @param bsys the object
 * @param bt timer to start
 */
void BReactor_SetTimer (BReactor *bsys, BTimer *bt);

/**
 * Starts a timer to expire after a given time.
 * The timer must have been initialized with {@link BTimer_Init}.
 * If the timer is in running state, it must be associated with this reactor.
 * The timer enters running state, associated with this reactor.
 *
 * @param bsys the object
 * @param bt timer to start
 * @param after relative expiration time
 */
void BReactor_SetTimerAfter (BReactor *bsys, BTimer *bt, btime_t after);

/**
 * Starts a timer to expire at the specified time.
 * The timer must have been initialized with {@link BTimer_Init}.
 * If the timer is in running state, it must be associated with this reactor.
 * The timer enters running state, associated with this reactor.
 * The timer's expiration time is set to the time argument.
 *
 * @param bsys the object
 * @param bt timer to start
 * @param time absolute expiration time (according to {@link btime_gettime})
 */
void BReactor_SetTimerAbsolute (BReactor *bsys, BTimer *bt, btime_t time);

/**
 * Stops a timer.
 * If the timer is in running state, it must be associated with this reactor.
 * The timer enters not running state.
 *
 * @param bsys the object
 * @param bt timer to stop
 */
void BReactor_RemoveTimer (BReactor *bsys, BTimer *bt);

/**
 * Returns a {@link BPendingGroup} object that can be used to schedule jobs for
 * the reactor to execute. These jobs have complete priority over other events
 * (timers, file descriptors and Windows handles).
 * The returned pending group may only be used as an argument to {@link BPending_Init},
 * and must not be accessed by other means.
 * All {@link BPending} objects using this group must be freed before freeing
 * the reactor.
 * 
 * @param bsys the object
 * @return pending group for scheduling jobs for the reactor to execute
 */
BPendingGroup * BReactor_PendingGroup (BReactor *bsys);

/**
 * Executes pending jobs until either:
 *   - the reference job is reached, or
 *   - {@link BReactor_Quit} is called.
 * The reference job must be reached before the job list empties.
 * The reference job will not be executed.
 * 
 * WARNING: Use with care. This should only be used to to work around third-party software
 * that does not integrade into the jobs system. In particular, you should think about:
 *   - the effects the jobs to be executed may have, and
 *   - the environment those jobs expect to be executed in.
 * 
 * @param bsys the object
 * @param ref reference job. It is not accessed in any way, only its address is compared to
 *            pending jobs before they are executed.
 * @return 1 if the reference job was reached,
 *         0 if {@link BReactor_Quit} was called (either while executing a job, or before)
 */
int BReactor_Synchronize (BReactor *bsys, BPending *ref);

#ifdef BADVPN_USE_WINAPI

/**
 * Registers a Windows handle to be monitored.
 *
 * @param bsys the object
 * @param bh handle object. Must have been initialized with {@link BHandle_Init}.
 *           Must be in not active state. On success, the handle object enters
 *           active state, associated with this reactor, and is initialized
 *           to not monitored state.
 * @return 1 on success, 0 on failure
 */
int BReactor_AddHandle (BReactor *bsys, BHandle *bh) WARN_UNUSED;

/**
 * Unregisters a Windows handle.
 *
 * @param bsys the object
 * @param bh handle object. Must be in active state, associated with this reactor.
 *           The handle object enters not active state.
 */
void BReactor_RemoveHandle (BReactor *bsys, BHandle *bh);

/**
 * Starts monitoring a Windows handle.
 *
 * @param bsys the object
 * @param bh handle object. Must be in active state, associated with this reactor.
 *           Must be in not monitored state.
 *           The handle object enters monitored state.
 */
void BReactor_EnableHandle (BReactor *bsys, BHandle *bh);

/**
 * Stops monitoring a Windows handle.
 *
 * @param bsys the object
 * @param bh handle object. Must be in active state, associated with this reactor.
 *           Must be in monitored state.
 *           The handle object enters not monitored state.
 */
void BReactor_DisableHandle (BReactor *bsys, BHandle *bh);

#else

/**
 * Starts monitoring a file descriptor.
 *
 * @param bsys the object
 * @param bs file descriptor object. Must have been initialized with
 *           {@link BFileDescriptor_Init} Must be in not active state.
 *           On success, the file descriptor object enters active state,
 *           associated with this reactor.
 * @return 1 on success, 0 on failure
 */
int BReactor_AddFileDescriptor (BReactor *bsys, BFileDescriptor *bs) WARN_UNUSED;

/**
 * Stops monitoring a file descriptor.
 *
 * @param bsys the object
 * @param bs {@link BFileDescriptor} object. Must be in active state,
 *           associated with this reactor. The file descriptor object
 *           enters not active state.
 */
void BReactor_RemoveFileDescriptor (BReactor *bsys, BFileDescriptor *bs);

/**
 * Sets monitored file descriptor events.
 *
 * @param bsys the object
 * @param bs {@link BFileDescriptor} object. Must be in active state,
 *           associated with this reactor.
 * @param events events to watch for. Must not have any bits other than
 *               BREACTOR_READ and BREACTOR_WRITE.
 *               This overrides previosly monitored events.
 */
void BReactor_SetFileDescriptorEvents (BReactor *bsys, BFileDescriptor *bs, int events);

#endif

#endif
