/**
 * @file stdin_input.c
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
 * 
 * @section DESCRIPTION
 * 
 * Example program which reads stdin and waits for SIGINT and SIGTERM.
 */

#include <stdio.h>
#include <stddef.h>

#include <base/DebugObject.h>
#include <system/BReactor.c>
#include <system/BNetwork.h>
#include <system/BConnection.h>
#include <system/BUnixSignal.h>

#define BUF_SIZE 64

BReactor reactor;
BConnection pipe_con;
BUnixSignal usignal;
StreamRecvInterface *source_if;
uint8_t buf[BUF_SIZE + 1];

static void signal_handler (void *user, int signo)
{
    fprintf(stderr, "received %s, exiting\n", (signo == SIGINT ? "SIGINT" : "SIGTERM"));
    
    // exit event loop
    BReactor_Quit(&reactor, 1);
}

static void connection_handler (void *user, int event)
{
    if (event == BCONNECTION_EVENT_RECVCLOSED) {
        fprintf(stderr, "pipe closed\n");
    } else {
        fprintf(stderr, "pipe error\n");
    }
    
    // exit event loop
    BReactor_Quit(&reactor, (event == BCONNECTION_EVENT_RECVCLOSED ? 0 : 1));
}

static void input_handler_done (void *user, int data_len)
{
    // receive next chunk
    StreamRecvInterface_Receiver_Recv(source_if, buf, BUF_SIZE);
    
    // print this chunk
    buf[data_len] = '\0';
    printf("Received: '%s'\n", buf);
}

int main ()
{
    int ret = 1;
    
    BLog_InitStdout();
    
    // init network
    if (!BNetwork_GlobalInit()) {
        fprintf(stderr, "BNetwork_GlobalInit failed\n");
        goto fail1;
    }
    
    // init reactor (event loop)
    if (!BReactor_Init(&reactor)) {
        fprintf(stderr, "BReactor_Init failed\n");
        goto fail1;
    }
    
    // init signal handling
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    if (!BUnixSignal_Init(&usignal, &reactor, set, signal_handler, NULL)) {
        fprintf(stderr, "BUnixSignal_Init failed\n");
        goto fail2;
    }
    
    // init BConnection object backed by the stdin fd
    if (!BConnection_Init(&pipe_con, BCONNECTION_SOURCE_PIPE(0), &reactor, NULL, connection_handler)) {
        fprintf(stderr, "BConnection_Init failed\n");
        goto fail3;
    }
    
    // init connection receive interface
    BConnection_RecvAsync_Init(&pipe_con);
    source_if = BConnection_RecvAsync_GetIf(&pipe_con);
    
    // init receive done callback
    StreamRecvInterface_Receiver_Init(source_if, input_handler_done, NULL);
    
    // receive first chunk
    StreamRecvInterface_Receiver_Recv(source_if, buf, BUF_SIZE);
    
    // run event loop
    ret = BReactor_Exec(&reactor);
    
    BConnection_RecvAsync_Free(&pipe_con);
    BConnection_Free(&pipe_con);
fail3:
    BUnixSignal_Free(&usignal, 0);
fail2:
    BReactor_Free(&reactor);
fail1:
    BLog_Free();
    DebugObjectGlobal_Finish();
    return ret;
}
