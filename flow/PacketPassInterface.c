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
    ASSERT(i->d_user_busy)
    ASSERT(i->buf_len >= 0)
    ASSERT(i->buf_len <= i->mtu)
    ASSERT(i->handler_done)
    DebugObject_Access(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_AmOut(&i->d_in_operation);
    DebugIn_AmOut(&i->d_in_cancel);
    DebugIn_AmOut(&i->d_in_done);
    #endif
    
    // call operation handler
    #ifndef NDEBUG
    DebugIn_GoIn(&i->d_in_operation);
    DEAD_ENTER(i->d_dead)
    #endif
    i->handler_operation(i->user_provider, i->buf, i->buf_len);
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->d_dead)) {
        return;
    }
    DebugIn_GoOut(&i->d_in_operation);
    #endif
}

void _PacketPassInterface_job_done (PacketPassInterface *i)
{
    ASSERT(i->d_user_busy)
    ASSERT(i->buf_len >= 0)
    ASSERT(i->buf_len <= i->mtu)
    ASSERT(i->handler_done)
    DebugObject_Access(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_AmOut(&i->d_in_operation);
    DebugIn_AmOut(&i->d_in_cancel);
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
    i->handler_done(i->user_user);
    #ifndef NDEBUG
    if (DEAD_LEAVE(i->d_dead)) {
        return;
    }
    DebugIn_GoOut(&i->d_in_done);
    #endif
}
