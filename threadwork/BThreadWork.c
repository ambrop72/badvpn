/**
 * @file BThreadWork.c
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
 */

#include <stdint.h>

#ifdef BADVPN_THREADWORK_USE_PTHREAD
    #include <unistd.h>
    #include <errno.h>
    #include <fcntl.h>
#endif

#include <misc/offset.h>
#include <system/BLog.h>

#include <generated/blog_channel_BThreadWork.h>

#include <threadwork/BThreadWork.h>

#ifdef BADVPN_THREADWORK_USE_PTHREAD

static void * dispatcher_thread (BThreadWorkDispatcher *o)
{
    ASSERT_FORCE(pthread_mutex_lock(&o->mutex) == 0)
    
    while (1) {
        // exit if requested
        if (o->cancel) {
            break;
        }
        
        if (LinkedList2_IsEmpty(&o->pending_list)) {
            // wait for event
            ASSERT_FORCE(pthread_cond_wait(&o->new_cond, &o->mutex) == 0)
            continue;
        }
        
        // grab the work
        BThreadWork *w = UPPER_OBJECT(LinkedList2_GetFirst(&o->pending_list), BThreadWork, list_node);
        ASSERT(w->state == BTHREADWORK_STATE_PENDING)
        LinkedList2_Remove(&o->pending_list, &w->list_node);
        o->running_work = w;
        w->state = BTHREADWORK_STATE_RUNNING;
        
        // do the work
        ASSERT_FORCE(pthread_mutex_unlock(&o->mutex) == 0)
        w->work_func(w->work_func_user);
        ASSERT_FORCE(pthread_mutex_lock(&o->mutex) == 0)
        
        // release the work
        o->running_work = NULL;
        LinkedList2_Append(&o->finished_list, &w->list_node);
        w->state = BTHREADWORK_STATE_FINISHED;
        ASSERT_FORCE(sem_post(&w->finished_sem) == 0)
        
        // write to pipe
        uint8_t b = 0;
        int res = write(o->pipe[1], &b, sizeof(b));
        if (res < 0) {
            int error = errno;
            ASSERT_FORCE(error == EAGAIN || error == EWOULDBLOCK)
        }
    }
    
    ASSERT_FORCE(pthread_mutex_unlock(&o->mutex) == 0)
}

static void dispatch_job (BThreadWorkDispatcher *o)
{
    ASSERT(o->num_threads > 0)
    
    // lock
    ASSERT_FORCE(pthread_mutex_lock(&o->mutex) == 0)
    
    // check for finished job
    if (LinkedList2_IsEmpty(&o->finished_list)) {
        ASSERT_FORCE(pthread_mutex_unlock(&o->mutex) == 0)
        return;
    }
    
    // grab finished job
    BThreadWork *w = UPPER_OBJECT(LinkedList2_GetFirst(&o->finished_list), BThreadWork, list_node);
    ASSERT(w->state == BTHREADWORK_STATE_FINISHED)
    LinkedList2_Remove(&o->finished_list, &w->list_node);
    
    // schedule more
    if (!LinkedList2_IsEmpty(&o->finished_list)) {
        BPending_Set(&o->more_job);
    }
    
    // set state forgotten
    w->state = BTHREADWORK_STATE_FORGOTTEN;
    
    // unlock
    ASSERT_FORCE(pthread_mutex_unlock(&o->mutex) == 0)
    
    // call handler
    w->handler_done(w->user);
    return;
}

static void pipe_fd_handler (BThreadWorkDispatcher *o, int events)
{
    ASSERT(o->num_threads > 0)
    DebugObject_Access(&o->d_obj);
    
    // read data from pipe
    uint8_t b[64];
    int res = read(o->pipe[0], b, sizeof(b));
    if (res < 0) {
        int error = errno;
        ASSERT_FORCE(error == EAGAIN || error == EWOULDBLOCK)
    } else {
        ASSERT(res > 0)
    }
    
    dispatch_job(o);
    return;
}

static void more_job_handler (BThreadWorkDispatcher *o)
{
    ASSERT(o->num_threads > 0)
    DebugObject_Access(&o->d_obj);
    
    dispatch_job(o);
    return;
}

#endif

static void work_job_handler (BThreadWork *o)
{
    ASSERT(o->d->num_threads == 0)
    DebugObject_Access(&o->d_obj);
    
    // do the work
    o->work_func(o->work_func_user);
    
    // call handler
    o->handler_done(o->user);
    return;
}

int BThreadWorkDispatcher_Init (BThreadWorkDispatcher *o, BReactor *reactor, int num_threads_hint)
{
    // init arguments
    o->reactor = reactor;
    
    // set num threads
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    if (num_threads_hint < 0) {
        o->num_threads = 2;
    } else {
        o->num_threads = num_threads_hint;
    }
    #else
    o->num_threads = 0;
    #endif
    
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    
    if (o->num_threads > 0) {
        // init pending list
        LinkedList2_Init(&o->pending_list);
        
        // set no running work
        o->running_work = NULL;
        
        // init finished list
        LinkedList2_Init(&o->finished_list);
        
        // init mutex
        if (pthread_mutex_init(&o->mutex, NULL) != 0) {
            BLog(BLOG_ERROR, "pthread_mutex_init failed");
            goto fail0;
        }
        
        // init condition variable
        if (pthread_cond_init(&o->new_cond, NULL) != 0) {
            BLog(BLOG_ERROR, "pthread_cond_init failed");
            goto fail1;
        }
        
        // init pipe
        if (pipe(o->pipe) < 0) {
            BLog(BLOG_ERROR, "pipe failed");
            goto fail2;
        }
        
        // set read end non-blocking
        if (fcntl(o->pipe[0], F_SETFL, O_NONBLOCK) < 0) {
            BLog(BLOG_ERROR, "fcntl failed");
            goto fail3;
        }
        
        // set write end non-blocking
        if (fcntl(o->pipe[1], F_SETFL, O_NONBLOCK) < 0) {
            BLog(BLOG_ERROR, "fcntl failed");
            goto fail3;
        }
        
        // init BFileDescriptor
        BFileDescriptor_Init(&o->bfd, o->pipe[0], (BFileDescriptor_handler)pipe_fd_handler, o);
        if (!BReactor_AddFileDescriptor(o->reactor, &o->bfd)) {
            BLog(BLOG_ERROR, "BReactor_AddFileDescriptor failed");
            goto fail3;
        }
        BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, BREACTOR_READ);
        
        // init more job
        BPending_Init(&o->more_job, BReactor_PendingGroup(o->reactor), (BPending_handler)more_job_handler, o);
        
        // set not cancelling
        o->cancel = 0;
        
        // init thread
        if (pthread_create(&o->thread, NULL, (void * (*) (void *))dispatcher_thread, o) != 0) {
            BLog(BLOG_ERROR, "pthread_create failed");
            goto fail4;
        }
    }
    
    #endif
    
    DebugObject_Init(&o->d_obj);
    DebugCounter_Init(&o->d_ctr);
    return 1;
    
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
fail4:
    BPending_Free(&o->more_job);
    BReactor_RemoveFileDescriptor(o->reactor, &o->bfd);
fail3:
    ASSERT_FORCE(close(o->pipe[0]) == 0)
    ASSERT_FORCE(close(o->pipe[1]) == 0)
fail2:
    ASSERT_FORCE(pthread_cond_destroy(&o->new_cond) == 0)
fail1:
    ASSERT_FORCE(pthread_mutex_destroy(&o->mutex) == 0)
    #endif
fail0:
    return 0;
}

void BThreadWorkDispatcher_Free (BThreadWorkDispatcher *o)
{
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    if (o->num_threads > 0) {
        ASSERT(LinkedList2_IsEmpty(&o->pending_list))
        ASSERT(!o->running_work)
        ASSERT(LinkedList2_IsEmpty(&o->finished_list))
    }
    #endif
    DebugObject_Free(&o->d_obj);
    DebugCounter_Free(&o->d_ctr);
    
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    
    if (o->num_threads > 0) {
        // post termination request
        ASSERT_FORCE(pthread_mutex_lock(&o->mutex) == 0)
        o->cancel = 1;
        ASSERT_FORCE(pthread_cond_signal(&o->new_cond) == 0)
        ASSERT_FORCE(pthread_mutex_unlock(&o->mutex) == 0)
        
        // wait for thread to exit
        ASSERT_FORCE(pthread_join(o->thread, NULL) == 0)
        
        // free more job
        BPending_Free(&o->more_job);
        
        // free BFileDescriptor
        BReactor_RemoveFileDescriptor(o->reactor, &o->bfd);
        
        // free pipe
        ASSERT_FORCE(close(o->pipe[0]) == 0)
        ASSERT_FORCE(close(o->pipe[1]) == 0)
        
        // free condition variable
        ASSERT_FORCE(pthread_cond_destroy(&o->new_cond) == 0)
        
        // free mutex
        ASSERT_FORCE(pthread_mutex_destroy(&o->mutex) == 0)
    }
    
    #endif
}

void BThreadWork_Init (BThreadWork *o, BThreadWorkDispatcher *d, BThreadWork_handler_done handler_done, void *user, BThreadWork_work_func work_func, void *work_func_user)
{
    DebugObject_Access(&d->d_obj);
    
    // init arguments
    o->d = d;
    o->handler_done = handler_done;
    o->user = user;
    o->work_func = work_func;
    o->work_func_user = work_func_user;
    
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    if (d->num_threads > 0) {
        // set state
        o->state = BTHREADWORK_STATE_PENDING;
        
        // init finished semaphore
        ASSERT_FORCE(sem_init(&o->finished_sem, 0, 0) == 0)
        
        // post work
        ASSERT_FORCE(pthread_mutex_lock(&d->mutex) == 0)
        LinkedList2_Append(&d->pending_list, &o->list_node);
        ASSERT_FORCE(pthread_cond_signal(&d->new_cond) == 0)
        ASSERT_FORCE(pthread_mutex_unlock(&d->mutex) == 0)
    } else {
    #endif
        // schedule job
        BPending_Init(&o->job, BReactor_PendingGroup(d->reactor), (BPending_handler)work_job_handler, o);
        BPending_Set(&o->job);
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    }
    #endif
    
    DebugObject_Init(&o->d_obj);
    DebugCounter_Increment(&d->d_ctr);
}

void BThreadWork_Free (BThreadWork *o)
{
    BThreadWorkDispatcher *d = o->d;
    DebugObject_Free(&o->d_obj);
    DebugCounter_Decrement(&d->d_ctr);
    
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    if (d->num_threads > 0) {
        ASSERT_FORCE(pthread_mutex_lock(&d->mutex) == 0)
        
        switch (o->state) {
            case BTHREADWORK_STATE_PENDING: {
                BLog(BLOG_DEBUG, "remove pending work");
                
                // remove from pending list
                LinkedList2_Remove(&d->pending_list, &o->list_node);
            } break;
            
            case BTHREADWORK_STATE_RUNNING: {
                BLog(BLOG_DEBUG, "remove running work");
                
                // wait for the work to finish running
                ASSERT_FORCE(pthread_mutex_unlock(&d->mutex) == 0)
                ASSERT_FORCE(sem_wait(&o->finished_sem) == 0)
                ASSERT_FORCE(pthread_mutex_lock(&d->mutex) == 0)
                
                ASSERT(o->state == BTHREADWORK_STATE_FINISHED)
                
                // remove from finished list
                LinkedList2_Remove(&d->finished_list, &o->list_node);
            } break;
            
            case BTHREADWORK_STATE_FINISHED: {
                BLog(BLOG_DEBUG, "remove finished work");
                
                // remove from finished list
                LinkedList2_Remove(&d->finished_list, &o->list_node);
            } break;
            
            case BTHREADWORK_STATE_FORGOTTEN: {
                BLog(BLOG_DEBUG, "remove forgotten work");
            } break;
            
            default:
                ASSERT(0);
        }
        
        ASSERT_FORCE(pthread_mutex_unlock(&d->mutex) == 0)
        
        // free finished semaphore
        ASSERT_FORCE(sem_destroy(&o->finished_sem) == 0)
    } else {
    #endif
        BPending_Free(&o->job);
    #ifdef BADVPN_THREADWORK_USE_PTHREAD
    }
    #endif
}
