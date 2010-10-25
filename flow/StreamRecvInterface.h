/**
 * @file StreamRecvInterface.h
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
 * Interface allowing a stream receiver to receive stream data from a stream sender.
 * 
 * Note that this interface behaves exactly the same and has the same code as
 * {@link StreamPassInterface} if names and its external semantics are disregarded.
 * If you modify this file, you should probably modify {@link StreamPassInterface}
 * too.
 */

#ifndef BADVPN_FLOW_STREAMRECVINTERFACE_H
#define BADVPN_FLOW_STREAMRECVINTERFACE_H

#include <stdint.h>
#include <stddef.h>

#include <misc/dead.h>
#include <misc/debug.h>
#include <system/DebugObject.h>

/**
 * Handler called at the sender when {@link StreamRecvInterface_Receiver_Recv} is called
 * from the receiver.
 * It is guaranteed that the interface is in not receiving state.
 * It is guaranteed that the handler is not being called from within Recv or Cancel handlers.
 *
 * @param user value supplied to {@link StreamRecvInterface_Init}
 * @param data pointer to the buffer where data is to be written
 * @param data_avail size of the buffer. Will be >0.
 * @return - >0 if the sender provides some data immediately, indicating how much data was
 *           written to the buffer. The interface remains in not receiving state.
 *           The sender may not use the provided buffer after the handler returns.
 *         - 0 if the sender cannot provide any data immediately. The interface enters
 *           receiving state as the handler returns. The sender must write some data to the
 *           provided buffer and call {@link StreamRecvInterface_Done} when it's done.
 */
typedef int (*StreamRecvInterface_handler_recv) (void *user, uint8_t *data, int data_avail);

/**
 * Handler called at the receiver when {@link StreamRecvInterface_Done} is called from the sender.
 * The sender will no longer use the buffer it was provided with.
 * It is guaranteed that the interface was in receiving state.
 * The interface enters not receiving state before the handler is called.
 * It is guaranteed that the handler is not being called from within Recv or Done handlers.
 *
 * @param user value supplied to {@link StreamRecvInterface_Receiver_Init}
 * @param data_len number of bytes written. Will be >0 and <= the size of the buffer
 *                 that was provided in the previous {@link StreamRecvInterface_Receiver_Recv}
 *                 call.
 */
typedef void (*StreamRecvInterface_handler_done) (void *user, int data_len);

/**
 * Interface allowing a stream receiver to receive stream data from a stream sender.
 * The receiver receives data by providing the sender with a buffer. The sender
 * may then either provide some data immediately, or tell the receiver to wait for
 * some data to be available and inform it when it's done.
 */
typedef struct {
    DebugObject d_obj;
    
    // sender data
    StreamRecvInterface_handler_recv handler_recv;
    void *user_sender;

    // receiver data
    StreamRecvInterface_handler_done handler_done;
    void *user_receiver;
    
    // debug vars
    #ifndef NDEBUG
    dead_t debug_dead;
    int debug_busy_len;
    int debug_in_recv;
    int debug_in_done;
    #endif
} StreamRecvInterface;

/**
 * Initializes the interface. The receiver portion must also be initialized
 * with {@link StreamRecvInterface_Receiver_Init} before I/O can start.
 * The interface is initialized in not receiving state.
 *
 * @param i the object
 * @param handler_recv handler called when the receiver wants to receive data
 * @param user arbitrary value that will be passed to sender callback functions
 */
static void StreamRecvInterface_Init (StreamRecvInterface *i, StreamRecvInterface_handler_recv handler_recv, void *user);

/**
 * Frees the interface.
 *
 * @param i the object
 */
static void StreamRecvInterface_Free (StreamRecvInterface *i);

/**
 * Notifies the receiver that the sender has finished providing some data.
 * The sender must not use the buffer it was provided any more.
 * The interface must be in receiving state.
 * The interface enters not receiving state before notifying the receiver.
 * Must not be called from within Recv or Done handlers.
 *
 * Be aware that the receiver may attempt to receive data from within this function.
 *
 * @param i the object
 * @param data_len number of bytes written. Must be >0 and <= the size of the buffer
 *                 that was provided in the previous {@link StreamRecvInterface_handler_recv}
 *                 call.
 */
static void StreamRecvInterface_Done (StreamRecvInterface *i, int data_len);

/**
 * Initializes the receiver portion of the interface.
 *
 * @param i the object
 * @param handler_done handler called when the sender has finished providing a packet
 * @param user arbitrary value that will be passed to receiver callback functions
 */
static void StreamRecvInterface_Receiver_Init (StreamRecvInterface *i, StreamRecvInterface_handler_done handler_done, void *user);

/**
 * Attempts to receive some data.
 * The interface must be in not receiving state.
 * Must not be called from within the Recv handler.
 *
 * @param i the object
 * @param data pointer to the buffer where data is to be written
 * @param data_avail size of the buffer. Must be >0.
 * @return - >0 if some data was provided by the sender imediately, indicating how much data was
 *           provided. The buffer is no longer needed.
 *           The interface remains in not receiving state.
 *         - 0 if no data could not be provided immediately.
 *           The interface enters receiving state, and the buffer must stay accessible while the
 *           sender is providing the data. When the sender is done providing it, the
 *           {@link StreamRecvInterface_handler_done} handler will be called.
 */
static int StreamRecvInterface_Receiver_Recv (StreamRecvInterface *i, uint8_t *data, int data_avail);

#ifndef NDEBUG

/**
 * Determines if we are in a Recv call.
 * Only available if NDEBUG is not defined.
 * 
 * @param i the object
 * @return 1 if in a Recv call, 0 if not
 */
static int StreamRecvInterface_InClient (StreamRecvInterface *i);

/**
 * Determines if we are in a Done call.
 * Only available if NDEBUG is not defined.
 * 
 * @param i the object
 * @return 1 if in a Done call, 0 if not
 */
static int StreamRecvInterface_InDone (StreamRecvInterface *i);

#endif

void StreamRecvInterface_Init (StreamRecvInterface *i, StreamRecvInterface_handler_recv handler_recv, void *user)
{
    i->handler_recv = handler_recv;
    i->user_sender = user;
    i->handler_done = NULL;
    i->user_receiver = NULL;
    
    // init debugging
    #ifndef NDEBUG
    DEAD_INIT(i->debug_dead);
    i->debug_busy_len = -1;
    i->debug_in_recv = 0;
    i->debug_in_done = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&i->d_obj);
}

void StreamRecvInterface_Free (StreamRecvInterface *i)
{
    // free debug object
    DebugObject_Free(&i->d_obj);
    
    // free debugging
    #ifndef NDEBUG
    DEAD_KILL(i->debug_dead);
    #endif
}

void StreamRecvInterface_Done (StreamRecvInterface *i, int data_len)
{
    ASSERT(i->debug_busy_len > 0)
    ASSERT(!i->debug_in_recv)
    ASSERT(!i->debug_in_done)
    ASSERT(data_len > 0)
    ASSERT(data_len <= i->debug_busy_len)
    
    #ifndef NDEBUG
    i->debug_busy_len = -1;
    i->debug_in_done = 1;
    DEAD_ENTER(i->debug_dead)
    #endif
    
    i->handler_done(i->user_receiver, data_len);
    
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->debug_dead)) {
        return;
    }
    i->debug_in_done = 0;
    #endif
}

void StreamRecvInterface_Receiver_Init (StreamRecvInterface *i, StreamRecvInterface_handler_done handler_done, void *user)
{
    i->handler_done = handler_done;
    i->user_receiver = user;
}

int StreamRecvInterface_Receiver_Recv (StreamRecvInterface *i, uint8_t *data, int data_avail)
{
    ASSERT(i->debug_busy_len == -1)
    ASSERT(!i->debug_in_recv)
    ASSERT(data_avail > 0)
    ASSERT(data)
    
    #ifndef NDEBUG
    i->debug_in_recv = 1;
    DEAD_ENTER(i->debug_dead)
    #endif
    
    int res = i->handler_recv(i->user_sender, data, data_avail);
    
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->debug_dead)) {
        return -1;
    }
    i->debug_in_recv = 0;
    ASSERT(res >= 0)
    ASSERT(res <= data_avail)
    if (res == 0) {
        i->debug_busy_len = data_avail;
    }
    #endif
    
    return res;
}

#ifndef NDEBUG

int StreamRecvInterface_InClient (StreamRecvInterface *i)
{
    return i->debug_in_recv;
}

int StreamRecvInterface_InDone (StreamRecvInterface *i)
{
    return i->debug_in_done;
}

#endif

#endif
