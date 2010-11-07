/**
 * @file ipc_client.c
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
#include <system/BSignal.h>
#include <ipc/BIPC.h>

#define SEND_MTU 100
#define MAX_PACKETS 4096

char *packets[MAX_PACKETS];
int num_packets;
int current_packet;
int waiting;
dead_t dead;
BReactor reactor;
BIPC ipc;
PacketPassInterface recv_if;
PacketPassInterface *send_if;

static void terminate (int ret);
static void signal_handler (void *user);
static void ipc_handler (void *user);
static void send_packets (void);
static void ipc_send_handler_done (void *user);
static void ipc_recv_handler_send (void *user, uint8_t *data, int data_len);

int main (int argc, char **argv)
{
    if (argc <= 0) {
        return 1;
    }
    
    if (argc < 2) {
        printf("Usage: %s <path> [message] ...\n", argv[0]);
        goto fail0;
    }
    
    char *path = argv[1];
    
    num_packets = 0;
    for (int i = 2; i < argc; i++) {
        if (num_packets == MAX_PACKETS) {
            DEBUG("too many packets");
            goto fail0;
        }
        packets[num_packets] = argv[i];
        num_packets++;
    }
    
    current_packet = 0;
    
    waiting = 0;
    
    DEAD_INIT(dead);
    
    BLog_InitStdout();
    
    if (!BReactor_Init(&reactor)) {
        DEBUG("BReactor_Init failed");
        goto fail1;
    }
    
    if (!BSignal_Init()) {
        DEBUG("BSignal_Init failed");
        goto fail2;
    }
    
    BSignal_Capture();
    
    if (!BSignal_SetHandler(&reactor, signal_handler, NULL)) {
        DEBUG("BSignal_SetHandler failed");
        goto fail2;
    }
    
    PacketPassInterface_Init(&recv_if, 0, ipc_recv_handler_send, NULL, BReactor_PendingGroup(&reactor));
    
    if (!BIPC_InitConnect(&ipc, path, SEND_MTU, &recv_if, ipc_handler, NULL, &reactor)) {
        DEBUG("BIPC_InitConnect failed");
        goto fail3;
    }
    
    send_if = BIPC_GetSendInterface(&ipc);
    PacketPassInterface_Sender_Init(send_if, ipc_send_handler_done, NULL);
    
    send_packets();
    
    int ret = BReactor_Exec(&reactor);
    
    BReactor_Free(&reactor);
    
    BLog_Free();
    
    DebugObjectGlobal_Finish();
    
    return ret;
    
fail3:
    PacketPassInterface_Free(&recv_if);
    BSignal_RemoveHandler();
fail2:
    BReactor_Free(&reactor);
fail1:
    BLog_Free();
fail0:
    DebugObjectGlobal_Finish();
    return 1;
}

void terminate (int ret)
{
    BIPC_Free(&ipc);
    
    PacketPassInterface_Free(&recv_if);
    
    BSignal_RemoveHandler();
    
    BReactor_Quit(&reactor, ret);
    
    DEAD_KILL(dead);
}

void signal_handler (void *user)
{
    DEBUG("termination requested");
    
    terminate(1);
}

void ipc_handler (void *user)
{
    DEBUG("IPC broken");
    
    terminate(1);
}

void send_packets (void)
{
    ASSERT(current_packet >= 0)
    ASSERT(current_packet <= num_packets)
    ASSERT(!waiting)
    
    if (current_packet < num_packets) {
        PacketPassInterface_Sender_Send(send_if, (uint8_t *)packets[current_packet], strlen(packets[current_packet]));
    } else {
        terminate(0);
    }
}

void ipc_send_handler_done (void *user)
{
    ASSERT(current_packet >= 0)
    ASSERT(current_packet < num_packets)
    ASSERT(!waiting)
    
    // wait for confirmation
    waiting = 1;
}

void ipc_recv_handler_send (void *user, uint8_t *data, int data_len)
{
    ASSERT(current_packet >= 0)
    ASSERT(current_packet <= num_packets)
    ASSERT(!waiting || current_packet < num_packets)
    
    if (!waiting) {
        DEBUG("not waiting!");
        terminate(1);
        return;
    }
    
    if (data_len != 0) {
        DEBUG("reply not empty!");
        terminate(1);
        return;
    }
    
    current_packet++;
    waiting = 0;
    
    // accept received packet
    PacketPassInterface_Done(&recv_if);
    
    // send more packets
    send_packets();
}
