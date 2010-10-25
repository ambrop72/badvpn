/*
    Circular packet buffer
    Copyright (C) Ambroz Bizjak, 2009

    This file is part of BadVPN.

    BadVPN is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.

    BadVPN is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef BADVPN_STRUCTURE_CHUNKBUFFER2_H
#define BADVPN_STRUCTURE_CHUNKBUFFER2_H

#include <stdint.h>
#include <stdlib.h>

#include <misc/balign.h>
#include <misc/debug.h>

#ifndef NDEBUG
#define CHUNKBUFFER2_ASSERT_BUFFER(_buf) _ChunkBuffer2_assert_buffer(_buf);
#define CHUNKBUFFER2_ASSERT_IO(_buf) _ChunkBuffer2_assert_io(_buf);
#else
#define CHUNKBUFFER2_ASSERT_BUFFER(_buf)
#define CHUNKBUFFER2_ASSERT_IO(_buf)
#endif

struct ChunkBuffer2_block {
    int len;
};

typedef struct {
    struct ChunkBuffer2_block *buffer;
    int size;
    int wrap;
    int start;
    int used;
    int mtu;
    uint8_t *input_dest;
    int input_avail;
    uint8_t *output_dest;
    int output_avail;
} ChunkBuffer2;

// calculates a buffer size needed to hold at least 'cnum' packets long at least 'clen'
#define CHUNKBUFFER2_MAKE_NUMBLOCKS(_clen, _cnum) \
    ( \
        (1 + BDIVIDE_UP((_clen), sizeof(struct ChunkBuffer2_block))) * \
        ((_cnum) + 1) \
    )

// initialize
static void ChunkBuffer2_Init (ChunkBuffer2 *buf, struct ChunkBuffer2_block *buffer, int blocks, int mtu);

// submit a packet written to the buffer
static void ChunkBuffer2_SubmitPacket (ChunkBuffer2 *buf, int len);

// remove the first packet
static void ChunkBuffer2_ConsumePacket (ChunkBuffer2 *buf);

static int _ChunkBuffer2_end (ChunkBuffer2 *buf)
{
    if (buf->used >= buf->wrap - buf->start) {
        return (buf->used - (buf->wrap - buf->start));
    } else {
        return (buf->start + buf->used);
    }
}

static void _ChunkBuffer2_assert_buffer (ChunkBuffer2 *buf)
{
    ASSERT(buf->size > 0)
    ASSERT(buf->wrap > 0)
    ASSERT(buf->wrap <= buf->size)
    ASSERT(buf->start >= 0)
    ASSERT(buf->start < buf->wrap)
    ASSERT(buf->used >= 0)
    ASSERT(buf->used <= buf->wrap)
    ASSERT(buf->wrap == buf->size || buf->used >= buf->wrap - buf->start)
    ASSERT(buf->mtu >= 0)
}

static void _ChunkBuffer2_assert_io (ChunkBuffer2 *buf)
{
    // check input
    
    int end = _ChunkBuffer2_end(buf);
    
    if (buf->size - end - 1 < buf->mtu) {
        // it will never be possible to write a MTU long packet here
        ASSERT(!buf->input_dest)
        ASSERT(buf->input_avail == -1)
    } else {
        // calculate number of free blocks
        int free;
        if (buf->used >= buf->wrap - buf->start) {
            free = buf->start - end;
        } else {
            free = buf->size - end;
        }
        
        if (free > 0) {
            // got space at least for a header. More space will become available as packets are
            // read from the buffer, up to MTU.
            ASSERT(buf->input_dest == (uint8_t *)&buf->buffer[end + 1])
            ASSERT(buf->input_avail == (free - 1) * sizeof(struct ChunkBuffer2_block))
        } else {
            // no space
            ASSERT(!buf->input_dest)
            ASSERT(buf->input_avail == -1)
        }
    }
    
    // check output
    
    if (buf->used > 0) {
        int datalen = buf->buffer[buf->start].len;
        ASSERT(datalen >= 0)
        int blocklen = BDIVIDE_UP(datalen, sizeof(struct ChunkBuffer2_block));
        ASSERT(blocklen <= buf->used - 1)
        ASSERT(blocklen <= buf->wrap - buf->start - 1)
        ASSERT(buf->output_dest == (uint8_t *)&buf->buffer[buf->start + 1])
        ASSERT(buf->output_avail == datalen)
    } else {
        ASSERT(!buf->output_dest)
        ASSERT(buf->output_avail == -1)
    }
}

static void _ChunkBuffer2_update_input (ChunkBuffer2 *buf)
{
    int end = _ChunkBuffer2_end(buf);
    
    if (buf->size - end - 1 < buf->mtu) {
        // it will never be possible to write a MTU long packet here
        buf->input_dest = NULL;
        buf->input_avail = -1;
        return;
    }
    
    // calculate number of free blocks
    int free;
    if (buf->used >= buf->wrap - buf->start) {
        free = buf->start - end;
    } else {
        free = buf->size - end;
    }
    
    if (free > 0) {
        // got space at least for a header. More space will become available as packets are
        // read from the buffer, up to MTU.
        buf->input_dest = (uint8_t *)&buf->buffer[end + 1];
        buf->input_avail = (free - 1) * sizeof(struct ChunkBuffer2_block);
    } else {
        // no space
        buf->input_dest = NULL;
        buf->input_avail = -1;
    }
}

static void _ChunkBuffer2_update_output (ChunkBuffer2 *buf)
{
    if (buf->used > 0) {
        int datalen = buf->buffer[buf->start].len;
        ASSERT(datalen >= 0)
        int blocklen = BDIVIDE_UP(datalen, sizeof(struct ChunkBuffer2_block));
        ASSERT(blocklen <= buf->used - 1)
        ASSERT(blocklen <= buf->wrap - buf->start - 1)
        buf->output_dest = (uint8_t *)&buf->buffer[buf->start + 1];
        buf->output_avail = datalen;
    } else {
        buf->output_dest = NULL;
        buf->output_avail = -1;
    }
}

void ChunkBuffer2_Init (ChunkBuffer2 *buf, struct ChunkBuffer2_block *buffer, int blocks, int mtu)
{
    ASSERT(blocks > 0)
    ASSERT(mtu >= 0)
    
    buf->buffer = buffer;
    buf->size = blocks;
    buf->wrap = blocks;
    buf->start = 0;
    buf->used = 0;
    buf->mtu = BDIVIDE_UP(mtu, sizeof(struct ChunkBuffer2_block));
    
    CHUNKBUFFER2_ASSERT_BUFFER(buf)
    
    _ChunkBuffer2_update_input(buf);
    _ChunkBuffer2_update_output(buf);
    
    CHUNKBUFFER2_ASSERT_IO(buf)
}

void ChunkBuffer2_SubmitPacket (ChunkBuffer2 *buf, int len)
{
    ASSERT(buf->input_dest)
    ASSERT(len >= 0)
    ASSERT(len <= buf->input_avail)
    
    CHUNKBUFFER2_ASSERT_BUFFER(buf)
    CHUNKBUFFER2_ASSERT_IO(buf)
    
    int end = _ChunkBuffer2_end(buf);
    int blocklen = BDIVIDE_UP(len, sizeof(struct ChunkBuffer2_block));
    
    ASSERT(blocklen <= buf->size - end - 1)
    ASSERT(buf->used < buf->wrap - buf->start || blocklen <= buf->start - end - 1)
    
    buf->buffer[end].len = len;
    buf->used += 1 + blocklen;
    
    if (buf->used <= buf->wrap - buf->start && buf->mtu > buf->size - (end + 1 + blocklen) - 1) {
        buf->wrap = end + 1 + blocklen;
    }
    
    CHUNKBUFFER2_ASSERT_BUFFER(buf)
    
    // update input
    _ChunkBuffer2_update_input(buf);
    
    // update output
    if (buf->used == 1 + blocklen) {
        _ChunkBuffer2_update_output(buf);
    }
    
    CHUNKBUFFER2_ASSERT_IO(buf)
}

void ChunkBuffer2_ConsumePacket (ChunkBuffer2 *buf)
{
    ASSERT(buf->output_dest)
    
    CHUNKBUFFER2_ASSERT_BUFFER(buf)
    CHUNKBUFFER2_ASSERT_IO(buf)
    
    ASSERT(1 <= buf->wrap - buf->start)
    ASSERT(1 <= buf->used)
    
    int blocklen = BDIVIDE_UP(buf->buffer[buf->start].len, sizeof(struct ChunkBuffer2_block));
    
    ASSERT(blocklen <= buf->wrap - buf->start - 1)
    ASSERT(blocklen <= buf->used - 1)
    
    int data_wrapped = (buf->used >= buf->wrap - buf->start);
    
    buf->start += 1 + blocklen;
    buf->used -= 1 + blocklen;
    if (buf->start == buf->wrap) {
        buf->start = 0;
        buf->wrap = buf->size;
    }
    
    CHUNKBUFFER2_ASSERT_BUFFER(buf)
    
    // update input
    if (data_wrapped) {
        _ChunkBuffer2_update_input(buf);
    }
    
    // update output
    _ChunkBuffer2_update_output(buf);
    
    CHUNKBUFFER2_ASSERT_IO(buf)
}

#endif
