/**
 * @file bprocess_example.c
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
#include <unistd.h>

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <base/BLog.h>
#include <system/BReactor.h>
#include <system/BUnixSignal.h>
#include <system/BTime.h>
#include <system/BProcess.h>

BReactor reactor;
BUnixSignal unixsignal;
BProcessManager manager;
BProcess process;

static void unixsignal_handler (void *user, int signo);
static void process_handler (void *user, int normally, uint8_t normally_exit_status);

int main (int argc, char **argv)
{
    if (argc <= 0) {
        return 1;
    }
    
    int ret = 1;
    
    if (argc < 2) {
        printf("Usage: %s <program> [argument ...]\n", argv[0]);
        goto fail0;
    }
    
    char *program = argv[1];
    
    // init time
    BTime_Init();
    
    // init logger
    BLog_InitStdout();
    
    // init reactor (event loop)
    if (!BReactor_Init(&reactor)) {
        DEBUG("BReactor_Init failed");
        goto fail1;
    }
    
    // choose signals to catch
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    
    // init BUnixSignal for catching signals
    if (!BUnixSignal_Init(&unixsignal, &reactor, set, unixsignal_handler, NULL)) {
        DEBUG("BUnixSignal_Init failed");
        goto fail2;
    }
    
    // init process manager
    if (!BProcessManager_Init(&manager, &reactor)) {
        DEBUG("BProcessManager_Init failed");
        goto fail3;
    }
    
    char **p_argv = argv + 1;
    
    // map fds 0, 1, 2 in child to fds 0, 1, 2 in parent
    int fds[] = { 0, 1, 2, -1 };
    int fds_map[] = { 0, 1, 2 };
    
    // start child process
    if (!BProcess_InitWithFds(&process, &manager, process_handler, NULL, program, p_argv, NULL, fds, fds_map)) {
        DEBUG("BProcess_Init failed");
        goto fail4;
    }
    
    // enter event loop
    ret = BReactor_Exec(&reactor);
    
    BProcess_Free(&process);
fail4:
    BProcessManager_Free(&manager);
fail3:
    BUnixSignal_Free(&unixsignal, 0);
fail2:
    BReactor_Free(&reactor);
fail1:
    BLog_Free();
fail0:
    DebugObjectGlobal_Finish();
    
    return ret;
}

void unixsignal_handler (void *user, int signo)
{
    DEBUG("received %s, terminating child", (signo == SIGINT ? "SIGINT" : "SIGTERM"));
    
    // send SIGTERM to child
    BProcess_Terminate(&process);
}

void process_handler (void *user, int normally, uint8_t normally_exit_status)
{
    DEBUG("process terminated");
    
    int ret = (normally ? normally_exit_status : 1);
    
    // return from event loop
    BReactor_Quit(&reactor, ret);
}
