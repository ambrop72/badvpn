/**
 * @file PacketPassInterface.h
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
 * Interface allowing a packet sender to pass data packets to a packet receiver.
 */

#ifndef BADVPN_FLOW_PACKETPASSINTERFACE_H
#define BADVPN_FLOW_PACKETPASSINTERFACE_H

#include <stdint.h>
#include <stddef.h>

#include <misc/dead.h>
#include <misc/debug.h>
#include <system/DebugObject.h>

/**
 * Handler called at the receiver when {@link PacketPassInterface_Sender_Send} is called
 * from the sender.
 * It is guaranteed that the interface is in not sending state.
 * It is guaranteed that the handler is not being called from within Send or Cancel handlers.
 *
 * @param user value supplied to {@link PacketPassInterface_Init}
 * @param data pointer to packet being sent. May be NULL if data_len=0.
 * @param data_len length of the packet being sent. Will be >=0 and <=MTU.
 * @return - 1 if the receiver accepts the packet immediately. The interface remains in
 *           not sending state. The receiver may not use the provided data after the handler
 *           returns.
 *         - 0 if the receiver cannot accept the packet immediately. The interface enters
 *           sending state as the handler returns. The receiver may use the provided data
 *           as long as it needs to. When it's done processing the packet and doesn't need
 *           the data any more, it must call {@link PacketPassInterface_Done}.
 */
typedef int (*PacketPassInterface_handler_send) (void *user, uint8_t *data, int data_len);

/**
 * Handler called at the receiver when {@link PacketPassInterface_Sender_Cancel} is called
 * from the sender.
 * The buffer is still available inside the handler. It is no longer available
 * after the handler returns.
 * It is guaranteed that the interface is in sending state.
 * The interface enters not sending state as the handler returns.
 * It is guaranteed that the handler is not being called from within Send or Cancel handlers.
 *
 * @param user value supplied to {@link PacketPassInterface_Init}
 */
typedef void (*PacketPassInterface_handler_cancel) (void *user);

/**
 * Handler called at the sender when {@link PacketPassInterface_Done} is called from the receiver.
 * The receiver will no longer use the packet it was provided with.
 * It is guaranteed that the interface was in sending state.
 * The interface enters not sending state before the handler is called.
 * It is guaranteed that the handler is not being called from within Send, Cancel or Done handlers.
 *
 * @param user value supplied to {@link PacketPassInterface_Sender_Init}
 */
typedef void (*PacketPassInterface_handler_done) (void *user);

/**
 * Interface allowing a packet sender to pass data packets to a packet receiver.
 * The sender passes a packet by providing the receiver with a pointer
 * to a packet. The receiver may then either accept the packet immediately,
 * or tell the sender to wait for the packet to be processed and inform it
 * when it's done.
 */
typedef struct {
    DebugObject d_obj;
    
    // receiver data
    int mtu;
    PacketPassInterface_handler_send handler_send;
    PacketPassInterface_handler_cancel handler_cancel;
    void *user_receiver;
    
    // sender data
    PacketPassInterface_handler_done handler_done;
    void *user_sender;
    
    // debug vars
    #ifndef NDEBUG
    dead_t debug_dead;
    int debug_busy;
    int debug_in_send;
    int debug_in_done;
    #endif
} PacketPassInterface;

/**
 * Initializes the interface. The sender portion must also be initialized
 * with {@link PacketPassInterface_Sender_Init} before I/O can start.
 * The interface is initialized in not sending state.
 *
 * @param i the object
 * @param mtu maximum packet size the receiver can accept. Must be >=0.
 * @param handler_send handler called when the sender wants to send a packet
 * @param user arbitrary value that will be passed to receiver callback functions
 */
static void PacketPassInterface_Init (PacketPassInterface *i, int mtu, PacketPassInterface_handler_send handler_send, void *user);

/**
 * Frees the interface.
 *
 * @param i the object
 */
static void PacketPassInterface_Free (PacketPassInterface *i);

/**
 * Enables cancel functionality for the interface.
 * May only be called once for the interface.
 *
 * @param i the object
 * @param handler_cancel callback function invoked when the sender wants to cancel sending
 */
static void PacketPassInterface_EnableCancel (PacketPassInterface *i, PacketPassInterface_handler_cancel handler_cancel);

/**
 * Notifies the sender that the receiver has finished processing the packet being sent.
 * The receiver must not use the data it was provided any more.
 * The interface must be in sending state.
 * The interface enters not sending state before notifying the sender.
 * Must not be called from within Send, Cancel or Done handlers.
 *
 * Be aware that the sender may attempt to send packets from within this function.
 *
 * @param i the object
 */
static void PacketPassInterface_Done (PacketPassInterface *i);

/**
 * Returns the maximum packet size the receiver can accept.
 *
 * @return maximum packet size. Will be >=0.
 */
static int PacketPassInterface_GetMTU (PacketPassInterface *i);

/**
 * Initializes the sender portion of the interface.
 *
 * @param i the object
 * @param handler_done handler called when the receiver has finished processing a packet
 * @param user arbitrary value that will be passed to sender callback functions
 */
static void PacketPassInterface_Sender_Init (PacketPassInterface *i, PacketPassInterface_handler_done handler_done, void *user);

/**
 * Attempts to send a packet.
 * The interface must be in not sending state.
 * Must not be called from within Send or Cancel handlers.
 *
 * @param i the object
 * @param data pointer to the packet to send. If the size of the packet is zero, this argument
 *             is ignored.
 * @param data_len length of the packet. Must be >=0 and <=MTU.
 * @return - 1 if the packet was accepted by the receiver. The packet is no longer needed.
 *           The interface remains in not sending state.
 *         - 0 if the packet could not be accepted immediately and is being processed.
 *           The interface enters sending state, and the packet must stay accessible while the
 *           receiver is processing it. When the receiver is done processing it, the
 *           {@link PacketPassInterface_handler_done} handler will be called.
 */
static int PacketPassInterface_Sender_Send (PacketPassInterface *i, uint8_t *data, int data_len);

/**
 * Cancels sending a packet.
 * Cancel functionality must be available for the interface.
 * The buffer must still be available while calling this.
 * The buffer is no longer needed after this function returns.
 * The interface must be in sending state.
 * The interface enters not sending state.
 * Must not be called from within Send or Cancel handlers.
 *
 * @param i the object
 */
static void PacketPassInterface_Sender_Cancel (PacketPassInterface *i);

/**
 * Determines if the interface supports cancel functionality.
 * 
 * @param i the object
 * @return 1 if the interface supports cancel functionality, 0 if not
 */
static int PacketPassInterface_HasCancel (PacketPassInterface *i);

#ifndef NDEBUG

/**
 * Determines if we are in a Send or Cancel call.
 * Only available if NDEBUG is not defined.
 * 
 * @param i the object
 * @return 1 if in a Send or Cancel call, 0 if not
 */
static int PacketPassInterface_InClient (PacketPassInterface *i);

/**
 * Determines if we are in a Done call.
 * Only available if NDEBUG is not defined.
 * 
 * @param i the object
 * @return 1 if in a Done call, 0 if not
 */
static int PacketPassInterface_InDone (PacketPassInterface *i);

#endif

void PacketPassInterface_Init (PacketPassInterface *i, int mtu, PacketPassInterface_handler_send handler_send, void *user)
{
    ASSERT(mtu >= 0)
    
    i->mtu = mtu;
    i->handler_send = handler_send;
    i->handler_cancel = NULL;
    i->user_receiver = user;
    i->handler_done = NULL;
    i->user_sender = NULL;
    
    // init debugging
    #ifndef NDEBUG
    DEAD_INIT(i->debug_dead);
    i->debug_busy = 0;
    i->debug_in_send = 0;
    i->debug_in_done = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&i->d_obj);
}

void PacketPassInterface_Free (PacketPassInterface *i)
{
    // free debug object
    DebugObject_Free(&i->d_obj);
    
    // free debugging
    #ifndef NDEBUG
    DEAD_KILL(i->debug_dead);
    #endif
}

void PacketPassInterface_EnableCancel (PacketPassInterface *i, PacketPassInterface_handler_cancel handler_cancel)
{
    ASSERT(!i->handler_cancel)
    ASSERT(handler_cancel)
    
    i->handler_cancel = handler_cancel;
}

void PacketPassInterface_Done (PacketPassInterface *i)
{
    ASSERT(i->debug_busy)
    ASSERT(!i->debug_in_send)
    ASSERT(!i->debug_in_done)
    
    #ifndef NDEBUG
    i->debug_busy = 0;
    i->debug_in_done = 1;
    DEAD_ENTER(i->debug_dead)
    #endif
    
    i->handler_done(i->user_sender);
    
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->debug_dead)) {
        return;
    }
    i->debug_in_done = 0;
    #endif
}

int PacketPassInterface_GetMTU (PacketPassInterface *i)
{
    return i->mtu;
}

void PacketPassInterface_Sender_Init (PacketPassInterface *i, PacketPassInterface_handler_done handler_done, void *user)
{
    i->handler_done = handler_done;
    i->user_sender = user;
}

int PacketPassInterface_Sender_Send (PacketPassInterface *i, uint8_t *data, int data_len)
{
    ASSERT(!i->debug_busy)
    ASSERT(!i->debug_in_send)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= i->mtu)
    ASSERT(!(data_len > 0) || data)
    
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
    ASSERT(res == 0 || res == 1)
    if (!res) {
        i->debug_busy = 1;
    }
    #endif

    return res;
}

void PacketPassInterface_Sender_Cancel (PacketPassInterface *i)
{
    ASSERT(i->handler_cancel)
    ASSERT(i->debug_busy)
    ASSERT(!i->debug_in_send)
    
    #ifndef NDEBUG
    i->debug_in_send = 1;
    DEAD_ENTER(i->debug_dead)
    #endif
    
    i->handler_cancel(i->user_receiver);
    
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->debug_dead)) {
        return;
    }
    ASSERT(i->debug_in_send)
    i->debug_in_send = 0;
    i->debug_busy = 0;
    #endif
}

int PacketPassInterface_HasCancel (PacketPassInterface *i)
{
    return !!i->handler_cancel;
}

#ifndef NDEBUG

int PacketPassInterface_InClient (PacketPassInterface *i)
{
    return i->debug_in_send;
}

int PacketPassInterface_InDone (PacketPassInterface *i)
{
    return i->debug_in_done;
}

#endif

#endif
