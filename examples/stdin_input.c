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

#include <system/BReactor.c>
#include <system/BSocket.h>
#include <system/BUnixSignal.h>
#include <system/DebugObject.h>
#include <flow/StreamSocketSource.h>

#define BUF_SIZE 64

BReactor reactor;
BSocket pipe_bsock;
BUnixSignal usignal;
FlowErrorDomain errdomain;
StreamSocketSource source;
StreamRecvInterface *source_if;
uint8_t buf[BUF_SIZE + 1];

static void signal_handler (void *user, int signo)
{
    fprintf(stderr, "received %s, exiting\n", (signo == SIGINT ? "SIGINT" : "SIGTERM"));
    
    // exit event loop
    BReactor_Quit(&reactor, 1);
}

static void source_error_handle (void *user, int component, int code)
{
    if (code == 0) {
        fprintf(stderr, "pipe closed\n");
    } else {
        fprintf(stderr, "pipe error\n");
    }
    
    // exit event loop
    BReactor_Quit(&reactor, (code == 0 ? 0 : 1));
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
    
    // init BSocket object backed by the stdin fd
    if (BSocket_InitPipe(&pipe_bsock, &reactor, 0) < 0) {
        fprintf(stderr, "BSocket_InitPipe failed\n");
        goto fail3;
    }
    
    // init error handler
    FlowErrorDomain_Init(&errdomain, source_error_handle, NULL);
    
    // init source (object for reading from a stream BSocket using StreamRecvInterface)
    StreamSocketSource_Init(&source, FlowErrorReporter_Create(&errdomain, 0), &pipe_bsock, BReactor_PendingGroup(&reactor));
    source_if = StreamSocketSource_GetOutput(&source);
    
    // init receive done callback
    StreamRecvInterface_Receiver_Init(source_if, input_handler_done, NULL);
    
    // receive first chunk
    StreamRecvInterface_Receiver_Recv(source_if, buf, BUF_SIZE);
    
    // run event loop
    ret = BReactor_Exec(&reactor);
    
    StreamSocketSource_Free(&source);
    BSocket_Free(&pipe_bsock);
fail3:
    BUnixSignal_Free(&usignal, 0);
fail2:
    BReactor_Free(&reactor);
fail1:
    BLog_Free();
    DebugObjectGlobal_Finish();
    return ret;
}
