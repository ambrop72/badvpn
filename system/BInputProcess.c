/**
 * @file BInputProcess.c
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

#include <unistd.h>

#include <base/BLog.h>

#include "BInputProcess.h"

#include <generated/blog_channel_BInputProcess.h>

static void connection_handler (BInputProcess *o, int event);
static void process_handler (BInputProcess *o, int normally, uint8_t normally_exit_status);

void connection_handler (BInputProcess *o, int event)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->pipe_fd >= 0)
    
    if (event == BCONNECTION_EVENT_RECVCLOSED) {
        BLog(BLOG_INFO, "pipe closed");
    } else {
        BLog(BLOG_ERROR, "pipe error");
    }
    
    // free pipe connection read interface
    BConnection_RecvAsync_Free(&o->pipe_con);
    
    // free pipe connection
    BConnection_Free(&o->pipe_con);
    
    // close pipe read end
    ASSERT_FORCE(close(o->pipe_fd) == 0)
    
    // forget pipe
    o->pipe_fd = -1;
    
    // call closed handler
    o->handler_closed(o->user, (event != BCONNECTION_EVENT_RECVCLOSED));
    return;
}

void process_handler (BInputProcess *o, int normally, uint8_t normally_exit_status)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->started)
    ASSERT(o->have_process)
    
    // free process
    BProcess_Free(&o->process);
    
    // set not have process
    o->have_process = 0;
    
    // call terminated handler
    o->handler_terminated(o->user, normally, normally_exit_status);
    return;
}

int BInputProcess_Init (BInputProcess *o, BReactor *reactor, BProcessManager *manager, void *user,
                        BInputProcess_handler_terminated handler_terminated,
                        BInputProcess_handler_closed handler_closed)
{
    // init arguments
    o->reactor = reactor;
    o->manager = manager;
    o->user = user;
    o->handler_terminated = handler_terminated;
    o->handler_closed = handler_closed;
    
    // create pipe
    int pipefds[2];
    if (pipe(pipefds) < 0) {
        BLog(BLOG_ERROR, "pipe failed");
        goto fail0;
    }
    
    // init pipe connection
    if (!BConnection_Init(&o->pipe_con, BCONNECTION_SOURCE_PIPE(pipefds[0]), o->reactor, o, (BConnection_handler)connection_handler)) {
        BLog(BLOG_ERROR, "BConnection_Init failed");
        goto fail1;
    }
    
    // init pipe connection read interface
    BConnection_RecvAsync_Init(&o->pipe_con);
    
    // remember pipe fds
    o->pipe_fd = pipefds[0];
    o->pipe_write_fd = pipefds[1];
    
    // set not started
    o->started = 0;
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail1:
    ASSERT_FORCE(close(pipefds[0]) == 0)
    ASSERT_FORCE(close(pipefds[1]) == 0)
fail0:
    return 0;
}

void BInputProcess_Free (BInputProcess *o)
{
    DebugObject_Free(&o->d_obj);
    
    if (!o->started) {
        // close pipe write end
        ASSERT_FORCE(close(o->pipe_write_fd) == 0)
    } else {
        // free process
        if (o->have_process) {
            BProcess_Free(&o->process);
        }
    }
    
    if (o->pipe_fd >= 0) {
        // free pipe connection read interface
        BConnection_RecvAsync_Free(&o->pipe_con);
        
        // free pipe connection
        BConnection_Free(&o->pipe_con);
        
        // close pipe read end
        ASSERT_FORCE(close(o->pipe_fd) == 0)
    }
}

int BInputProcess_Start (BInputProcess *o, const char *file, char *const argv[], const char *username)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(!o->started)
    
    // start process
    int fds[] = { o->pipe_write_fd, -1 };
    int fds_map[] = { 1 };
    if (!BProcess_InitWithFds(&o->process, o->manager, (BProcess_handler)process_handler, o, file, argv, username, fds, fds_map)) {
        BLog(BLOG_ERROR, "BProcess_Init failed");
        goto fail0;
    }
    
    // close pipe write end
    ASSERT_FORCE(close(o->pipe_write_fd) == 0)
    
    // set started
    o->started = 1;
    
    // set have process
    o->have_process = 1;
    
    return 1;
    
fail0:
    return 0;
}

int BInputProcess_Terminate (BInputProcess *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->started)
    ASSERT(o->have_process)
    
    return BProcess_Terminate(&o->process);
}

int BInputProcess_Kill (BInputProcess *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->started)
    ASSERT(o->have_process)
    
    return BProcess_Kill(&o->process);
}

StreamRecvInterface * BInputProcess_GetInput (BInputProcess *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->pipe_fd >= 0)
    
    return BConnection_RecvAsync_GetIf(&o->pipe_con);
}
