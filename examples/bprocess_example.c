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
#include <system/DebugObject.h>
#include <system/BLog.h>
#include <system/BReactor.h>
#include <system/BSignal.h>
#include <system/BTime.h>
#include <system/BProcess.h>

BReactor reactor;
BProcessManager manager;
BProcess process;

static void signal_handler (void *user);
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
    
    BTime_Init();
    
    BLog_InitStdout();
    
    if (!BReactor_Init(&reactor)) {
        DEBUG("BReactor_Init failed");
        goto fail1;
    }
    
    if (!BSignal_Init(&reactor, signal_handler, NULL)) {
        DEBUG("BSignal_Init failed");
        goto fail2;
    }
    
    if (!BProcessManager_Init(&manager, &reactor)) {
        DEBUG("BProcessManager_Init failed");
        goto fail3;
    }
    
    char **p_argv = argv + 1;
    
    if (!BProcess_Init(&process, &manager, process_handler, NULL, program, p_argv, NULL)) {
        DEBUG("BProcess_Init failed");
        goto fail4;
    }
    
    ret = BReactor_Exec(&reactor);
    
    BProcess_Free(&process);
fail4:
    BProcessManager_Free(&manager);
fail3:
    BSignal_Finish();
fail2:
    BReactor_Free(&reactor);
fail1:
    BLog_Free();
fail0:
    DebugObjectGlobal_Finish();
    
    return ret;
}

void signal_handler (void *user)
{
    DEBUG("termination requested, passing to child");
    
    BProcess_Terminate(&process);
}

void process_handler (void *user, int normally, uint8_t normally_exit_status)
{
    DEBUG("process terminated");
    
    int ret = (normally ? normally_exit_status : 1);
    
    BReactor_Quit(&reactor, ret);
}
