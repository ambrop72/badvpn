/**
 * @file PacketProtoDecoder.h
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
 * Object which decodes a stream according to PacketProto.
 */

#ifndef BADVPN_FLOW_PACKETPROTODECODER_H
#define BADVPN_FLOW_PACKETPROTODECODER_H

#include <stdint.h>

#include <protocol/packetproto.h>
#include <misc/debug.h>
#include <misc/balign.h>
#include <base/DebugObject.h>
#include <flow/StreamRecvInterface.h>
#include <flow/PacketPassInterface.h>

/**
 * Handler called when a protocol error occurs.
 * When an error occurs, the decoder is reset to the initial state.
 * 
 * @param user as in {@link PacketProtoDecoder_Init}
 */
typedef void (*PacketProtoDecoder_handler_error) (void *user);

typedef struct {
    StreamRecvInterface *input;
    PacketPassInterface *output;
    void *user;
    PacketProtoDecoder_handler_error handler_error;
    int output_mtu;
    int buf_size;
    int buf_start;
    int buf_used;
    uint8_t *buf;
    DebugObject d_obj;
} PacketProtoDecoder;

/**
 * Initializes the object.
 *
 * @param enc the object
 * @param input input interface. The decoder will accept packets with payload size up to its MTU
 *              (but the payload can never be more than PACKETPROTO_MAXPAYLOAD).
 * @param output output interface
 * @param pg pending group
 * @param user argument to handlers
 * @param handler_error error handler
 * @return 1 on success, 0 on failure
 */
int PacketProtoDecoder_Init (PacketProtoDecoder *enc, StreamRecvInterface *input, PacketPassInterface *output, BPendingGroup *pg, void *user, PacketProtoDecoder_handler_error handler_error) WARN_UNUSED;

/**
 * Frees the object.
 *
 * @param enc the object
 */
void PacketProtoDecoder_Free (PacketProtoDecoder *enc);

/**
 * Clears the internal buffer.
 * The next data received from the input will be treated as a new
 * PacketProto stream.
 *
 * @param enc the object
 */
void PacketProtoDecoder_Reset (PacketProtoDecoder *enc);

#endif
