/**
 * @file BProcess.h
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

#ifndef BADVPN_PROCESS_BPROCESS_H
#define BADVPN_PROCESS_BPROCESS_H

#include <stdint.h>
#include <unistd.h>

#include <misc/debug.h>
#include <misc/debugerror.h>
#include <structure/LinkedList2.h>
#include <system/DebugObject.h>
#include <system/BUnixSignal.h>
#include <system/BPending.h>

typedef struct {
    BReactor *reactor;
    BUnixSignal signal;
    LinkedList2 processes;
    BPending wait_job;
    DebugObject d_obj;
} BProcessManager;

typedef void (*BProcess_handler) (void *user, int normally, uint8_t normally_exit_status);

typedef struct {
    BProcessManager *m;
    BProcess_handler handler;
    void *user;
    pid_t pid;
    LinkedList2Node list_node; // node in BProcessManager.processes
    DebugObject d_obj;
    DebugError d_err;
} BProcess;

int BProcessManager_Init (BProcessManager *o, BReactor *reactor) WARN_UNUSED;
void BProcessManager_Free (BProcessManager *o);

int BProcess_Init (BProcess *o, BProcessManager *m, BProcess_handler handler, void *user, const char *file, char *const argv[], const char *username) WARN_UNUSED;
void BProcess_Free (BProcess *o);
int BProcess_Terminate (BProcess *o);
int BProcess_Kill (BProcess *o);

#endif
