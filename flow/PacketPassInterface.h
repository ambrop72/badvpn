/**
 * @file PacketPassInterface.h
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
 * 
 * @section DESCRIPTION
 * 
 * Interface allowing a packet sender to pass data packets to a packet receiver.
 */

#ifndef BADVPN_FLOW_PACKETPASSINTERFACE_H
#define BADVPN_FLOW_PACKETPASSINTERFACE_H

#include <stdint.h>
#include <stddef.h>

#include <misc/dead.h>
#include <misc/debug.h>
#include <misc/debugin.h>
#include <system/DebugObject.h>
#include <system/BPending.h>

typedef void (*PacketPassInterface_handler_send) (void *user, uint8_t *data, int data_len);

typedef void (*PacketPassInterface_handler_cancel) (void *user);

typedef void (*PacketPassInterface_handler_done) (void *user);

typedef struct {
    // provider data
    int mtu;
    PacketPassInterface_handler_send handler_operation;
    PacketPassInterface_handler_cancel handler_cancel;
    void *user_provider;
    
    // user data
    PacketPassInterface_handler_done handler_done;
    void *user_user;
    
    // jobs
    BPending job_operation;
    BPending job_done;
    
    // packet supplied by user
    uint8_t *buf;
    int buf_len;
    
    DebugObject d_obj;
    #ifndef NDEBUG
    DebugIn d_in_operation;
    DebugIn d_in_cancel;
    DebugIn d_in_done;
    dead_t d_dead;
    int d_user_busy;
    #endif
} PacketPassInterface;

static void PacketPassInterface_Init (PacketPassInterface *i, int mtu, PacketPassInterface_handler_send handler_operation, void *user, BPendingGroup *pg);

static void PacketPassInterface_Free (PacketPassInterface *i);

static void PacketPassInterface_EnableCancel (PacketPassInterface *i, PacketPassInterface_handler_cancel handler_cancel);

static void PacketPassInterface_Done (PacketPassInterface *i);

static int PacketPassInterface_GetMTU (PacketPassInterface *i);

static void PacketPassInterface_Sender_Init (PacketPassInterface *i, PacketPassInterface_handler_done handler_done, void *user);

static void PacketPassInterface_Sender_Send (PacketPassInterface *i, uint8_t *data, int data_len);

static void PacketPassInterface_Sender_Cancel (PacketPassInterface *i);

static int PacketPassInterface_HasCancel (PacketPassInterface *i);

#ifndef NDEBUG

static int PacketPassInterface_InClient (PacketPassInterface *i);

static int PacketPassInterface_InDone (PacketPassInterface *i);

#endif

void _PacketPassInterface_job_operation (PacketPassInterface *i);
void _PacketPassInterface_job_done (PacketPassInterface *i);

void PacketPassInterface_Init (PacketPassInterface *i, int mtu, PacketPassInterface_handler_send handler_operation, void *user, BPendingGroup *pg)
{
    ASSERT(mtu >= 0)
    
    // init arguments
    i->mtu = mtu;
    i->handler_operation = handler_operation;
    i->handler_cancel = NULL;
    i->user_provider = user;
    
    // set no user
    i->handler_done = NULL;
    
    // init jobs
    BPending_Init(&i->job_operation, pg, (BPending_handler)_PacketPassInterface_job_operation, i);
    BPending_Init(&i->job_done, pg, (BPending_handler)_PacketPassInterface_job_done, i);
    
    DebugObject_Init(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_Init(&i->d_in_operation);
    DebugIn_Init(&i->d_in_cancel);
    DebugIn_Init(&i->d_in_done);
    DEAD_INIT(i->d_dead);
    i->d_user_busy = 0;
    #endif
}

void PacketPassInterface_Free (PacketPassInterface *i)
{
    #ifndef NDEBUG
    DEAD_KILL(i->d_dead);
    #endif
    DebugObject_Free(&i->d_obj);
    
    // free jobs
    BPending_Free(&i->job_done);
    BPending_Free(&i->job_operation);
}

void PacketPassInterface_EnableCancel (PacketPassInterface *i, PacketPassInterface_handler_cancel handler_cancel)
{
    ASSERT(!i->handler_cancel)
    ASSERT(!i->handler_done)
    ASSERT(handler_cancel)
    
    i->handler_cancel = handler_cancel;
}

void PacketPassInterface_Done (PacketPassInterface *i)
{
    ASSERT(i->d_user_busy)
    ASSERT(i->buf_len >= 0)
    ASSERT(i->buf_len <= i->mtu)
    ASSERT(i->handler_done)
    ASSERT(!BPending_IsSet(&i->job_operation))
    DebugObject_Access(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_AmOut(&i->d_in_cancel);
    DebugIn_AmOut(&i->d_in_done);
    #endif
    
    BPending_Set(&i->job_done);
}

int PacketPassInterface_GetMTU (PacketPassInterface *i)
{
    DebugObject_Access(&i->d_obj);
    
    return i->mtu;
}

void PacketPassInterface_Sender_Init (PacketPassInterface *i, PacketPassInterface_handler_done handler_done, void *user)
{
    ASSERT(handler_done)
    ASSERT(!i->handler_done)
    DebugObject_Access(&i->d_obj);
    
    i->handler_done = handler_done;
    i->user_user = user;
}

void PacketPassInterface_Sender_Send (PacketPassInterface *i, uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= i->mtu)
    ASSERT(!(data_len > 0) || data)
    ASSERT(!i->d_user_busy)
    ASSERT(i->handler_done)
    DebugObject_Access(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_AmOut(&i->d_in_operation);
    DebugIn_AmOut(&i->d_in_cancel);
    #endif
    
    i->buf = data;
    i->buf_len = data_len;
    
    #ifndef NDEBUG
    i->d_user_busy = 1;
    #endif
    
    BPending_Set(&i->job_operation);
}

void PacketPassInterface_Sender_Cancel (PacketPassInterface *i)
{
    ASSERT(i->d_user_busy)
    ASSERT(i->buf_len >= 0)
    ASSERT(i->buf_len <= i->mtu)
    ASSERT(i->handler_cancel)
    ASSERT(i->handler_done)
    DebugObject_Access(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_AmOut(&i->d_in_operation);
    DebugIn_AmOut(&i->d_in_cancel);
    #endif
    
    BPending_Unset(&i->job_operation);
    BPending_Unset(&i->job_done);
    
    #ifndef NDEBUG
    i->d_user_busy = 0;
    #endif
    
    if (!BPending_IsSet(&i->job_operation) && !BPending_IsSet(&i->job_done)) {
        #ifndef NDEBUG
        DebugIn_GoIn(&i->d_in_cancel);
        DEAD_ENTER(i->d_dead)
        #endif
        i->handler_cancel(i->user_provider);
        #ifndef NDEBUG
        ASSERT(!DEAD_KILLED)
        DEAD_LEAVE(i->d_dead);
        DebugIn_GoOut(&i->d_in_cancel);
        #endif
    }
}

int PacketPassInterface_HasCancel (PacketPassInterface *i)
{
    DebugObject_Access(&i->d_obj);
    
    return !!i->handler_cancel;
}

#ifndef NDEBUG

int PacketPassInterface_InClient (PacketPassInterface *i)
{
    DebugObject_Access(&i->d_obj);
    
    return DebugIn_In(&i->d_in_operation);
}

int PacketPassInterface_InDone (PacketPassInterface *i)
{
    DebugObject_Access(&i->d_obj);
    
    return DebugIn_In(&i->d_in_done);
}

#endif

#endif
