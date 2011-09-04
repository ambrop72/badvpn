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

#ifndef BADVPN_BPROCESS_H
#define BADVPN_BPROCESS_H

#include <stdint.h>
#include <unistd.h>

#include <misc/debug.h>
#include <misc/debugerror.h>
#include <structure/LinkedList2.h>
#include <base/DebugObject.h>
#include <system/BUnixSignal.h>
#include <base/BPending.h>

/**
 * Manages child processes.
 * There may be at most one process manager at any given time. This restriction is not
 * enforced, however.
 */
typedef struct {
    BReactor *reactor;
    BUnixSignal signal;
    LinkedList2 processes;
    BPending wait_job;
    DebugObject d_obj;
} BProcessManager;

/**
 * Handler called when the process terminates.
 * The process object must be freed from the job context of this handler.
 * {@link BProcess_Terminate} or {@link BProcess_Kill} must not be called
 * after this handler is called.
 * 
 * @param user as in {@link BProcess_InitWithFds} or {@link BProcess_Init}
 * @param normally whether the child process terminated normally (0 or 1)
 * @param normally_exit_status if the child process terminated normally, its exit
 *                             status; otherwise undefined
 */
typedef void (*BProcess_handler) (void *user, int normally, uint8_t normally_exit_status);

/**
 * Represents a child process.
 */
typedef struct {
    BProcessManager *m;
    BProcess_handler handler;
    void *user;
    pid_t pid;
    LinkedList2Node list_node; // node in BProcessManager.processes
    DebugObject d_obj;
    DebugError d_err;
} BProcess;

/**
 * Initializes the process manager.
 * There may be at most one process manager at any given time. This restriction is not
 * enforced, however.
 * 
 * @param o the object
 * @param reactor reactor we live in
 * @return 1 on success, 0 on failure
 */
int BProcessManager_Init (BProcessManager *o, BReactor *reactor) WARN_UNUSED;

/**
 * Frees the process manager.
 * There must be no {@link BProcess} objects using this process manager.
 * 
 * @param o the object
 */
void BProcessManager_Free (BProcessManager *o);

/**
 * Initializes the process.
 * 'file', 'argv', 'username', 'fds' and 'fds_map' arguments are only used during this
 * function call.
 * If no file descriptor is mapped to a standard stream (file descriptors 0, 1, 2),
 * then /dev/null will be opened in the child for that standard stream.
 * 
 * @param o the object
 * @param m process manager
 * @param handler handler called when the process terminates
 * @param user argument to handler
 * @param file path to executable file
 * @param argv arguments array, including the zeroth argument, terminated with a NULL pointer
 * @param username user account to run the program as, or NULL to not switch user
 * @param fds array of file descriptors in the parent to map to file descriptors in the child,
 *            terminated with -1
 * @param fds_map array of file descriptors in the child that file descriptors in 'fds' will
 *                be mapped to, in the same order. Must contain the same number of file descriptors
 *                as the 'fds' argument, and does not have to be terminated with -1.
 * @return 1 on success, 0 on failure
 */
int BProcess_InitWithFds (BProcess *o, BProcessManager *m, BProcess_handler handler, void *user, const char *file, char *const argv[], const char *username, const int *fds, const int *fds_map) WARN_UNUSED;

/**
 * Initializes the process.
 * Like {@link BProcess_InitWithFds}, but without file descriptor mapping.
 * 'file', 'argv' and 'username' arguments are only used during this function call.
 * 
 * @param o the object
 * @param m process manager
 * @param handler handler called when the process terminates
 * @param user argument to handler
 * @param file path to executable file
 * @param argv arguments array, including the zeroth argument, terminated with a NULL pointer
 * @param username user account to run the program as, or NULL to not switch user
 * @return 1 on success, 0 on failure
 */
int BProcess_Init (BProcess *o, BProcessManager *m, BProcess_handler handler, void *user, const char *file, char *const argv[], const char *username) WARN_UNUSED;

/**
 * Frees the process.
 * This does not do anything with the actual child process; it only prevents the user to wait
 * for its termination. If the process terminates while a process manager is running, it will still
 * be waited for (and will not become a zombie).
 * 
 * @param o the object
 */
void BProcess_Free (BProcess *o);

/**
 * Sends the process the SIGTERM signal.
 * Success of this action does NOT mean that the child has terminated.
 * 
 * @param o the object
 * @return 1 on success, 0 on failure
 */
int BProcess_Terminate (BProcess *o);

/**
 * Sends the process the SIGKILL signal.
 * Success of this action does NOT mean that the child has terminated.
 * 
 * @param o the object
 * @return 1 on success, 0 on failure
 */
int BProcess_Kill (BProcess *o);

#endif
