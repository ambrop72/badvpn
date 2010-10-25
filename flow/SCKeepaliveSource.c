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

#include <flow/SCKeepaliveSource.h>

static int output_handler_recv (SCKeepaliveSource *o, uint8_t *data, int *data_len)
{
    struct sc_header *header = (struct sc_header *)data;
    header->type = SCID_KEEPALIVE;
    
    *data_len = sizeof(struct sc_header);
    return 1;
}

void SCKeepaliveSource_Init (SCKeepaliveSource *o)
{
    // init output
    PacketRecvInterface_Init(&o->output, sizeof(struct sc_header), (PacketRecvInterface_handler_recv)output_handler_recv, o);
    
    // init debug object
    DebugObject_Init(&o->d_obj);
}

void SCKeepaliveSource_Free (SCKeepaliveSource *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);

    // free output
    PacketRecvInterface_Free(&o->output);
}

PacketRecvInterface * SCKeepaliveSource_GetOutput (SCKeepaliveSource *o)
{
    return &o->output;
}
