/**
 * @file SimpleStreamBuffer.h
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

#ifndef BADVPN_SIMPLESTREAMBUFFER_H
#define BADVPN_SIMPLESTREAMBUFFER_H

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <flow/StreamRecvInterface.h>

typedef struct {
    int buf_size;
    StreamRecvInterface output;
    uint8_t *buf;
    int buf_used;
    uint8_t *output_data;
    int output_data_len;
    DebugObject d_obj;
} SimpleStreamBuffer;

int SimpleStreamBuffer_Init (SimpleStreamBuffer *o, int buf_size, BPendingGroup *pg) WARN_UNUSED;
void SimpleStreamBuffer_Free (SimpleStreamBuffer *o);
StreamRecvInterface * SimpleStreamBuffer_GetOutput (SimpleStreamBuffer *o);
int SimpleStreamBuffer_Write (SimpleStreamBuffer *o, uint8_t *data, int data_len) WARN_UNUSED;

#endif
