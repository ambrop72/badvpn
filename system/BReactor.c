/**
 * @file BReactor.c
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#ifdef BADVPN_USE_WINAPI
#include <windows.h>
#else
#include <limits.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#endif

#include <misc/overflow.h>
#include <misc/debug.h>
#include <misc/offset.h>
#include <system/BLog.h>

#include <system/BReactor.h>

#include <generated/blog_channel_BReactor.h>

#ifdef BADVPN_USE_WINAPI
typedef DWORD btimeout_t;
#define BTIMEOUT_T_MAX ((DWORD)INFINITE - 1)
#define WAITRES_TIMED_OUT(_res) ((_res) == WAIT_TIMEOUT)
#else
typedef int btimeout_t;
#define BTIMEOUT_T_MAX INT_MAX
#define WAITRES_TIMED_OUT(_res) ((_res) == 0)
#endif

static void dispatch_jobs (BReactor *bsys)
{
    while (!bsys->exiting && BPendingGroup_HasJobs(&bsys->pending_jobs)) {
        BPendingGroup_ExecuteJob(&bsys->pending_jobs);
    }
}

static int timer_comparator (void *user, btime_t *val1, btime_t *val2)
{
    if (*val1 < *val2) {
        return -1;
    }
    if (*val1 > *val2) {
        return 1;
    }
    return 0;
}

static int move_expired_timers (BReactor *bsys, btime_t now)
{
    int moved = 0;
    
    // move timed out timers to the expired list
    BHeapNode *heap_node;
    while (heap_node = BHeap_GetFirst(&bsys->timers_heap)) {
        BTimer *timer = UPPER_OBJECT(heap_node, BTimer, heap_node);
        ASSERT(timer->active)
        
        // if it's in the future, stop
        if (timer->absTime > now) {
            break;
        }
        moved = 1;
        
        // remove from running timers heap
        BHeap_Remove(&bsys->timers_heap, &timer->heap_node);
        
        // add to expired timers list
        LinkedList2_Append(&bsys->timers_expired_list, &timer->list_node);

        // set expired
        timer->expired = 1;
    }

    return moved;
}

static void move_first_timers (BReactor *bsys)
{
    // get the time of the first timer
    BHeapNode *heap_node = BHeap_GetFirst(&bsys->timers_heap);
    ASSERT(heap_node)
    BTimer *first_timer = UPPER_OBJECT(heap_node, BTimer, heap_node);
    ASSERT(first_timer->active)
    btime_t first_time = first_timer->absTime;
    
    // remove from running timers heap
    BHeap_Remove(&bsys->timers_heap, &first_timer->heap_node);
    
    // add to expired timers list
    LinkedList2_Append(&bsys->timers_expired_list, &first_timer->list_node);
    
    // set expired
    first_timer->expired = 1;
    
    // also move other timers with the same timeout
    while (heap_node = BHeap_GetFirst(&bsys->timers_heap)) {
        BTimer *timer = UPPER_OBJECT(heap_node, BTimer, heap_node);
        ASSERT(timer->active)
        ASSERT(timer->absTime >= first_time)
        
        // if it's in the future, stop
        if (timer->absTime > first_time) {
            break;
        }
        
        // remove from running timers heap
        BHeap_Remove(&bsys->timers_heap, &timer->heap_node);
        
        // add to expired timers list
        LinkedList2_Append(&bsys->timers_expired_list, &timer->list_node);
        
        // set expired
        timer->expired = 1;
    }
}

static void dispatch_timers (BReactor *bsys)
{
    // call event hendlers for expired timers
    // Handler functions are free to remove any timer, and if a pending
    // expired timer is removed, it will not be reported.
    
    LinkedList2Node *list_node;
    while (!bsys->exiting && (list_node = LinkedList2_GetFirst(&bsys->timers_expired_list))) {
        BTimer *timer = UPPER_OBJECT(list_node, BTimer, list_node);
        ASSERT(timer->active)
        ASSERT(timer->expired)
        
        // remove from expired list
        LinkedList2_Remove(&bsys->timers_expired_list, &timer->list_node);
        
        // set inactive
        timer->active = 0;
        
        // call handler
        BLog(BLOG_DEBUG, "Dispatching timer");
        timer->handler(timer->handler_pointer);
        
        // dispatch jobs
        dispatch_jobs(bsys);
    }
}

#ifdef BADVPN_USE_WINAPI

static void dispatch_io (BReactor *bsys)
{
    if (!bsys->exiting && bsys->returned_object) {
        BHandle *bh = bsys->returned_object;
        bsys->returned_object = NULL;
        ASSERT(bh->active)
        ASSERT(bh->position >= 0 && bh->position < bsys->enabled_num)
        ASSERT(bh == bsys->enabled_objects[bh->position])
        ASSERT(bh->h == bsys->enabled_handles[bh->position])
        
        // call handler
        BLog(BLOG_DEBUG, "Dispatching handle");
        bh->handler(bh->user);
        
        // dispatch jobs
        dispatch_jobs(bsys);
    }
}

#else

static void set_fd_pointers (BReactor *bsys)
{
    // Write pointers to our entry pointers into file descriptors.
    // If a handler function frees some other file descriptor, the
    // free routine will set our pointer to NULL so we don't dispatch it.
    for (int i = 0; i < bsys->epoll_results_num; i++) {
        struct epoll_event *event = &bsys->epoll_results[i];
        ASSERT(event->data.ptr)
        BFileDescriptor *bfd = (BFileDescriptor *)event->data.ptr;
        ASSERT(bfd->active)
        ASSERT(!bfd->epoll_returned_ptr)
        bfd->epoll_returned_ptr = (BFileDescriptor **)&event->data.ptr;
    }
}

static void dispatch_io (BReactor *bsys)
{
    while (!bsys->exiting && bsys->epoll_results_pos < bsys->epoll_results_num) {
        // grab event
        struct epoll_event *event = &bsys->epoll_results[bsys->epoll_results_pos];
        bsys->epoll_results_pos++;
        
        // check if the BFileDescriptor was removed
        if (!event->data.ptr) {
            continue;
        }
        
        // get BFileDescriptor
        BFileDescriptor *bfd = (BFileDescriptor *)event->data.ptr;
        ASSERT(bfd->active)
        ASSERT(bfd->epoll_returned_ptr == (BFileDescriptor **)&event->data.ptr)
        
        // zero pointer to the epoll entry
        bfd->epoll_returned_ptr = NULL;
        
        // calculate events to report
        int events = 0;
        if (bfd->waitEvents&BREACTOR_READ) {
            if ((event->events&EPOLLIN) || (event->events&EPOLLERR) || (event->events&EPOLLHUP)) {
                events |= BREACTOR_READ;
            }
        }
        if (bfd->waitEvents&BREACTOR_WRITE) {
            if ((event->events&EPOLLOUT) || (event->events&EPOLLERR) || (event->events&EPOLLHUP)) {
                events |= BREACTOR_WRITE;
            }
        }
        
        // call handler
        BLog(BLOG_DEBUG, "Dispatching file descriptor");
        bfd->handler(bfd->user, events);
        
        // dispatch jobs
        dispatch_jobs(bsys);
    }
}

#endif

static void wait_for_events (BReactor *bsys)
{
    // must have processed all pending events
    ASSERT(!BPendingGroup_HasJobs(&bsys->pending_jobs))
    ASSERT(LinkedList2_IsEmpty(&bsys->timers_expired_list))
    #ifdef BADVPN_USE_WINAPI
    ASSERT(!bsys->returned_object)
    #else
    ASSERT(bsys->epoll_results_pos == bsys->epoll_results_num)
    #endif

    // clean up epoll results
    #ifndef BADVPN_USE_WINAPI
    bsys->epoll_results_num = 0;
    bsys->epoll_results_pos = 0;
    #endif
    
    // timeout vars
    int have_timeout = 0;
    btime_t timeout_abs;
    btime_t now;
    
    // compute timeout
    BHeapNode *first_node;
    if (first_node = BHeap_GetFirst(&bsys->timers_heap)) {
        // get current time
        now = btime_gettime();
        
        // if some timers have already timed out, return them immediately
        if (move_expired_timers(bsys, now)) {
            BLog(BLOG_DEBUG, "Got already expired timers");
            return;
        }
        
        // timeout is first timer, remember absolute time
        BTimer *first_timer = UPPER_OBJECT(first_node, BTimer, heap_node);
        have_timeout = 1;
        timeout_abs = first_timer->absTime;
    }
    
    int timed_out;
    
    #ifdef BADVPN_USE_WINAPI
    int handle_index;
    #else
    int epoll_num_results;
    #endif
    
    // wait until the timeout is reached or the file descriptor / handle in ready
    while (1) {
        // compute timeout
        btimeout_t timeout_arg;
        btime_t timeout_rel;
        if (have_timeout) {
            timeout_rel = timeout_abs - now;
            if (timeout_rel > BTIMEOUT_T_MAX) {
                timeout_arg = BTIMEOUT_T_MAX;
            } else {
                timeout_arg = timeout_rel;
            }
        }
        
        // perform wait
        
        #ifdef BADVPN_USE_WINAPI
        
        BLog(BLOG_DEBUG, "Calling WaitForMultipleObjects on %d handles", bsys->enabled_num);
        
        DWORD waitres = WaitForMultipleObjects(bsys->enabled_num, bsys->enabled_handles, FALSE, (have_timeout ? timeout_arg : INFINITE));
        ASSERT_FORCE(waitres != WAIT_FAILED)
        ASSERT_FORCE(!(waitres == WAIT_TIMEOUT) || have_timeout)
        ASSERT_FORCE(!(waitres != WAIT_TIMEOUT) || (waitres >= WAIT_OBJECT_0 && waitres < WAIT_OBJECT_0 + bsys->enabled_num))
        
        #else
        
        BLog(BLOG_DEBUG, "Calling epoll_wait");
        
        int waitres = epoll_wait(bsys->efd, bsys->epoll_results, BSYSTEM_MAX_RESULTS, (have_timeout ? timeout_arg : -1));
        if (waitres < 0) {
            int error = errno;
            if (error == EINTR) {
                BLog(BLOG_DEBUG, "epoll_wait interrupted");
                goto try_again;
            }
            perror("epoll_wait");
            ASSERT_FORCE(0)
        }
        
        ASSERT_FORCE(!(waitres == 0) || have_timeout)
        ASSERT_FORCE(waitres <= BSYSTEM_MAX_RESULTS)
        
        #endif
        
        if (!WAITRES_TIMED_OUT(waitres) || timeout_rel <= BTIMEOUT_T_MAX) {
            timed_out = WAITRES_TIMED_OUT(waitres);
            if (!timed_out) {
                #ifdef BADVPN_USE_WINAPI
                handle_index = waitres - WAIT_OBJECT_0;
                #else
                epoll_num_results = waitres;
                #endif
            }
            break;
        }
        
    try_again:
        if (have_timeout) {
            // get current time
            now = btime_gettime();
            // check if we already reached the time we're waiting for
            if (now >= timeout_abs) {
                timed_out = 1;
                break;
            }
        }
    }
    
    if (timed_out) {
        // timed out, expire first timers
        BLog(BLOG_DEBUG, "Wait timed out");
        move_first_timers(bsys);
    } else {
        #ifdef BADVPN_USE_WINAPI
        // user's handle got signalled
        BLog(BLOG_DEBUG, "Wait returned handle %d", handle_index);
        bsys->returned_object = bsys->enabled_objects[handle_index];
        #else
        // setup returned file descriptors list
        BLog(BLOG_DEBUG, "Wait returned %d file descriptors", epoll_num_results);
        bsys->epoll_results_num = epoll_num_results;
        set_fd_pointers(bsys);
        #endif
    }
}

#ifdef BADVPN_USE_WINAPI

void BHandle_Init (BHandle *bh, HANDLE handle, BHandle_handler handler, void *user)
{
    bh->h = handle;
    bh->handler = handler;
    bh->user = user;
    bh->active = 0;
}

#else

void BFileDescriptor_Init (BFileDescriptor *bs, int fd, BFileDescriptor_handler handler, void *user)
{
    bs->fd = fd;
    bs->handler = handler;
    bs->user = user;
    bs->active = 0;
}

#endif

void BTimer_Init (BTimer *bt, btime_t msTime, BTimer_handler handler, void *handler_pointer)
{
    bt->msTime = msTime;
    bt->handler = handler;
    bt->handler_pointer = handler_pointer;

    bt->active = 0;
}

int BTimer_IsRunning (BTimer *bt)
{
    ASSERT(bt->active == 0 || bt->active == 1)
    
    return bt->active;
}

int BReactor_Init (BReactor *bsys)
{
    BLog(BLOG_DEBUG, "Reactor initializing");
    
    bsys->exiting = 0;
    
    // init jobs
    BPendingGroup_Init(&bsys->pending_jobs);
    
    // init timers
    BHeap_Init(&bsys->timers_heap, OFFSET_DIFF(BTimer, absTime, heap_node), (BHeap_comparator)timer_comparator, NULL);
    LinkedList2_Init(&bsys->timers_expired_list);
    
    #ifdef BADVPN_USE_WINAPI
    
    bsys->num_handles = 0;
    bsys->enabled_num = 0;
    bsys->returned_object = NULL;
    
    #else
    
    // create epoll fd
    if ((bsys->efd = epoll_create(10)) < 0) {
        BLog(BLOG_ERROR, "epoll_create failed");
        goto fail0;
    }
    
    // init results array
    bsys->epoll_results_num = 0;
    bsys->epoll_results_pos = 0;
    
    DebugCounter_Init(&bsys->d_fds_counter);
    
    #endif
    
    // init debug object
    DebugObject_Init(&bsys->d_obj);
    
    return 1;
    
fail0:
    BPendingGroup_Free(&bsys->pending_jobs);
    BLog(BLOG_ERROR, "Reactor failed to initialize");
    return 0;
}

void BReactor_Free (BReactor *bsys)
{
    // {pending group has no BPending objects}
    ASSERT(!BPendingGroup_HasJobs(&bsys->pending_jobs))
    ASSERT(!BHeap_GetFirst(&bsys->timers_heap))
    ASSERT(LinkedList2_IsEmpty(&bsys->timers_expired_list))
    DebugObject_Free(&bsys->d_obj);
    #ifdef BADVPN_USE_WINAPI
    ASSERT(bsys->num_handles == 0)
    #else
    DebugCounter_Free(&bsys->d_fds_counter);
    #endif
    
    BLog(BLOG_DEBUG, "Reactor freeing");
    
    #ifndef BADVPN_USE_WINAPI
    
    // close epoll fd
    ASSERT_FORCE(close(bsys->efd) == 0)
    
    #endif
    
    // free jobs
    BPendingGroup_Free(&bsys->pending_jobs);
}

int BReactor_Exec (BReactor *bsys)
{
    BLog(BLOG_DEBUG, "Entering event loop");

    while (1) {
        dispatch_jobs(bsys);
        dispatch_timers(bsys);
        dispatch_io(bsys);
        
        if (bsys->exiting) {
            break;
        }
        
        wait_for_events(bsys);
    }

    BLog(BLOG_DEBUG, "Exiting event loop, exit code %d", bsys->exit_code);

    return bsys->exit_code;
}

void BReactor_Quit (BReactor *bsys, int code)
{
    bsys->exiting = 1;
    bsys->exit_code = code;
}

void BReactor_SetTimer (BReactor *bsys, BTimer *bt)
{
    btime_t now = btime_gettime();
    
    // handle overflow
    int overflows = add_int64_overflows(now, bt->msTime);
    btime_t absTime;
    if (overflows != 0) {
        if (overflows > 0) {
            absTime = INT64_MAX;
        } else {
            absTime = INT64_MIN;
        }
    } else {
        absTime = now + bt->msTime;
    }
    
    BReactor_SetTimerAbsolute(bsys, bt, absTime);
}

void BReactor_SetTimerAbsolute (BReactor *bsys, BTimer *bt, btime_t time)
{
    // unlink it if it's already in the list
    BReactor_RemoveTimer(bsys, bt);

    // initialize timer
    bt->active = 1;
    bt->expired = 0;
    bt->absTime = time;

    // insert to running timers heap
    BHeap_Insert(&bsys->timers_heap, &bt->heap_node);
}

void BReactor_RemoveTimer (BReactor *bsys, BTimer *bt)
{
    if (!bt->active) {
        return;
    }

    if (bt->expired) {
        // remove from expired list
        LinkedList2_Remove(&bsys->timers_expired_list, &bt->list_node);
    } else {
        // remove from running heap
        BHeap_Remove(&bsys->timers_heap, &bt->heap_node);
    }

    // set inactive
    bt->active = 0;
}

BPendingGroup * BReactor_PendingGroup (BReactor *bsys)
{
    return &bsys->pending_jobs;
}

#ifdef BADVPN_USE_WINAPI

int BReactor_AddHandle (BReactor *bsys, BHandle *bh)
{
    ASSERT(!bh->active)

    if (bsys->num_handles >= BSYSTEM_MAX_HANDLES) {
        return 0;
    }

    bh->active = 1;
    bh->position = -1;

    bsys->num_handles++;

    return 1;
}

void BReactor_RemoveHandle (BReactor *bsys, BHandle *bh)
{
    ASSERT(bh->active)

    if (bh->position >= 0) {
        BReactor_DisableHandle(bsys, bh);
    }

    bh->active = 0;

    ASSERT(bsys->num_handles > 0)
    bsys->num_handles--;
}

void BReactor_EnableHandle (BReactor *bsys, BHandle *bh)
{
    ASSERT(bh->active)
    ASSERT(bh->position == -1)

    ASSERT(bsys->enabled_num < BSYSTEM_MAX_HANDLES)
    bsys->enabled_handles[bsys->enabled_num] = bh->h;
    bsys->enabled_objects[bsys->enabled_num] = bh;
    bh->position = bsys->enabled_num;
    bsys->enabled_num++;
}

void BReactor_DisableHandle (BReactor *bsys, BHandle *bh)
{
    ASSERT(bh->active)
    ASSERT(bh->position >= 0)

    ASSERT(bh->position < bsys->enabled_num)
    ASSERT(bh == bsys->enabled_objects[bh->position])
    ASSERT(bh->h == bsys->enabled_handles[bh->position])

    // if there are more handles after this one, move the last
    // one into its position
    if (bh->position < bsys->enabled_num - 1) {
        int move_position = bsys->enabled_num - 1;
        BHandle *move_handle = bsys->enabled_objects[move_position];

        ASSERT(move_handle->active)
        ASSERT(move_handle->position == move_position)
        ASSERT(move_handle->h == bsys->enabled_handles[move_position])

        bsys->enabled_handles[bh->position] = move_handle->h;
        bsys->enabled_objects[bh->position] = move_handle;
        move_handle->position = bh->position;
    }

    bh->position = -1;
    bsys->enabled_num--;

    // make sure the handler will not be called
    if (bsys->returned_object == bh) {
        bsys->returned_object = NULL;
    }
}

#else

int BReactor_AddFileDescriptor (BReactor *bsys, BFileDescriptor *bs)
{
    ASSERT(!bs->active)
    
    // add epoll entry
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = 0;
    event.data.ptr = bs;
    if (epoll_ctl(bsys->efd, EPOLL_CTL_ADD, bs->fd, &event) < 0) {
        int error = errno;
        BLog(BLOG_ERROR, "epoll_ctl failed: %d", error);
        return 0;
    }
    
    bs->active = 1;
    bs->waitEvents = 0;
    bs->epoll_returned_ptr = NULL;
    
    DebugCounter_Increment(&bsys->d_fds_counter);
    
    return 1;
}

void BReactor_RemoveFileDescriptor (BReactor *bsys, BFileDescriptor *bs)
{
    ASSERT(bs->active)

    bs->active = 0;

    // delete epoll entry
    ASSERT_FORCE(epoll_ctl(bsys->efd, EPOLL_CTL_DEL, bs->fd, NULL) == 0)
    
    // The user can now free the file descriptor object, however the file descriptor
    // can still be in the list of returned events. To prevent the event dispatcher
    // from crashing, zero its pointer to the file descriptor.
    if (bs->epoll_returned_ptr) {
        *bs->epoll_returned_ptr = NULL;
    }
    
    DebugCounter_Decrement(&bsys->d_fds_counter);
}

void BReactor_SetFileDescriptorEvents (BReactor *bsys, BFileDescriptor *bs, int events)
{
    ASSERT(bs->active)
    ASSERT(!(events&~(BREACTOR_READ|BREACTOR_WRITE)))
    
    if (bs->waitEvents == events) {
        return;
    }
    
    // update events
    bs->waitEvents = events;
    
    // calculate epoll events
    int eevents = 0;
    if (bs->waitEvents&BREACTOR_READ) {
        eevents |= EPOLLIN;
    }
    if (bs->waitEvents&BREACTOR_WRITE) {
        eevents |= EPOLLOUT;
    }
    
    // update epoll entry
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = eevents;
    event.data.ptr = bs;
    ASSERT_FORCE(epoll_ctl(bsys->efd, EPOLL_CTL_MOD, bs->fd, &event) == 0)
}

#endif
