/**
 * @file BProcess.c
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

#include <stddef.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>

#include <misc/offset.h>
#include <system/BLog.h>

#include <system/BProcess.h>

#include <generated/blog_channel_BProcess.h>

static void call_handler (BProcess *o, int normally, uint8_t normally_exit_status)
{
    DEBUGERROR(&o->d_err, o->handler(o->user, normally, normally_exit_status))
}

static BProcess * find_process (BProcessManager *o, pid_t pid)
{
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &o->processes);
    LinkedList2Node *node;
    while (node = LinkedList2Iterator_Next(&it)) {
        BProcess *p = UPPER_OBJECT(node, BProcess, list_node);
        if (p->pid == pid) {
            LinkedList2Iterator_Free(&it);
            return p;
        }
    }
    
    return NULL;
}

static void work_signals (BProcessManager *o)
{
    // read exit status with waitpid()
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid <= 0) {
        return;
    }
    
    // schedule next waitpid
    BPending_Set(&o->wait_job);
    
    // find process
    BProcess *p = find_process(o, pid);
    if (!p) {
        BLog(BLOG_DEBUG, "unknown child %p");
    }
    
    if (WIFEXITED(status)) {
        uint8_t exit_status = WEXITSTATUS(status);
        
        BLog(BLOG_INFO, "child %"PRIiMAX" exited with status %"PRIu8, (intmax_t)pid, exit_status);
        
        if (p) {
            call_handler(p, 1, exit_status);
            return;
        }
    }
    else if (WIFSIGNALED(status)) {
        int signo = WTERMSIG(status);
        
        BLog(BLOG_INFO, "child %"PRIiMAX" exited with signal %d", (intmax_t)pid, signo);
        
        if (p) {
            call_handler(p, 0, 0);
            return;
        }
    }
    else {
        BLog(BLOG_ERROR, "unknown wait status type for pid %"PRIiMAX" (%d)", (intmax_t)pid, status);
    }
}

static void wait_job_handler (BProcessManager *o)
{
    DebugObject_Access(&o->d_obj);
    
    work_signals(o);
    return;
}

static void signal_handler (BProcessManager *o, int signo)
{
    ASSERT(signo == SIGCHLD)
    DebugObject_Access(&o->d_obj);
    
    work_signals(o);
    return;
}

int BProcessManager_Init (BProcessManager *o, BReactor *reactor)
{
    // init arguments
    o->reactor = reactor;
    
    // init signal handling
    sigset_t sset;
    ASSERT_FORCE(sigemptyset(&sset) == 0)
    ASSERT_FORCE(sigaddset(&sset, SIGCHLD) == 0)
    if (!BUnixSignal_Init(&o->signal, o->reactor, sset, (BUnixSignal_handler)signal_handler, o)) {
        BLog(BLOG_ERROR, "BUnixSignal_Init failed");
        goto fail0;
    }
    
    // init processes list
    LinkedList2_Init(&o->processes);
    
    // init wait job
    BPending_Init(&o->wait_job, BReactor_PendingGroup(o->reactor), (BPending_handler)wait_job_handler, o);
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail0:
    return 0;
}

void BProcessManager_Free (BProcessManager *o)
{
    ASSERT(LinkedList2_IsEmpty(&o->processes))
    DebugObject_Free(&o->d_obj);
    
    // free wait job
    BPending_Free(&o->wait_job);
    
    // free signal handling
    BUnixSignal_Free(&o->signal, 1);
}

int BProcess_Init (BProcess *o, BProcessManager *m, BProcess_handler handler, void *user, const char *file, char *const argv[], const char *username)
{
    // init arguments
    o->m = m;
    o->handler = handler;
    o->user = user;
    
    // fork
    pid_t pid = fork();
    if (pid < 0) {
        BLog(BLOG_ERROR, "fork failed");
        goto fail0;
    }
    
    if (pid == 0) {
        // this is child
        
        // find maximum file descriptors
        int max_fd = sysconf(_SC_OPEN_MAX);
        if (max_fd < 0) {
            abort();
        }
        
        // close all file descriptors
        for (int i = 0; i < max_fd; i++) {
            close(i);
        }
        
        // assume identity of username, if requested
        if (username) {
            size_t bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
            if (bufsize < 0) {
                bufsize = 16384;
            }
            
            char *buf = malloc(bufsize);
            if (!buf) {
                abort();
            }
            
            struct passwd pwd;
            struct passwd *res;
            getpwnam_r(username, &pwd, buf, bufsize, &res);
            if (!res) {
                abort();
            }
            
            if (initgroups(username, pwd.pw_gid) < 0) {
                abort();
            }
            
            if (setgid(pwd.pw_gid) < 0) {
                abort();
            }
            
            if (setuid(pwd.pw_uid) < 0) {
                abort();
            }
        }
        
        execv(file, argv);
        
        abort();
    }
    
    // remember pid
    o->pid = pid;
    
    // add to processes list
    LinkedList2_Append(&o->m->processes, &o->list_node);
    
    DebugObject_Init(&o->d_obj);
    DebugError_Init(&o->d_err, BReactor_PendingGroup(m->reactor));
    
    return 1;
    
fail0:
    return 0;
}

void BProcess_Free (BProcess *o)
{
    DebugError_Free(&o->d_err);
    DebugObject_Free(&o->d_obj);
    
    // remove from processes list
    LinkedList2_Remove(&o->m->processes, &o->list_node);
}

int BProcess_Terminate (BProcess *o)
{
    DebugObject_Access(&o->d_obj);
    
    ASSERT(o->pid > 0)
    
    if (kill(o->pid, SIGTERM) < 0) {
        BLog(BLOG_ERROR, "kill(%"PRIiMAX", SIGTERM) failed", (intmax_t)o->pid);
        return 0;
    }
    
    return 1;
}

int BProcess_Kill (BProcess *o)
{
    DebugObject_Access(&o->d_obj);
    
    ASSERT(o->pid > 0)
    
    if (kill(o->pid, SIGKILL) < 0) {
        BLog(BLOG_ERROR, "kill(%"PRIiMAX", SIGKILL) failed", (intmax_t)o->pid);
        return 0;
    }
    
    return 1;
}
