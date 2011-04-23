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

#include <system/BLog.h>

#include <inputprocess/BInputProcess.h>

#include <generated/blog_channel_BInputProcess.h>

static int init_pipe (BInputProcess *o, int pipe_fd);
static void free_pipe (BInputProcess *o);
static void pipe_source_handler_error (BInputProcess *o, int component, int code);
static void process_handler (BInputProcess *o, int normally, uint8_t normally_exit_status);

int init_pipe (BInputProcess *o, int pipe_fd)
{
    // init socket
    if (BSocket_InitPipe(&o->pipe_sock, o->reactor, pipe_fd) < 0) {
        BLog(BLOG_ERROR, "BSocket_InitPipe failed");
        goto fail0;
    }
    
    // init domain
    FlowErrorDomain_Init(&o->pipe_domain, (FlowErrorDomain_handler)pipe_source_handler_error, o);
    
    // init source
    StreamSocketSource_Init(&o->pipe_source, FlowErrorReporter_Create(&o->pipe_domain, 0), &o->pipe_sock, BReactor_PendingGroup(o->reactor));
    
    return 1;
    
fail0:
    return 0;
}

void free_pipe (BInputProcess *o)
{
    // free source
    StreamSocketSource_Free(&o->pipe_source);
    
    // free socket
    BSocket_Free(&o->pipe_sock);
}

void pipe_source_handler_error (BInputProcess *o, int component, int code)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->pipe_fd >= 0)
    
    if (code == STREAMSOCKETSOURCE_ERROR_CLOSED) {
        BLog(BLOG_INFO, "pipe closed");
    } else {
        BLog(BLOG_ERROR, "pipe error");
    }
    
    // free pipe reading
    free_pipe(o);
    
    // close pipe read end
    ASSERT_FORCE(close(o->pipe_fd) == 0)
    
    // forget pipe
    o->pipe_fd = -1;
    
    // call closed handler
    o->handler_closed(o->user, (code != STREAMSOCKETSOURCE_ERROR_CLOSED));
    return;
}

void process_handler (BInputProcess *o, int normally, uint8_t normally_exit_status)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->have_process)
    
    // free process
    BProcess_Free(&o->process);
    
    // set not have process
    o->have_process = 0;
    
    // call terminated handler
    o->handler_terminated(o->user, normally, normally_exit_status);
    return;
}

int BInputProcess_Init (BInputProcess *o, const char *file, char *const argv[], const char *username, BReactor *reactor, BProcessManager *manager, void *user,
                        BInputProcess_handler_terminated handler_terminated,
                        BInputProcess_handler_closed handler_closed)
{
    // init arguments
    o->reactor = reactor;
    o->user = user;
    o->handler_terminated = handler_terminated;
    o->handler_closed = handler_closed;
    
    // create pipe
    int pipefds[2];
    if (pipe(pipefds) < 0) {
        BLog(BLOG_ERROR, "pipe failed");
        goto fail0;
    }
    
    // init pipe reading
    if (!init_pipe(o, pipefds[0])) {
        goto fail1;
    }
    
    // start process
    int fds[] = { pipefds[1], -1 };
    int fds_map[] = { 1 };
    if (!BProcess_InitWithFds(&o->process, manager, (BProcess_handler)process_handler, o, file, argv, username, fds, fds_map)) {
        BLog(BLOG_ERROR, "BProcess_Init failed");
        goto fail2;
    }
    
    // set have process
    o->have_process = 1;
    
    // remember pipe read end
    o->pipe_fd = pipefds[0];
    
    // close pipe write end
    ASSERT_FORCE(close(pipefds[1]) == 0)
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail2:
    free_pipe(o);
fail1:
    ASSERT_FORCE(close(pipefds[0]) == 0)
    ASSERT_FORCE(close(pipefds[1]) == 0)
fail0:
    return 0;
}

void BInputProcess_Free (BInputProcess *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free process
    if (o->have_process) {
        BProcess_Free(&o->process);
    }
    
    if (o->pipe_fd >= 0) {
        // free pipe reading
        free_pipe(o);
        
        // close pipe read end
        ASSERT_FORCE(close(o->pipe_fd) == 0)
    }
}

int BInputProcess_Terminate (BInputProcess *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->have_process)
    
    return BProcess_Terminate(&o->process);
}

int BInputProcess_Kill (BInputProcess *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->have_process)
    
    return BProcess_Kill(&o->process);
}

StreamRecvInterface * BInputProcess_GetInput (BInputProcess *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->pipe_fd >= 0)
    
    return StreamSocketSource_GetOutput(&o->pipe_source);
}
