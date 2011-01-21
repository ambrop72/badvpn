/**
 * @file FragmentProtoDisassembler.h
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
 * Object which encodes packets into packets composed of chunks
 * according to FragmentProto.
 */

#ifndef BADVPN_FLOW_CCPROTODISASSEMBLER_H
#define BADVPN_FLOW_CCPROTODISASSEMBLER_H

#include <stdint.h>

#include <protocol/fragmentproto.h>
#include <system/DebugObject.h>
#include <system/BReactor.h>
#include <system/BTime.h>
#include <flow/PacketPassInterface.h>
#include <flow/PacketRecvInterface.h>

/**
 * Object which encodes packets into packets composed of chunks
 * according to FragmentProto.
 *
 * Input is with {@link PacketPassInterface}.
 * Output is with {@link PacketRecvInterface}.
 */
typedef struct {
    BReactor *reactor;
    int output_mtu;
    int chunk_mtu;
    btime_t latency;
    PacketPassInterface input;
    PacketRecvInterface output;
    BTimer timer;
    int in_len;
    uint8_t *in;
    int in_used;
    uint8_t *out;
    int out_used;
    fragmentproto_frameid frame_id;
    DebugObject d_obj;
} FragmentProtoDisassembler;

/**
 * Initializes the object.
 *
 * @param o the object
 * @param reactor reactor we live in
 * @param input_mtu maximum input packet size. Must be >=0 and <=UINT16_MAX.
 * @param output_mtu maximum output packet size. Must be >sizeof(struct fragmentproto_chunk_header).
 * @param chunk_mtu maximum chunk size. Must be >0, or <0 for no explicit limit.
 * @param latency maximum time a pending output packet with some data can wait for more data
 *                before being sent out. If nonnegative, a timer will be used. If negative,
 *                packets will always be sent out immediately. If low latency is desired,
 *                prefer setting this to zero rather than negative.
 */
void FragmentProtoDisassembler_Init (FragmentProtoDisassembler *o, BReactor *reactor, int input_mtu, int output_mtu, int chunk_mtu, btime_t latency);

/**
 * Frees the object.
 *
 * @param o the object
 */
void FragmentProtoDisassembler_Free (FragmentProtoDisassembler *o);

/**
 * Returns the input interface.
 *
 * @param o the object
 * @return input interface
 */
PacketPassInterface * FragmentProtoDisassembler_GetInput (FragmentProtoDisassembler *o);

/**
 * Returns the output interface.
 *
 * @param o the object
 * @return output interface
 */
PacketRecvInterface * FragmentProtoDisassembler_GetOutput (FragmentProtoDisassembler *o);

#endif
