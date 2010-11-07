/**
 * @file PacketRecvInterface.h
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
 * Interface allowing a packet receiver to receive data packets from a packet sender.
 */

#ifndef BADVPN_FLOW_PACKETRECVINTERFACE_H
#define BADVPN_FLOW_PACKETRECVINTERFACE_H

#include <stdint.h>
#include <stddef.h>

#include <misc/dead.h>
#include <misc/debug.h>
#include <misc/debugin.h>
#include <system/DebugObject.h>
#include <system/BPending.h>

typedef void (*PacketRecvInterface_handler_recv) (void *user, uint8_t *data);

typedef void (*PacketRecvInterface_handler_done) (void *user, int data_len);

typedef struct {
    // provider data
    int mtu;
    PacketRecvInterface_handler_recv handler_operation;
    void *user_provider;

    // user data
    PacketRecvInterface_handler_done handler_done;
    void *user_user;
    
    // jobs
    BPending job_operation;
    BPending job_done;
    
    // buffer supplied by user
    uint8_t *buf;
    
    // length supplied by done
    int done_len;
    
    DebugObject d_obj;
    #ifndef NDEBUG
    DebugIn d_in_operation;
    DebugIn d_in_done;
    dead_t d_dead;
    int d_user_busy;
    #endif
} PacketRecvInterface;

static void PacketRecvInterface_Init (PacketRecvInterface *i, int mtu, PacketRecvInterface_handler_recv handler_operation, void *user, BPendingGroup *pg);

static void PacketRecvInterface_Free (PacketRecvInterface *i);

static void PacketRecvInterface_Done (PacketRecvInterface *i, int data_len);

static int PacketRecvInterface_GetMTU (PacketRecvInterface *i);

static void PacketRecvInterface_Receiver_Init (PacketRecvInterface *i, PacketRecvInterface_handler_done handler_done, void *user);

static void PacketRecvInterface_Receiver_Recv (PacketRecvInterface *i, uint8_t *data);

#ifndef NDEBUG

static int PacketRecvInterface_InClient (PacketRecvInterface *i);

static int PacketRecvInterface_InDone (PacketRecvInterface *i);

#endif

void _PacketRecvInterface_job_operation (PacketRecvInterface *i);
void _PacketRecvInterface_job_done (PacketRecvInterface *i);

void PacketRecvInterface_Init (PacketRecvInterface *i, int mtu, PacketRecvInterface_handler_recv handler_operation, void *user, BPendingGroup *pg)
{
    ASSERT(mtu >= 0)
    
    // init arguments
    i->mtu = mtu;
    i->handler_operation = handler_operation;
    i->user_provider = user;
    
    // set no user
    i->handler_done = NULL;
    
    // init jobs
    BPending_Init(&i->job_operation, pg, (BPending_handler)_PacketRecvInterface_job_operation, i);
    BPending_Init(&i->job_done, pg, (BPending_handler)_PacketRecvInterface_job_done, i);
    
    DebugObject_Init(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_Init(&i->d_in_operation);
    DebugIn_Init(&i->d_in_done);
    DEAD_INIT(i->d_dead);
    i->d_user_busy = 0;
    #endif
}

void PacketRecvInterface_Free (PacketRecvInterface *i)
{
    #ifndef NDEBUG
    DEAD_KILL(i->d_dead);
    #endif
    DebugObject_Free(&i->d_obj);
    
    // free jobs
    BPending_Free(&i->job_done);
    BPending_Free(&i->job_operation);
}

void PacketRecvInterface_Done (PacketRecvInterface *i, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= i->mtu)
    ASSERT(i->d_user_busy)
    ASSERT(i->handler_done)
    ASSERT(!BPending_IsSet(&i->job_operation))
    DebugObject_Access(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_AmOut(&i->d_in_done);
    #endif
    
    i->done_len = data_len;
    
    BPending_Set(&i->job_done);
}

int PacketRecvInterface_GetMTU (PacketRecvInterface *i)
{
    DebugObject_Access(&i->d_obj);
    
    return i->mtu;
}

void PacketRecvInterface_Receiver_Init (PacketRecvInterface *i, PacketRecvInterface_handler_done handler_done, void *user)
{
    ASSERT(handler_done)
    ASSERT(!i->handler_done)
    DebugObject_Access(&i->d_obj);
    
    i->handler_done = handler_done;
    i->user_user = user;
}

void PacketRecvInterface_Receiver_Recv (PacketRecvInterface *i, uint8_t *data)
{
    ASSERT(!(i->mtu > 0) || data)
    ASSERT(!i->d_user_busy)
    ASSERT(i->handler_done)
    DebugObject_Access(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_AmOut(&i->d_in_operation);
    #endif
    
    BPending_Set(&i->job_operation);
    
    i->buf = data;
    
    #ifndef NDEBUG
    i->d_user_busy = 1;
    #endif
}

#ifndef NDEBUG

int PacketRecvInterface_InClient (PacketRecvInterface *i)
{
    DebugObject_Access(&i->d_obj);
    
    return DebugIn_In(&i->d_in_operation);
}

int PacketRecvInterface_InDone (PacketRecvInterface *i)
{
    DebugObject_Access(&i->d_obj);
    
    return DebugIn_In(&i->d_in_done);
}

#endif

#endif
