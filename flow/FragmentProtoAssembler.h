/**
 * @file FragmentProtoAssembler.h
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
 * Object which decodes packets according to FragmentProto.
 */

#ifndef BADVPN_FLOW_FRAGMENTPROTOASSEMBLER_H
#define BADVPN_FLOW_FRAGMENTPROTOASSEMBLER_H

#include <stdint.h>

#include <protocol/fragmentproto.h>
#include <misc/dead.h>
#include <misc/debug.h>
#include <system/DebugObject.h>
#include <structure/LinkedList2.h>
#include <structure/BAVL.h>
#include <flow/PacketPassInterface.h>

struct FragmentProtoAssembler_chunk {
    int start;
    int len;
};

struct FragmentProtoAssembler_frame {
    LinkedList2Node list_node; // node in free or used list
    struct FragmentProtoAssembler_chunk *chunks; // array of chunks, up to num_chunks
    uint8_t *buffer; // buffer with frame data, size output_mtu
    // everything below only defined when frame entry is used
    fragmentproto_frameid id; // frame identifier
    uint32_t time; // packet time when the last chunk was received
    BAVLNode tree_node; // node in tree for searching frames by id
    int num_chunks; // number of valid chunks
    int sum; // sum of all chunks' lengths
    int length; // length of the frame, or -1 if not yet known
    int length_so_far; // if length=-1, current data set's upper bound
};

/**
 * Object which decodes packets according to FragmentProto.
 *
 * Input is with {@link PacketPassInterface}.
 * Output is with {@link PacketPassInterface}.
 */
typedef struct {
    DebugObject d_obj;
    dead_t dead;
    PacketPassInterface input;
    int input_mtu;
    PacketPassInterface *output;
    int output_mtu;
    int num_frames;
    int num_chunks;
    uint32_t time;
    int time_tolerance;
    struct FragmentProtoAssembler_frame *frames_entries;
    struct FragmentProtoAssembler_chunk *frames_chunks;
    uint8_t *frames_buffer;
    LinkedList2 frames_free;
    LinkedList2 frames_used;
    BAVL frames_used_tree;
    int in_len;
    uint8_t *in;
    int in_pos;
    int output_blocking;
} FragmentProtoAssembler;

/**
 * Initializes the object.
 * {@link BLog_Init} must have been done.
 *
 * @param o the object
 * @param input_mtu maximum input packet size. Must be >=0.
 * @param output output interface
 * @param num_frames number of frames we can hold. Must be >0 and < UINT32_MAX.
 * @param num_chunks maximum number of chunks a frame can come in. Must be >0.
 * @return 1 on success, 0 on failure
 */
int FragmentProtoAssembler_Init (FragmentProtoAssembler *o, int input_mtu, PacketPassInterface *output, int num_frames, int num_chunks) WARN_UNUSED;

/**
 * Frees the object.
 *
 * @param o the object
 */
void FragmentProtoAssembler_Free (FragmentProtoAssembler *o);

/**
 * Returns the input interface.
 *
 * @param o the object
 * @return input interface
 */
PacketPassInterface * FragmentProtoAssembler_GetInput (FragmentProtoAssembler *o);

#endif
