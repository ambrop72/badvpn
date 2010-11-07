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
    ASSERT(i->d_user_busy)
    ASSERT(i->handler_done)
    DebugObject_Access(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_AmOut(&i->d_in_operation);
    DebugIn_AmOut(&i->d_in_done);
    #endif
    
    // call operation handler
    #ifndef NDEBUG
    DebugIn_GoIn(&i->d_in_operation);
    DEAD_ENTER(i->d_dead)
    #endif
    i->handler_operation(i->user_provider, i->buf);
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->d_dead)) {
        return;
    }
    DebugIn_GoOut(&i->d_in_operation);
    #endif
}

void _PacketRecvInterface_job_done (PacketRecvInterface *i)
{
    ASSERT(i->d_user_busy)
    ASSERT(i->done_len >= 0)
    ASSERT(i->done_len <= i->mtu)
    ASSERT(i->handler_done)
    DebugObject_Access(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_AmOut(&i->d_in_operation);
    DebugIn_AmOut(&i->d_in_done);
    #endif
    
    #ifndef NDEBUG
    i->d_user_busy = 0;
    #endif
    
    // call done handler
    #ifndef NDEBUG
    DebugIn_GoIn(&i->d_in_done);
    DEAD_ENTER(i->d_dead)
    #endif
    i->handler_done(i->user_user, i->done_len);
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->d_dead)) {
        return;
    }
    DebugIn_GoOut(&i->d_in_done);
    #endif
}
