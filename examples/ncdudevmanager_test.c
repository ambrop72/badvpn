/**
 * @file ncdudevmanager_test.c
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

#include <misc/debug.h>
#include <system/BTime.h>
#include <system/BLog.h>
#include <system/BReactor.h>
#include <system/BUnixSignal.h>
#include <process/BProcess.h>
#include <udevmonitor/NCDUdevManager.h>

BReactor reactor;
BUnixSignal usignal;
BProcessManager manager;
NCDUdevManager umanager;
NCDUdevClient client;

static void signal_handler (void *user, int signo);
static void client_handler (void *unused, char *devpath, int have_map, BStringMap map);

int main (int argc, char **argv)
{
    BTime_Init();
    
    BLog_InitStdout();
    
    if (!BReactor_Init(&reactor)) {
        DEBUG("BReactor_Init failed");
        goto fail1;
    }
    
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGHUP);
    if (!BUnixSignal_Init(&usignal, &reactor, set, signal_handler, NULL)) {
        fprintf(stderr, "BUnixSignal_Init failed\n");
        goto fail2;
    }
    
    if (!BProcessManager_Init(&manager, &reactor)) {
        DEBUG("BProcessManager_Init failed");
        goto fail3;
    }
    
    NCDUdevManager_Init(&umanager, &reactor, &manager);
    
    NCDUdevClient_Init(&client, &umanager, NULL, client_handler);
    
    BReactor_Exec(&reactor);
    
    NCDUdevClient_Free(&client);
    
    NCDUdevManager_Free(&umanager);
    
    BProcessManager_Free(&manager);
fail3:
    BUnixSignal_Free(&usignal, 0);
fail2:
    BReactor_Free(&reactor);
fail1:
    BLog_Free();
fail0:
    DebugObjectGlobal_Finish();
    
    return 1;
}

static void signal_handler (void *user, int signo)
{
    if (signo == SIGHUP) {
        fprintf(stderr, "received SIGHUP, restarting client\n");
        
        NCDUdevClient_Free(&client);
        NCDUdevClient_Init(&client, &umanager, NULL, client_handler);
    } else {
        fprintf(stderr, "received %s, exiting\n", (signo == SIGINT ? "SIGINT" : "SIGTERM"));
        
        // exit event loop
        BReactor_Quit(&reactor, 1);
    }
}

void client_handler (void *unused, char *devpath, int have_map, BStringMap map)
{
    printf("event %s\n", devpath);
    
    if (!have_map) {
        printf("  no map\n");
    } else {
        printf("  map:\n");
        
        const char *name = BStringMap_First(&map);
        while (name) {
            printf("    %s=%s\n", name, BStringMap_Get(&map, name));
            name = BStringMap_Next(&map, name);
        }
    }
    
    const BStringMap *cache_map = NCDUdevManager_Query(&umanager, devpath);
    if (!cache_map) {
        printf("  no cache\n");
    } else {
        printf("  cache:\n");
        
        const char *name = BStringMap_First(cache_map);
        while (name) {
            printf("    %s=%s\n", name, BStringMap_Get(cache_map, name));
            name = BStringMap_Next(cache_map, name);
        }
    }
    
    if (have_map) {
        BStringMap_Free(&map);
    }
    free(devpath);
}
