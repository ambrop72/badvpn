/**
 * @file PacketRecvInterface.h
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
 * Interface allowing a packet receiver to receive data packets from a packet sender.
 */

#ifndef BADVPN_FLOW_PACKETRECVINTERFACE_H
#define BADVPN_FLOW_PACKETRECVINTERFACE_H

#include <stdint.h>
#include <stddef.h>

#include <misc/dead.h>
#include <misc/debug.h>
#include <system/DebugObject.h>

/**
 * Handler called at the sender when {@link PacketRecvInterface_Receiver_Recv} is called
 * from the receiver.
 * It is guaranteed that the interface is in not receiving state.
 * It is guaranteed that the handler is not being called from within Recv or Cancel handlers.
 *
 * @param user value supplied to {@link PacketRecvInterface_Init}
 * @param data pointer to the buffer where the packet is to be written. Will have space
 *             for MTU bytes. May be NULL if MTU is 0.
 * @param data_len if the packet was written immediately, must be set to its length
 * @return - 1 if the sender provides a packet immediately. The interface remains in
 *           not receiving state. The sender may not use the provided buffer after the handler
 *           returns.
 *         - 0 if the sender cannot provide a packet immediately. The interface enters
 *           receiving state as the handler returns. The sender must write a packet to the
 *           provided buffer and call {@link PacketRecvInterface_Done} when it's done.
 */
typedef int (*PacketRecvInterface_handler_recv) (void *user, uint8_t *data, int *data_len);

/**
 * Handler called at the receiver when {@link PacketRecvInterface_Done} is called from the sender.
 * The sender will no longer use the buffer it was provided with.
 * It is guaranteed that the interface was in receiving state.
 * The interface enters not receiving state before the handler is called.
 * It is guaranteed that the handler is not being called from within Recv, Cancel or Done handlers.
 *
 * @param user value supplied to {@link PacketRecvInterface_Receiver_Init}
 * @param data_len size of the packet that was written to the buffer. Will be >=0 and <=MTU.
 */
typedef void (*PacketRecvInterface_handler_done) (void *user, int data_len);

/**
 * Interface allowing a packet receiver to receive data packets from a packet sender.
 * The receiver receives a packet by providing the sender with a buffer. The sender
 * may then either provide the packet immediately, or tell the receiver to wait for
 * the packet to be available and inform it when it's done.
 */
typedef struct {
    DebugObject d_obj;
    
    // sender data
    int mtu;
    PacketRecvInterface_handler_recv handler_recv;
    void *user_sender;

    // receiver data
    PacketRecvInterface_handler_done handler_done;
    void *user_receiver;
    
    // debug vars
    #ifndef NDEBUG
    dead_t debug_dead;
    int debug_busy;
    int debug_in_recv;
    int debug_in_done;
    #endif
} PacketRecvInterface;

/**
 * Initializes the interface. The receiver portion must also be initialized
 * with {@link PacketRecvInterface_Receiver_Init} before I/O can start.
 * The interface is initialized in not receiving state.
 *
 * @param i the object
 * @param mtu maximum packet size the sender can provide. Must be >=0.
 * @param handler_recv handler called when the receiver wants to receive a packet
 * @param user arbitrary value that will be passed to sender callback functions
 */
static void PacketRecvInterface_Init (PacketRecvInterface *i, int mtu, PacketRecvInterface_handler_recv handler_recv, void *user);

/**
 * Frees the interface.
 *
 * @param i the object
 */
static void PacketRecvInterface_Free (PacketRecvInterface *i);

/**
 * Notifies the receiver that the sender has finished providing the packet being received.
 * The sender must not use the buffer it was provided any more.
 * The interface must be in receiving state.
 * The interface enters not receiving state before notifying the receiver.
 * Must not be called from within Recv, Cancel or Done handlers.
 *
 * Be aware that the receiver may attempt to receive packets from within this function.
 *
 * @param i the object
 * @param data_len size of the packet written to the buffer. Must be >=0 and <=MTU.
 */
static void PacketRecvInterface_Done (PacketRecvInterface *i, int data_len);

/**
 * Returns the maximum packet size the sender can provide.
 *
 * @return maximum packet size. Will be >=0.
 */
static int PacketRecvInterface_GetMTU (PacketRecvInterface *i);

/**
 * Initializes the receiver portion of the interface.
 *
 * @param i the object
 * @param handler_done handler called when the sender has finished providing a packet
 * @param user arbitrary value that will be passed to receiver callback functions
 */
static void PacketRecvInterface_Receiver_Init (PacketRecvInterface *i, PacketRecvInterface_handler_done handler_done, void *user);

/**
 * Attempts to receive a packet.
 * The interface must be in not receiving state.
 * Must not be called from within Recv or Cancel handlers.
 *
 * @param i the object
 * @param data pointer to the buffer where the packet is to be written. Must have space
 *             for MTU bytes. Ignored if MTU is 0.
 * @param data_len will contain the size of the packet if it was provided immediately
 * @return - 1 if a packet was provided by the sender immediately. The buffer is no longer needed.
 *           The interface remains in not receiving state.
 *         - 0 if a packet could not be provided immediately.
 *           The interface enters receiving state, and the buffer must stay accessible while the
 *           sender is providing the packet. When the sender is done providing it, the
 *           {@link PacketRecvInterface_handler_done} handler will be called.
 */
static int PacketRecvInterface_Receiver_Recv (PacketRecvInterface *i, uint8_t *data, int *data_len);

#ifndef NDEBUG

/**
 * Determines if we are in a Recv call.
 * Only available if NDEBUG is not defined.
 * 
 * @param i the object
 * @return 1 if in a Recv call, 0 if not
 */
static int PacketRecvInterface_InClient (PacketRecvInterface *i);

/**
 * Determines if we are in a Done call.
 * Only available if NDEBUG is not defined.
 * 
 * @param i the object
 * @return 1 if in a Done call, 0 if not
 */
static int PacketRecvInterface_InDone (PacketRecvInterface *i);

#endif

void PacketRecvInterface_Init (PacketRecvInterface *i, int mtu, PacketRecvInterface_handler_recv handler_recv, void *user)
{
    ASSERT(mtu >= 0)
    
    i->mtu = mtu;
    i->handler_recv = handler_recv;
    i->user_sender = user;
    i->handler_done = NULL;
    i->user_receiver = NULL;
    
    // init debugging
    #ifndef NDEBUG
    DEAD_INIT(i->debug_dead);
    i->debug_busy = 0;
    i->debug_in_recv = 0;
    i->debug_in_done = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&i->d_obj);
}

void PacketRecvInterface_Free (PacketRecvInterface *i)
{
    // free debug object
    DebugObject_Free(&i->d_obj);
    
    // free debugging
    #ifndef NDEBUG
    DEAD_KILL(i->debug_dead);
    #endif
}

void PacketRecvInterface_Done (PacketRecvInterface *i, int data_len)
{
    ASSERT(i->debug_busy)
    ASSERT(!i->debug_in_recv)
    ASSERT(!i->debug_in_done)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= i->mtu)
    
    #ifndef NDEBUG
    i->debug_busy = 0;
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

int PacketRecvInterface_GetMTU (PacketRecvInterface *i)
{
    return i->mtu;
}

void PacketRecvInterface_Receiver_Init (PacketRecvInterface *i, PacketRecvInterface_handler_done handler_done, void *user)
{
    i->handler_done = handler_done;
    i->user_receiver = user;
}

int PacketRecvInterface_Receiver_Recv (PacketRecvInterface *i, uint8_t *data, int *data_len)
{
    ASSERT(!i->debug_busy)
    ASSERT(!i->debug_in_recv)
    ASSERT(!(i->mtu > 0) || data)
    ASSERT(data_len)
    
    #ifndef NDEBUG
    i->debug_in_recv = 1;
    DEAD_ENTER(i->debug_dead)
    #endif
    
    int res = i->handler_recv(i->user_sender, data, data_len);
    
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->debug_dead)) {
        return -1;
    }
    ASSERT(i->debug_in_recv)
    i->debug_in_recv = 0;
    ASSERT(res == 0 || res == 1)
    ASSERT(!(res == 1) || (*data_len >= 0 && *data_len <= i->mtu))
    if (!res) {
        i->debug_busy = 1;
    }
    #endif
    
    return res;
}

#ifndef NDEBUG

int PacketRecvInterface_InClient (PacketRecvInterface *i)
{
    return i->debug_in_recv;
}

int PacketRecvInterface_InDone (PacketRecvInterface *i)
{
    return i->debug_in_done;
}

#endif

#endif
