/**
 * @file SCOutmsgEncoder.c
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

#include <stddef.h>
#include <limits.h>

#include <misc/balign.h>
#include <misc/debug.h>
#include <misc/byteorder.h>

#include "SCOutmsgEncoder.h"

static void output_handler_recv (SCOutmsgEncoder *o, uint8_t *data)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(!o->output_packet)
    ASSERT(data)
    
    // schedule receive
    o->output_packet = data;
    PacketRecvInterface_Receiver_Recv(o->input, o->output_packet + SCOUTMSG_OVERHEAD);
}

static void input_handler_done (SCOutmsgEncoder *o, int in_len)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->output_packet)
    
    // write SC header
    struct sc_header *header = (struct sc_header *)o->output_packet;
    header->type = htol8(SCID_OUTMSG);
    
    // write outmsg
    struct sc_client_outmsg *outmsg = (struct sc_client_outmsg *)(header + 1);
    outmsg->clientid = htol16(o->peer_id);
    
    // finish output packet
    o->output_packet = NULL;
    PacketRecvInterface_Done(&o->output, SCOUTMSG_OVERHEAD + in_len);
}

void SCOutmsgEncoder_Init (SCOutmsgEncoder *o, peerid_t peer_id, PacketRecvInterface *input, BPendingGroup *pg)
{
    ASSERT(PacketRecvInterface_GetMTU(input) <= INT_MAX - SCOUTMSG_OVERHEAD)
    
    // init arguments
    o->peer_id = peer_id;
    o->input = input;
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    // init output
    PacketRecvInterface_Init(&o->output, SCOUTMSG_OVERHEAD + PacketRecvInterface_GetMTU(o->input), (PacketRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    // set no output packet
    o->output_packet = NULL;
    
    DebugObject_Init(&o->d_obj);
}

void SCOutmsgEncoder_Free (SCOutmsgEncoder *o)
{
    DebugObject_Free(&o->d_obj);

    // free input
    PacketRecvInterface_Free(&o->output);
}

PacketRecvInterface * SCOutmsgEncoder_GetOutput (SCOutmsgEncoder *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}
