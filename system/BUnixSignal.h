/**
 * @file BUnixSignal.h
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
 * Object for catching unix signals.
 */

#ifndef BADVPN_SYSTEM_BUNIXSIGNAL_H
#define BADVPN_SYSTEM_BUNIXSIGNAL_H

#if (defined(BADVPN_USE_SIGNALFD) + defined(BADVPN_USE_KEVENT) + defined(BADVPN_USE_SELFPIPE)) != 1
#error Unknown signal backend or too many signal backends
#endif

#include <unistd.h>
#include <signal.h>

#include <misc/debug.h>
#include <system/BReactor.h>
#include <system/DebugObject.h>

struct BUnixSignal_s;

/**
 * Handler function called when a signal is received.
 * 
 * @param user as in {@link BUnixSignal_Init}
 * @param signo signal number. Will be one of the signals provided to {@link signals}.
 */
typedef void (*BUnixSignal_handler) (void *user, int signo);

#ifdef BADVPN_USE_KEVENT
struct BUnixSignal_kevent_entry {
    struct BUnixSignal_s *parent;
    int signo;
    BReactorKEvent kevent;
};
#endif

#ifdef BADVPN_USE_SELFPIPE
struct BUnixSignal_selfpipe_entry {
    struct BUnixSignal_s *parent;
    int signo;
    int pipefds[2];
    BFileDescriptor pipe_read_bfd;
};
#endif

/**
 * Object for catching unix signals.
 */
typedef struct BUnixSignal_s {
    BReactor *reactor;
    sigset_t signals;
    BUnixSignal_handler handler;
    void *user;
    
    #ifdef BADVPN_USE_SIGNALFD
    int signalfd_fd;
    BFileDescriptor signalfd_bfd;
    #endif
    
    #ifdef BADVPN_USE_KEVENT
    struct BUnixSignal_kevent_entry *entries;
    int num_entries;
    #endif
    
    #ifdef BADVPN_USE_SELFPIPE
    struct BUnixSignal_selfpipe_entry *entries;
    int num_entries;
    #endif
    
    DebugObject d_obj;
} BUnixSignal;

/**
 * Initializes the object.
 * {@link BLog_Init} must have been done.
 * 
 * WARNING: for every signal number there should be at most one {@link BUnixSignal}
 * object handling it (or anything else that could interfere).
 * 
 * This blocks the signal using sigprocmask() and sets up signalfd() for receiving
 * signals.
 *
 * @param o the object
 * @param reactor reactor we live in
 * @param signals signals to handle. See man 3 sigsetops.
 * @param handler handler function to call when a signal is received
 * @param user value passed to callback function
 * @return 1 on success, 0 on failure
 */
int BUnixSignal_Init (BUnixSignal *o, BReactor *reactor, sigset_t signals, BUnixSignal_handler handler, void *user) WARN_UNUSED;

/**
 * Frees the object.
 * 
 * @param o the object
 * @param unblock whether to unblock the signals using sigprocmask(). Not unblocking it
 *                can be used while the program is exiting gracefully to prevent the
 *                signals from being handled handled according to its default disposition
 *                after this function is called. Must be 0 or 1.
 */
void BUnixSignal_Free (BUnixSignal *o, int unblock);

#endif
