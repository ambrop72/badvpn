/**
 * @file dhcpclient_test.c
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

#include <misc/debug.h>
#include <system/DebugObject.h>
#include <system/BLog.h>
#include <system/BReactor.h>
#include <system/BSignal.h>
#include <system/BTime.h>
#include <dhcpclient/BDHCPClient.h>

BReactor reactor;
BDHCPClient dhcp;

static void signal_handler (void *user);
static void dhcp_handler (void *unused, int event);

int main (int argc, char **argv)
{
    if (argc <= 0) {
        return 1;
    }
    
    if (argc != 2) {
        printf("Usage: %s <interface>\n", argv[0]);
        goto fail0;
    }
    
    char *ifname = argv[1];
    
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
    
    if (!BDHCPClient_Init(&dhcp, ifname, &reactor, dhcp_handler, NULL)) {
        DEBUG("BDHCPClient_Init failed");
        goto fail3;
    }
    
    BReactor_Exec(&reactor);
    
    BDHCPClient_Free(&dhcp);
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

void dhcp_handler (void *unused, int event)
{
    switch (event) {
        case BDHCPCLIENT_EVENT_UP: {
            printf("DHCP: up");
            
            uint32_t ip;
            uint8_t *ipb = (void *)&ip;
            
            BDHCPClient_GetClientIP(&dhcp, &ip);
            printf(" IP=%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8, ipb[0], ipb[1], ipb[2], ipb[3]);
            
            BDHCPClient_GetClientMask(&dhcp, &ip);
            printf(" Mask=%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8, ipb[0], ipb[1], ipb[2], ipb[3]);
            
            if (BDHCPClient_GetRouter(&dhcp, &ip)) {
                printf(" Router=%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8, ipb[0], ipb[1], ipb[2], ipb[3]);
            }
            
            uint32_t dns[BDHCPCLIENT_MAX_DOMAIN_NAME_SERVERS];
            int num = BDHCPClient_GetDNS(&dhcp, dns, BDHCPCLIENT_MAX_DOMAIN_NAME_SERVERS);
            for (int i = 0; i < num; i++) {
                ip=dns[i];
                printf(" DNS=%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8, ipb[0], ipb[1], ipb[2], ipb[3]);
            }
            
            printf("\n");
        } break;
        
        case BDHCPCLIENT_EVENT_DOWN: {
            printf("DHCP: down\n");
        } break;
        
        default:
            ASSERT(0);
    }
}
