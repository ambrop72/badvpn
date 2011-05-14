/**
 * @file LineBuffer.h
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

#ifndef BADVPN_FLOW_LINEBUFFER_H
#define BADVPN_FLOW_LINEBUFFER_H

#include <stdint.h>

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <flow/StreamRecvInterface.h>
#include <flow/PacketPassInterface.h>

typedef struct {
    StreamRecvInterface *input;
    PacketPassInterface *output;
    int buf_size;
    uint8_t nl_char;
    int buf_used;
    uint8_t *buf;
    int buf_consumed;
    DebugObject d_obj;
} LineBuffer;

int LineBuffer_Init (LineBuffer *o, StreamRecvInterface *input, PacketPassInterface *output, int buf_size, uint8_t nl_char) WARN_UNUSED;
void LineBuffer_Free (LineBuffer *o);

#endif
