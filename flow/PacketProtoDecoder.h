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
#include <misc/dead.h>
#include <misc/balign.h>
#include <system/DebugObject.h>
#include <flow/StreamRecvInterface.h>
#include <flow/PacketPassInterface.h>
#include <flow/error.h>

#define PACKETPROTODECODER_ERROR_TOOLONG 1

/**
 * Object which decodes a stream according to PacketProto.
 *
 * Input is with {@link StreamRecvInterface}.
 * Output is with {@link PacketPassInterface}.
 *
 * Errors are reported through {@link FlowErrorDomain}. All errors
 * are fatal and the object must be freed from the error handler.
 * Error code is an int which is one of the following:
 *     - PACKETPROTODECODER_ERROR_TOOLONG: the packet header contains
 *       a packet length value which is too big,
 */
typedef struct {
    dead_t dead;
    FlowErrorReporter rep;
    StreamRecvInterface *input;
    PacketPassInterface *output;
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
 * @param rep error reporting data
 * @param input input interface. The decoder will accept packets with payload size up to its MTU
 *              (but the payload can never be more than PACKETPROTO_MAXPAYLOAD).
 * @param output output interface
 * @param pg pending group
 * @return 1 on success, 0 on failure
 */
int PacketProtoDecoder_Init (PacketProtoDecoder *enc, FlowErrorReporter rep, StreamRecvInterface *input, PacketPassInterface *output, BPendingGroup *pg) WARN_UNUSED;

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
