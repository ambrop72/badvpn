/**
 * @file fragmentproto.h
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
 * Definitions for FragmentProto, a protocol that allows sending of arbitrarily sized packets over
 * a link with a fixed MTU.
 * 
 * All multi-byte integers in structs are little-endian, unless stated otherwise.
 * 
 * A FragmentProto packet consists of a number of chunks.
 * Each chunk consists of:
 *   - the chunk header (struct {@link fragmentproto_chunk_header})
 *   - the chunk payload, i.e. part of the frame specified in the header
 */

#ifndef BADVPN_PROTOCOL_FRAGMENTPROTO_H
#define BADVPN_PROTOCOL_FRAGMENTPROTO_H

#include <stdint.h>

#include <misc/balign.h>

typedef uint16_t fragmentproto_frameid;

/**
 * FragmentProto chunk header.
 */
struct fragmentproto_chunk_header {
    /**
     * Identifier of the frame this chunk belongs to.
     * Frames should be given ascending identifiers as they are encoded
     * into chunks (except when the ID wraps to zero).
     */
    fragmentproto_frameid frame_id;
    
    /**
     * Position in the frame where this chunk starts.
     */
    uint16_t chunk_start;
    
    /**
     * Length of the chunk's payload.
     */
    uint16_t chunk_len;
    
    /**
     * Whether this is the last chunk of the frame, i.e.
     * the total length of the frame is chunk_start + chunk_len.
     */
    uint8_t is_last;
} __attribute__((packed));

/**
 * Calculates how many chunks are needed at most for encoding one frame of the
 * given maximum size with FragmentProto onto a carrier with a given MTU.
 * This includes the case when the first chunk of a frame is not the first chunk
 * in a FragmentProto packet.
 * 
 * @param carrier_mtu MTU of the carrier, i.e. maximum length of FragmentProto packets. Must be >sizeof(struct fragmentproto_chunk_header).
 * @param frame_mtu maximum frame size
 * @return maximum number of chunks needed. Will be >0.
 */
static int fragmentproto_max_chunks_for_frame (int carrier_mtu, int frame_mtu)
{
    ASSERT(carrier_mtu > sizeof(struct fragmentproto_chunk_header))
    
    return (BDIVIDE_UP(frame_mtu, (carrier_mtu - sizeof(struct fragmentproto_chunk_header))) + 1);
}

#endif
