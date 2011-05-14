/**
 * @file DataProtoKeepaliveSource.c
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

#include <protocol/dataproto.h>
#include <misc/byteorder.h>

#include "DataProtoKeepaliveSource.h"

static void output_handler_recv (DataProtoKeepaliveSource *o, uint8_t *data)
{
    DebugObject_Access(&o->d_obj);
    
    struct dataproto_header *header = (struct dataproto_header *)data;
    header->flags = htol8(0);
    header->from_id = htol16(0);
    header->num_peer_ids = htol16(0);
    
    // finish packet
    PacketRecvInterface_Done(&o->output, sizeof(struct dataproto_header));
}

void DataProtoKeepaliveSource_Init (DataProtoKeepaliveSource *o, BPendingGroup *pg)
{
    // init output
    PacketRecvInterface_Init(&o->output, sizeof(struct dataproto_header), (PacketRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    DebugObject_Init(&o->d_obj);
}

void DataProtoKeepaliveSource_Free (DataProtoKeepaliveSource *o)
{
    DebugObject_Free(&o->d_obj);

    // free output
    PacketRecvInterface_Free(&o->output);
}

PacketRecvInterface * DataProtoKeepaliveSource_GetOutput (DataProtoKeepaliveSource *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}
