/**
 * @file PacketPassInterface.c
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

#include <flow/PacketPassInterface.h>

void _PacketPassInterface_job_operation (PacketPassInterface *i)
{
    ASSERT(i->state == PPI_STATE_OPERATION_PENDING)
    DebugObject_Access(&i->d_obj);
    
    // set state
    i->state = PPI_STATE_BUSY;
    
    // call handler
    i->handler_operation(i->user_provider, i->job_operation_data, i->job_operation_len);
    return;
}

void _PacketPassInterface_job_requestcancel (PacketPassInterface *i)
{
    ASSERT(i->state == PPI_STATE_BUSY)
    ASSERT(i->cancel_requested)
    ASSERT(i->handler_requestcancel)
    DebugObject_Access(&i->d_obj);
    
    // call handler
    i->handler_requestcancel(i->user_provider);
    return;
}

void _PacketPassInterface_job_done (PacketPassInterface *i)
{
    ASSERT(i->state == PPI_STATE_DONE_PENDING)
    DebugObject_Access(&i->d_obj);
    
    // set state
    i->state = PPI_STATE_NONE;
    
    // call handler
    i->handler_done(i->user_user);
    return;
}
