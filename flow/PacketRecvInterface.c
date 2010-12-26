/**
 * @file PacketRecvInterface.c
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

#include <flow/PacketRecvInterface.h>

void _PacketRecvInterface_job_operation (PacketRecvInterface *i)
{
    ASSERT(i->state == PRI_STATE_OPERATION_PENDING)
    DebugObject_Access(&i->d_obj);
    
    // set state
    i->state = PRI_STATE_BUSY;
    
    // call handler
    i->handler_operation(i->user_provider, i->job_operation_data);
    return;
}

void _PacketRecvInterface_job_done (PacketRecvInterface *i)
{
    ASSERT(i->state == PRI_STATE_DONE_PENDING)
    DebugObject_Access(&i->d_obj);
    
    // set state
    i->state = PRI_STATE_NONE;
    
    // call handler
    i->handler_done(i->user_user, i->job_done_len);
    return;
}
