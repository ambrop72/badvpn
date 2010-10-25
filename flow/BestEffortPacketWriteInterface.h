/**
 * @file BestEffortPacketWriteInterface.h
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
 * Interface which allows a sender to write packets to a buffer provided by the receiver
 * in a best-effort fashion.
 */

#ifndef BADVPN_FLOW_BESTEFFORTPACKETWRITEINTERFACE
#define BADVPN_FLOW_BESTEFFORTPACKETWRITEINTERFACE

#include <stdint.h>
#include <stdlib.h>

#include <misc/debug.h>
#include <misc/dead.h>
#include <system/DebugObject.h>

/**
 * Callback function invoked at the receiver when the sender requests a memory location
 * for writing a packet.
 * The interface was not in writing state.
 *
 * @param user value given to {@link BestEffortPacketWriteInterface_Init}
 * @param data if a memory location was provided, it must be returned here. It must have
 *             MTU bytes available. It may be set to NULL if MTU is 0.
 * @param return 1 - a memory location was provided. The interface will enter writing state.
 *               0 - a memory location was not provided
 */
typedef int (*BestEffortPacketWriteInterface_handler_startpacket) (void *user, uint8_t **data);

/**
 * Callback function invoked at the receiver when the sender has finished writing a packet.
 * The interface was in writing state before.
 * The interface enters not writing state after this function returns.
 *
 * @param user value given to {@link BestEffortPacketWriteInterface_Init}
 * @param len length of the packet written. Will be >=0 and <=MTU.
 */
typedef void (*BestEffortPacketWriteInterface_handler_endpacket) (void *user, int len);

/**
 * Interface which allows a sender to write packets to a buffer provided by the receiver
 * in a best-effort fashion.
 */
typedef struct {
    DebugObject d_obj;
    int mtu;
    BestEffortPacketWriteInterface_handler_startpacket handler_startpacket;
    BestEffortPacketWriteInterface_handler_endpacket handler_endpacket;
    void *user;
    #ifndef NDEBUG
    int sending;
    int in_call;
    dead_t dead;
    #endif
} BestEffortPacketWriteInterface;

/**
 * Initializes the interface.
 * The interface is initialized in not writing state.
 *
 * @param i the object
 * @param mtu maximum packet size. Must be >=0.
 * @param handler_startpacket callback function invoked at the receiver when
 *                            the sender wants a memory location for writing a packet
 * @param handler_endpacket callback function invoked at the receiver when the sender
 *                          has finished writing a packet
 * @param user value passed to receiver callback functions
 */
static void BestEffortPacketWriteInterface_Init (
    BestEffortPacketWriteInterface *i,
    int mtu,
    BestEffortPacketWriteInterface_handler_startpacket handler_startpacket,
    BestEffortPacketWriteInterface_handler_endpacket handler_endpacket,
    void *user
);

/**
 * Frees the interface.
 *
 * @param i the object
 */
static void BestEffortPacketWriteInterface_Free (BestEffortPacketWriteInterface *i);

/**
 * Requests a memory location for writing a packet to the receiver.
 * The interface must be in not writing state.
 *
 * @param i the object
 * @param data if the function returns 1, will be set to the memory location where the
 * *           packet should be written. May be set to NULL if MTU is 0.
 * @param return - 1 a memory location was provided. The interface enters writing state.
 *               - 0 a memory location was not provided
 */
static int BestEffortPacketWriteInterface_Sender_StartPacket (BestEffortPacketWriteInterface *i, uint8_t **data);

/**
 * Sumbits a packet written to the memory location provided by the receiver.
 * The interface must be in writing state.
 * The interface enters not writing state.
 *
 * @param i the object
 * @param len length of the packet written. Must be >=0 and <=MTU.
 */
static void BestEffortPacketWriteInterface_Sender_EndPacket (BestEffortPacketWriteInterface *i, int len);

void BestEffortPacketWriteInterface_Init (
    BestEffortPacketWriteInterface *i,
    int mtu,
    BestEffortPacketWriteInterface_handler_startpacket handler_startpacket,
    BestEffortPacketWriteInterface_handler_endpacket handler_endpacket,
    void *user
)
{
    ASSERT(mtu >= 0)
    
    // init arguments
    i->mtu = mtu;
    i->handler_startpacket = handler_startpacket;
    i->handler_endpacket = handler_endpacket;
    i->user = user;
    
    // init debug object
    DebugObject_Init(&i->d_obj);
    
    // init debugging
    #ifndef NDEBUG
    i->sending = 0;
    i->in_call = 0;
    DEAD_INIT(i->dead);
    #endif
}

void BestEffortPacketWriteInterface_Free (BestEffortPacketWriteInterface *i)
{
    // free debug object
    DebugObject_Free(&i->d_obj);
    
    // free debugging
    #ifndef NDEBUG
    DEAD_KILL(i->dead);
    #endif
}

int BestEffortPacketWriteInterface_Sender_StartPacket (BestEffortPacketWriteInterface *i, uint8_t **data)
{
    ASSERT(!i->sending)
    ASSERT(!i->in_call)
    
    #ifndef NDEBUG
    i->in_call = 1;
    DEAD_ENTER(i->dead)
    #endif
    
    int res = i->handler_startpacket(i->user, data);
    
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->dead)) {
        return -1;
    }
    ASSERT(res == 0 || res == 1)
    i->in_call = 0;
    if (res) {
        i->sending = 1;
    }
    #endif
    
    return res;
}

void BestEffortPacketWriteInterface_Sender_EndPacket (BestEffortPacketWriteInterface *i, int len)
{
    ASSERT(len >= 0)
    ASSERT(len <= i->mtu)
    ASSERT(i->sending)
    ASSERT(!i->in_call)
    
    #ifndef NDEBUG
    i->in_call = 1;
    DEAD_ENTER(i->dead)
    #endif
    
    i->handler_endpacket(i->user, len);
    
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->dead)) {
        return;
    }
    i->in_call = 0;
    i->sending = 0;
    #endif
}

#endif
