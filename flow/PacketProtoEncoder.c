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

static int encode_packet (PacketProtoEncoder *enc, uint8_t *data, int in_len);
static int output_handler_recv (PacketProtoEncoder *enc, uint8_t *data, int *out_len);
static void input_handler_done (PacketProtoEncoder *enc, int in_len);

int encode_packet (PacketProtoEncoder *enc, uint8_t *data, int in_len)
{
    // write header
    struct packetproto_header *header = (struct packetproto_header *)data;
    header->len = htol16(in_len);
    
    return PACKETPROTO_ENCLEN(in_len);
}

int output_handler_recv (PacketProtoEncoder *enc, uint8_t *data, int *out_len)
{
    ASSERT(!enc->output_packet)
    ASSERT(data)
    
    // call recv on input
    int in_len;
    DEAD_ENTER(enc->dead)
    int res = PacketRecvInterface_Receiver_Recv(enc->input, data + sizeof(struct packetproto_header), &in_len);
    if (DEAD_LEAVE(enc->dead)) {
        return -1;
    }
    
    ASSERT(res == 0 || res == 1)
    
    if (!res) {
        // input busy, continue in input_handler_done
        enc->output_packet = data;
        return 0;
    }
    
    // encode
    *out_len = encode_packet(enc, data, in_len);
    
    return 1;
}

void input_handler_done (PacketProtoEncoder *enc, int in_len)
{
    ASSERT(enc->output_packet)
    
    // encode
    int out_len = encode_packet(enc, enc->output_packet, in_len);
    
    // set no output packet
    enc->output_packet = NULL;
    
    // notify output
    PacketRecvInterface_Done(&enc->output, out_len);
    return;
}

void PacketProtoEncoder_Init (PacketProtoEncoder *enc, PacketRecvInterface *input)
{
    ASSERT(PacketRecvInterface_GetMTU(input) <= PACKETPROTO_MAXPAYLOAD)
    
    // init arguments
    enc->input = input;
    
    // init dead var
    DEAD_INIT(enc->dead);
    
    // init input
    PacketRecvInterface_Receiver_Init(enc->input, (PacketRecvInterface_handler_done)input_handler_done, enc);
    
    // init output
    PacketRecvInterface_Init(
        &enc->output,
        PACKETPROTO_ENCLEN(PacketRecvInterface_GetMTU(enc->input)),
        (PacketRecvInterface_handler_recv)output_handler_recv,
        enc
    );
    
    // set no output packet
    enc->output_packet = NULL;
    
    // init debug object
    DebugObject_Init(&enc->d_obj);
}

void PacketProtoEncoder_Free (PacketProtoEncoder *enc)
{
    // free debug object
    DebugObject_Free(&enc->d_obj);

    // free input
    PacketRecvInterface_Free(&enc->output);
    
    // free dead var
    DEAD_KILL(enc->dead);
}

PacketRecvInterface * PacketProtoEncoder_GetOutput (PacketProtoEncoder *enc)
{
    return &enc->output;
}
