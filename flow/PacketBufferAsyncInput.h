/**
 * @file PacketBufferAsyncInput.h
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
 * Object for writing packets to a {@link PacketRecvInterface} client
 * via {@link BestEffortPacketWriteInterface}.
 */

#ifndef BADVPN_FLOW_PACKETBUFFERASYNCINPUT_H
#define BADVPN_FLOW_PACKETBUFFERASYNCINPUT_H

#include <stdint.h>

#include <misc/debug.h>
#include <system/DebugObject.h>
#include <flow/PacketRecvInterface.h>
#include <flow/BestEffortPacketWriteInterface.h>

typedef void (*PacketBufferAsyncInput_handler_keepalive) (void *user);

/**
 * Object for writing packets to a {@link PacketRecvInterface} client
 * via {@link BestEffortPacketWriteInterface}.
 */
typedef struct {
    DebugObject d_obj;
    BestEffortPacketWriteInterface input;
    PacketRecvInterface recv_interface;
    int have_output_packet;
    uint8_t *output_packet;
} PacketBufferAsyncInput;

/**
 * Initializes the object.
 *
 * @param f the object
 */
static void PacketBufferAsyncInput_Init (PacketBufferAsyncInput *f, int mtu);

/**
 * Frees the object.
 *
 * @param f the object
 */
static void PacketBufferAsyncInput_Free (PacketBufferAsyncInput *f);

/**
 * Returns the output interface.
 *
 * @param f the object
 * @return output interface
 */
static PacketRecvInterface * PacketBufferAsyncInput_GetOutput (PacketBufferAsyncInput *f);

/**
 * Returns the input interface.
 *
 * @param f the object
 * @return input interface
 */
static BestEffortPacketWriteInterface * PacketBufferAsyncInput_GetInput (PacketBufferAsyncInput *f);

static int _PacketBufferAsyncInput_output_handler_recv (PacketBufferAsyncInput *f, uint8_t *data, int *data_len)
{
    ASSERT(!f->have_output_packet)
    
    // store destination
    f->have_output_packet = 1;
    f->output_packet = data;
    
    // block
    return 0;
}

static int _PacketBufferAsyncInput_handler_startpacket (PacketBufferAsyncInput *f, uint8_t **data)
{
    if (!f->have_output_packet) {
        // buffer full
        return 0;
    }
    
    if (data) {
        *data = f->output_packet;
    }
    
    return 1;
}

static void _PacketBufferAsyncInput_handler_endpacket (PacketBufferAsyncInput *f, int len)
{
    f->have_output_packet = 0;
    
    PacketRecvInterface_Done(&f->recv_interface, len);
    return;
}

void PacketBufferAsyncInput_Init (PacketBufferAsyncInput *f, int mtu)
{
    ASSERT(mtu >= 0)
    
    PacketRecvInterface_Init(
        &f->recv_interface,
        mtu,
        (PacketRecvInterface_handler_recv)_PacketBufferAsyncInput_output_handler_recv,
        f
    );
    
    BestEffortPacketWriteInterface_Init(
        &f->input,
        mtu,
        (BestEffortPacketWriteInterface_handler_startpacket)_PacketBufferAsyncInput_handler_startpacket,
        (BestEffortPacketWriteInterface_handler_endpacket)_PacketBufferAsyncInput_handler_endpacket,
        f
    );
    
    f->have_output_packet = 0;
    
    // init debug object
    DebugObject_Init(&f->d_obj);
}

void PacketBufferAsyncInput_Free (PacketBufferAsyncInput *f)
{
    // free debug object
    DebugObject_Free(&f->d_obj);

    BestEffortPacketWriteInterface_Free(&f->input);
    PacketRecvInterface_Free(&f->recv_interface);
}

PacketRecvInterface * PacketBufferAsyncInput_GetOutput (PacketBufferAsyncInput *f)
{
    return &f->recv_interface;
}

BestEffortPacketWriteInterface * PacketBufferAsyncInput_GetInput (PacketBufferAsyncInput *f)
{
    return &f->input;
}

#endif
