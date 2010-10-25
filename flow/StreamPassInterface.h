/**
 * @file StreamPassInterface.h
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
 * Interface allowing a stream sender to pass stream data to a stream receiver.
 * 
 * Note that this interface behaves exactly the same and has the same code as
 * {@link StreamRecvInterface} if names and its external semantics are disregarded.
 * If you modify this file, you should probably modify {@link StreamRecvInterface}
 * too.
 */

#ifndef BADVPN_FLOW_STREAMPASSINTERFACE_H
#define BADVPN_FLOW_STREAMPASSINTERFACE_H

#include <stdint.h>
#include <stddef.h>

#include <misc/dead.h>
#include <misc/debug.h>
#include <system/DebugObject.h>

/**
 * Handler called at the receiver when {@link StreamPassInterface_Sender_Send} is called
 * from the sender.
 * It is guaranteed that the interface is in not sending state.
 * It is guaranteed that the handler is not being called from within the Send handler.
 *
 * @param user value supplied to {@link StreamPassInterface_Init}
 * @param data pointer to data being sent
 * @param data_len amount of data being sent. Will be >0.
 * @return - >0 if the receiver accepts some data immediately, indicating how much of the
 *           data was accepted. The interface remains in not sending state. The receiver
 *           may not use the provided data after the handler returns.
 *         - 0 if the receiver cannot accept any data immediately. The interface enters
 *           sending state as the handler returns. The receiver may use the provided data
 *           as long as it needs to. When it's done processing some data and doesn't need
 *           the data any more, it must call {@link StreamPassInterface_Done}.
 */
typedef int (*StreamPassInterface_handler_send) (void *user, uint8_t *data, int data_len);

/**
 * Handler called at the sender when {@link StreamPassInterface_Done} is called from the receiver.
 * The receiver will no longer use the data it was provided with.
 * It is guaranteed that the interface was in sending state.
 * The interface enters not sending state before the handler is called.
 * It is guaranteed that the handler is not being called from within Send or Done handlers.
 *
 * @param user value supplied to {@link StreamPassInterface_Sender_Init}
 * @param data_len amount of data the receiver processed. Will be >0 and not exceed the amount
 *                 of data submitted in {@link StreamPassInterface_Sender_Send}.
 */
typedef void (*StreamPassInterface_handler_done) (void *user, int data_len);

/**
 * Interface allowing a stream sender to pass stream data to a stream receiver.
 * The sender passes some data by providing the receiver with a pointer
 * to the data. The receiver may then either accept some of the data immediately,
 * or tell the sender to wait for some data to be processed and inform it
 * when it's done.
 */
typedef struct
{
    DebugObject d_obj;
    
    // receiver data
    StreamPassInterface_handler_send handler_send;
    void *user_receiver;
    
    // sender data
    StreamPassInterface_handler_done handler_done;
    void *user_sender;
    
    // debug vars
    #ifndef NDEBUG
    dead_t debug_dead;
    int debug_busy_len;
    int debug_in_send;
    int debug_in_done;
    #endif
} StreamPassInterface;

/**
 * Initializes the interface. The sender portion must also be initialized
 * with {@link StreamPassInterface_Sender_Init} before I/O can start.
 * The interface is initialized in not sending state.
 *
 * @param i the object
 * @param handler_send handler called when the sender wants to send some data
 * @param user arbitrary value that will be passed to receiver callback functions
 */
static void StreamPassInterface_Init (StreamPassInterface *i, StreamPassInterface_handler_send handler_send, void *user);

/**
 * Frees the interface.
 *
 * @param i the object
 */
static void StreamPassInterface_Free (StreamPassInterface *i);

/**
 * Notifies the sender that the receiver has finished processing some of the data being sent.
 * The receiver must not use the data it was provided any more.
 * The interface must be in sending state.
 * The interface enters not sending state before notifying the sender.
 * Must not be called from within Send or Done handlers.
 *
 * Be aware that the sender may attempt to send data from within this function.
 *
 * @param i the object
 * @param data_len amount of data processed. Must be >0 and not exceed the amount of data
 *                 the receiver was provided with in {@link StreamPassInterface_handler_send}.
 */
static void StreamPassInterface_Done (StreamPassInterface *i, int data_len);

/**
 * Initializes the sender portion of the interface.
 *
 * @param i the object
 * @param handler_done handler called when the receiver has finished processing some data
 * @param user arbitrary value that will be passed to sender callback functions
 */
static void StreamPassInterface_Sender_Init (StreamPassInterface *i, StreamPassInterface_handler_done handler_done, void *user);

/**
 * Attempts to send some data.
 * The interface must be in not sending state.
 * Must not be called from within the Send handler.
 *
 * @param i the object
 * @param data pointer to data to send
 * @param data_len amount of data to send. Must be >0.
 * @return - >0 if some data was accepted by the receiver immediately, indicating how much of
 *           the data was accepted. The data is no longer needed.
 *           The interface remains in not sending state.
 *         - 0 if no data could not be accepted immediately and is being processed.
 *           The interface enters sending state, and the data must stay accessible while the
 *           receiver is processing it. When the receiver is done processing it, the
 *           {@link StreamPassInterface_handler_done} handler will be called.
 */
static int StreamPassInterface_Sender_Send (StreamPassInterface *i, uint8_t *data, int data_len);

#ifndef NDEBUG

/**
 * Determines if we are in a Send call.
 * Only available if NDEBUG is not defined.
 * 
 * @param i the object
 * @return 1 if in a Send call, 0 if not
 */
static int StreamPassInterface_InClient (StreamPassInterface *i);

/**
 * Determines if we are in a Done call.
 * Only available if NDEBUG is not defined.
 * 
 * @param i the object
 * @return 1 if in a Done call, 0 if not
 */
static int StreamPassInterface_InDone (StreamPassInterface *i);

#endif

void StreamPassInterface_Init (StreamPassInterface *i, StreamPassInterface_handler_send handler_send, void *user)
{
    i->handler_send = handler_send;
    i->user_receiver = user;
    i->handler_done = NULL;
    i->user_sender = NULL;
    
    // init debugging
    #ifndef NDEBUG
    DEAD_INIT(i->debug_dead);
    i->debug_busy_len = -1;
    i->debug_in_send = 0;
    i->debug_in_done = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&i->d_obj);
}

void StreamPassInterface_Free (StreamPassInterface *i)
{
    // free debug object
    DebugObject_Free(&i->d_obj);
    
    // free debugging
    #ifndef NDEBUG
    DEAD_KILL(i->debug_dead);
    #endif
}

void StreamPassInterface_Done (StreamPassInterface *i, int data_len)
{
    ASSERT(i->debug_busy_len > 0)
    ASSERT(!i->debug_in_send)
    ASSERT(!i->debug_in_done)
    ASSERT(data_len > 0)
    ASSERT(data_len <= i->debug_busy_len)
    
    #ifndef NDEBUG
    i->debug_busy_len = -1;
    i->debug_in_done = 1;
    DEAD_ENTER(i->debug_dead)
    #endif
    
    i->handler_done(i->user_sender, data_len);
    
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->debug_dead)) {
        return;
    }
    i->debug_in_done = 0;
    #endif
}

void StreamPassInterface_Sender_Init (StreamPassInterface *i, StreamPassInterface_handler_done handler_done, void *user)
{
    i->handler_done = handler_done;
    i->user_sender = user;
}

int StreamPassInterface_Sender_Send (StreamPassInterface *i, uint8_t *data, int data_len)
{
    ASSERT(i->debug_busy_len == -1)
    ASSERT(!i->debug_in_send)
    ASSERT(data_len > 0)
    ASSERT(data)
    
    #ifndef NDEBUG
    i->debug_in_send = 1;
    DEAD_ENTER(i->debug_dead)
    #endif
    
    int res = i->handler_send(i->user_receiver, data, data_len);
    
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->debug_dead)) {
        return -1;
    }
    i->debug_in_send = 0;
    ASSERT(res >= 0)
    ASSERT(res <= data_len)
    if (res == 0) {
        i->debug_busy_len = data_len;
    }
    #endif

    return res;
}

#ifndef NDEBUG

int StreamPassInterface_InClient (StreamPassInterface *i)
{
    return i->debug_in_send;
}

int StreamPassInterface_InDone (StreamPassInterface *i)
{
    return i->debug_in_done;
}

#endif

#endif
