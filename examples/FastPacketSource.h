/**
 * @file FastPacketSource.h
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

#ifndef _FASTPACKETSOURCE_H
#define _FASTPACKETSOURCE_H

#include <stdint.h>
#include <string.h>

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <flow/PacketPassInterface.h>

typedef struct {
    PacketPassInterface *output;
    int psize;
    uint8_t *data;
    int data_len;
    DebugObject d_obj;
} FastPacketSource;

static void _FastPacketSource_output_handler_done (FastPacketSource *s)
{
    DebugObject_Access(&s->d_obj);
    
    PacketPassInterface_Sender_Send(s->output, s->data, s->data_len);
}

static void FastPacketSource_Init (FastPacketSource *s, PacketPassInterface *output, uint8_t *data, int data_len, BPendingGroup *pg)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= PacketPassInterface_GetMTU(output));
    
    // init arguments
    s->output = output;
    s->data = data;
    s->data_len = data_len;
    
    // init output
    PacketPassInterface_Sender_Init(s->output, (PacketPassInterface_handler_done)_FastPacketSource_output_handler_done, s);
    
    // schedule send
    PacketPassInterface_Sender_Send(s->output, s->data, s->data_len);
    
    DebugObject_Init(&s->d_obj);
}

static void FastPacketSource_Free (FastPacketSource *s)
{
    DebugObject_Free(&s->d_obj);
}

#endif
