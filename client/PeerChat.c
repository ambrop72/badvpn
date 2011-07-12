/**
 * @file PeerChat.c
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

#include <string.h>

#include <misc/byteorder.h>

#include "PeerChat.h"

int PeerChat_Init (PeerChat *o, peerid_t peer_id, BPendingGroup *pg, void *user, PeerChat_handler_error handler_error)
{
    // init arguments
    o->user = user;
    o->handler_error = handler_error;
    
    // init copier
    PacketCopier_Init(&o->copier, SC_MAX_MSGLEN, pg);
    
    // init SC encoder
    SCOutmsgEncoder_Init(&o->sc_encoder, peer_id, PacketCopier_GetOutput(&o->copier), pg);
    
    // init PacketProto encoder
    PacketProtoEncoder_Init(&o->pp_encoder, SCOutmsgEncoder_GetOutput(&o->sc_encoder), pg);
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail1:
    PacketProtoEncoder_Free(&o->pp_encoder);
    SCOutmsgEncoder_Free(&o->sc_encoder);
    PacketCopier_Free(&o->copier);
    return 0;
}

void PeerChat_Free (PeerChat *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free PacketProto encoder
    PacketProtoEncoder_Free(&o->pp_encoder);
    
    // free SC encoder
    SCOutmsgEncoder_Free(&o->sc_encoder);
    
    // free copier
    PacketCopier_Free(&o->copier);
}

PacketPassInterface * PeerChat_GetSendInput (PeerChat *o)
{
    DebugObject_Access(&o->d_obj);
    
    return PacketCopier_GetInput(&o->copier);
}

PacketRecvInterface * PeerChat_GetSendOutput (PeerChat *o)
{
    DebugObject_Access(&o->d_obj);
    
    return PacketProtoEncoder_GetOutput(&o->pp_encoder);
}
