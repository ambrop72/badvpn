/**
 * @file BThreadWork.h
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
 * System for performing computations (possibly) in parallel with the event loop
 * in a different thread.
 */

#ifndef BADVPN_BTHREADWORK_BTHREADWORK_H
#define BADVPN_BTHREADWORK_BTHREADWORK_H

#ifdef BADVPN_THREADWORK_USE_PTHREAD
    #include <pthread.h>
    #include <semaphore.h>
#endif

#include <misc/debug.h>
#include <structure/LinkedList2.h>
#include <system/DebugObject.h>
#include <system/BReactor.h>

#define BTHREADWORK_STATE_PENDING 1
#define BTHREADWORK_STATE_RUNNING 2
#define BTHREADWORK_STATE_FINISHED 3
#define BTHREADWORK_STATE_FORGOTTEN 4

struct BThreadWork_s;

/**
 * Function called to do the work for a {@link BThreadWork}.
 * The function may be called in another thread, in parallel with the event loop.
 * 
 * @param user as work_func_user in {@link BThreadWork_Init}
 */
typedef void (*BThreadWork_work_func) (void *user);

/**
 * Handler called when a {@link BThreadWork} work is done.
 * 
 * @param user as in {@link BThreadWork_Init}
 */
typedef void (*BThreadWork_handler_done) (void *user);

typedef struct {
    BReactor *reactor;
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    int num_threads;
    LinkedList2 pending_list;
    struct BThreadWork_s *running_work;
    LinkedList2 finished_list;
    pthread_mutex_t mutex;
    pthread_cond_t new_cond;
    int pipe[2];
    BFileDescriptor bfd;
    BPending more_job;
    int cancel;
    pthread_t thread;
    #endif
    DebugObject d_obj;
    DebugCounter d_ctr;
} BThreadWorkDispatcher;

typedef struct BThreadWork_s {
    BThreadWorkDispatcher *d;
    BThreadWork_handler_done handler_done;
    void *user;
    BThreadWork_work_func work_func;
    void *work_func_user;
    union {
        #ifdef BADVPN_THREADWORK_USE_PTHREAD
        struct {
            LinkedList2Node list_node;
            int state;
            sem_t finished_sem;
        };
        #endif
        struct {
            BPending job;
        };
    };
    DebugObject d_obj;
} BThreadWork;

/**
 * Initializes the work dispatcher.
 * Works may be started using {@link BThreadWork_Init}.
 * 
 * @param o the object
 * @param reactor reactor we live in
 * @param num_threads_hint hint for the number of threads to use:
 *                         <0 - A choice will be made automatically, probably based on the number of CPUs.
 *                         0 - No additional threads will be used, and computations will be performed directly
 *                             in the event loop in job handlers.
 * @return 1 on success, 0 on failure
 */
int BThreadWorkDispatcher_Init (BThreadWorkDispatcher *o, BReactor *reactor, int num_threads_hint) WARN_UNUSED;

/**
 * Frees the work dispatcher.
 * There must be no {@link BThreadWork}'s with this dispatcher.
 * 
 * @param o the object
 */
void BThreadWorkDispatcher_Free (BThreadWorkDispatcher *o);

/**
 * Determines whether threads are being used for computations, or computations
 * are done in the event loop.
 * 
 * @return 1 if threads are being used, 0 if not
 */
int BThreadWorkDispatcher_UsingThreads (BThreadWorkDispatcher *o);

/**
 * Initializes the work.
 * 
 * @param o the object
 * @param d work dispatcher
 * @param handler_done handler to call when the work is done
 * @param user argument to handler
 * @param work_func function that will do the work, possibly from another thread
 * @param work_func_user argument to work_func
 */
void BThreadWork_Init (BThreadWork *o, BThreadWorkDispatcher *d, BThreadWork_handler_done handler_done, void *user, BThreadWork_work_func work_func, void *work_func_user);

/**
 * Frees the work.
 * After this function returns, the work function will either have fully executed,
 * or not called at all, and never will be.
 * 
 * @param o the object
 */
void BThreadWork_Free (BThreadWork *o);

#endif
