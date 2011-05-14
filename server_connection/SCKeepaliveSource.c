/**
 * @file SCKeepaliveSource.c
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

#include <protocol/scproto.h>
#include <misc/byteorder.h>

#include "SCKeepaliveSource.h"

static void output_handler_recv (SCKeepaliveSource *o, uint8_t *data)
{
    DebugObject_Access(&o->d_obj);
    
    struct sc_header *header = (struct sc_header *)data;
    header->type = htol8(SCID_KEEPALIVE);
    
    PacketRecvInterface_Done(&o->output, sizeof(struct sc_header));
}

void SCKeepaliveSource_Init (SCKeepaliveSource *o, BPendingGroup *pg)
{
    // init output
    PacketRecvInterface_Init(&o->output, sizeof(struct sc_header), (PacketRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    DebugObject_Init(&o->d_obj);
}

void SCKeepaliveSource_Free (SCKeepaliveSource *o)
{
    DebugObject_Free(&o->d_obj);

    // free output
    PacketRecvInterface_Free(&o->output);
}

PacketRecvInterface * SCKeepaliveSource_GetOutput (SCKeepaliveSource *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}
