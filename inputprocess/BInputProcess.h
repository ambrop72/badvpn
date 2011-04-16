/**
 * @file BInputProcess.h
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

#ifndef BADVPN_INPUTPROCESS_BINPUTPROCESS_H
#define BADVPN_INPUTPROCESS_BINPUTPROCESS_H

#include <misc/debug.h>
#include <misc/debugerror.h>
#include <system/DebugObject.h>
#include <system/BSocket.h>
#include <process/BProcess.h>
#include <flow/StreamSocketSource.h>

typedef void (*BInputProcess_handler_terminated) (void *user, int normally, uint8_t normally_exit_status);
typedef void (*BInputProcess_handler_closed) (void *user, int is_error);

typedef struct {
    BReactor *reactor;
    void *user;
    BInputProcess_handler_terminated handler_terminated;
    BInputProcess_handler_closed handler_closed;
    BProcess process;
    int pipe_fd;
    BSocket pipe_sock;
    FlowErrorDomain pipe_domain;
    StreamSocketSource pipe_source;
    DebugObject d_obj;
    DebugError d_err;
} BInputProcess;

int BInputProcess_Init (BInputProcess *o, const char *file, char *const argv[], const char *username, BReactor *reactor, BProcessManager *manager, void *user,
                        BInputProcess_handler_terminated handler_terminated,
                        BInputProcess_handler_closed handler_closed) WARN_UNUSED;
void BInputProcess_Free (BInputProcess *o);
int BInputProcess_Terminate (BInputProcess *o);
int BInputProcess_Kill (BInputProcess *o);
StreamRecvInterface * BInputProcess_GetInput (BInputProcess *o);

#endif
