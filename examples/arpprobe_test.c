/**
 * @file arpprobe_test.c
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <base/BLog.h>
#include <system/BReactor.h>
#include <system/BSignal.h>
#include <system/BTime.h>
#include <system/BNetwork.h>
#include <arpprobe/BArpProbe.h>

BReactor reactor;
BArpProbe arpprobe;

static void signal_handler (void *user);
static void arpprobe_handler (void *unused, int event);

int main (int argc, char **argv)
{
    if (argc <= 0) {
        return 1;
    }
    
    if (argc != 3) {
        printf("Usage: %s <interface> <addr>\n", argv[0]);
        goto fail0;
    }
    
    char *ifname = argv[1];
    uint32_t addr = inet_addr(argv[2]);
    
    BTime_Init();
    
    BLog_InitStdout();
    
    if (!BNetwork_GlobalInit()) {
        DEBUG("BNetwork_GlobalInit failed");
        goto fail1;
    }
    
    if (!BReactor_Init(&reactor)) {
        DEBUG("BReactor_Init failed");
        goto fail1;
    }
    
    if (!BSignal_Init(&reactor, signal_handler, NULL)) {
        DEBUG("BSignal_Init failed");
        goto fail2;
    }
    
    if (!BArpProbe_Init(&arpprobe, ifname, addr, &reactor, NULL, arpprobe_handler)) {
        DEBUG("BArpProbe_Init failed");
        goto fail3;
    }
    
    BReactor_Exec(&reactor);
    
    BArpProbe_Free(&arpprobe);
fail3:
    BSignal_Finish();
fail2:
    BReactor_Free(&reactor);
fail1:
    BLog_Free();
fail0:
    DebugObjectGlobal_Finish();
    
    return 1;
}

void signal_handler (void *user)
{
    DEBUG("termination requested");
    
    BReactor_Quit(&reactor, 0);
}

void arpprobe_handler (void *unused, int event)
{
    switch (event) {
        case BARPPROBE_EVENT_EXIST: {
            printf("ARPPROBE: exist\n");
        } break;
        
        case BARPPROBE_EVENT_NOEXIST: {
            printf("ARPPROBE: noexist\n");
        } break;
        
        case BARPPROBE_EVENT_ERROR: {
            printf("ARPPROBE: error\n");
            
            // exit reactor
            BReactor_Quit(&reactor, 0);
        } break;
        
        default:
            ASSERT(0);
    }
}
