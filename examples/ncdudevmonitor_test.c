/**
 * @file ncdudevmonitor_test.c
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

#include <string.h>

#include <system/BTime.h>
#include <base/BLog.h>
#include <system/BReactor.h>
#include <system/BSignal.h>
#include <process/BProcess.h>
#include <udevmonitor/NCDUdevMonitor.h>

BReactor reactor;
BProcessManager manager;
NCDUdevMonitor monitor;

static void signal_handler (void *user);
static void monitor_handler_event (void *unused);
static void monitor_handler_error (void *unused, int is_error);

int main (int argc, char **argv)
{
    int ret = 1;
    
    if (argc < 2 || (strcmp(argv[1], "monitor") && strcmp(argv[1], "info"))) {
        fprintf(stderr, "Usage: %s <monitor/info>\n", argv[0]);
        goto fail0;
    }
    
    int is_info_mode = !strcmp(argv[1], "info");
    
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
    
    if (!NCDUdevMonitor_Init(&monitor, &reactor, &manager, is_info_mode, NULL,
        monitor_handler_event,
        monitor_handler_error
    )) {
        DEBUG("NCDUdevMonitor_Init failed");
        goto fail4;
    }
    
    ret = BReactor_Exec(&reactor);
    
    NCDUdevMonitor_Free(&monitor);
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
    DEBUG("termination requested");
    
    BReactor_Quit(&reactor, 1);
}

void monitor_handler_event (void *unused)
{
    // accept event
    NCDUdevMonitor_Done(&monitor);
    
    if (NCDUdevMonitor_IsReadyEvent(&monitor)) {
        printf("ready\n");
        return;
    }
    
    printf("event\n");
    
    int num_props = NCDUdevMonitor_GetNumProperties(&monitor);
    for (int i = 0; i < num_props; i++) {
        const char *name;
        const char *value;
        NCDUdevMonitor_GetProperty(&monitor, i, &name, &value);
        printf("  %s=%s\n", name, value);
    }
}

void monitor_handler_error (void *unused, int is_error)
{
    if (is_error) {
        DEBUG("monitor error");
    } else {
        DEBUG("monitor finished");
    }
    
    BReactor_Quit(&reactor, (is_error ? 1 : 0));
}
