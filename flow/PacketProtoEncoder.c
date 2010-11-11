/**
 * @file PacketProtoEncoder.c
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

#include <stddef.h>

#include <protocol/packetproto.h>
#include <misc/balign.h>
#include <misc/debug.h>
#include <misc/byteorder.h>

#include <flow/PacketProtoEncoder.h>

static void output_handler_recv (PacketProtoEncoder *enc, uint8_t *data)
{
    ASSERT(!enc->output_packet)
    ASSERT(data)
    DebugObject_Access(&enc->d_obj);
    
    // schedule receive
    enc->output_packet = data;
    PacketRecvInterface_Receiver_Recv(enc->input, enc->output_packet + sizeof(struct packetproto_header));
}

static void input_handler_done (PacketProtoEncoder *enc, int in_len)
{
    ASSERT(enc->output_packet)
    DebugObject_Access(&enc->d_obj);
    
    // write length
    ((struct packetproto_header *)enc->output_packet)->len = htol16(in_len);
    
    // finish output packet
    enc->output_packet = NULL;
    PacketRecvInterface_Done(&enc->output, PACKETPROTO_ENCLEN(in_len));
}

void PacketProtoEncoder_Init (PacketProtoEncoder *enc, PacketRecvInterface *input, BPendingGroup *pg)
{
    ASSERT(PacketRecvInterface_GetMTU(input) <= PACKETPROTO_MAXPAYLOAD)
    
    // init arguments
    enc->input = input;
    
    // init input
    PacketRecvInterface_Receiver_Init(enc->input, (PacketRecvInterface_handler_done)input_handler_done, enc);
    
    // init output
    PacketRecvInterface_Init(
        &enc->output, PACKETPROTO_ENCLEN(PacketRecvInterface_GetMTU(enc->input)),
        (PacketRecvInterface_handler_recv)output_handler_recv, enc, pg
    );
    
    // set no output packet
    enc->output_packet = NULL;
    
    DebugObject_Init(&enc->d_obj);
}

void PacketProtoEncoder_Free (PacketProtoEncoder *enc)
{
    DebugObject_Free(&enc->d_obj);

    // free input
    PacketRecvInterface_Free(&enc->output);
}

PacketRecvInterface * PacketProtoEncoder_GetOutput (PacketProtoEncoder *enc)
{
    DebugObject_Access(&enc->d_obj);
    
    return &enc->output;
}
