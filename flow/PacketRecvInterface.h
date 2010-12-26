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

#include <misc/debug.h>
#include <system/DebugObject.h>
#include <system/BPending.h>

#define PRI_STATE_NONE 1
#define PRI_STATE_OPERATION_PENDING 2
#define PRI_STATE_BUSY 3
#define PRI_STATE_DONE_PENDING 4

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
    
    // operation job
    BPending job_operation;
    uint8_t *job_operation_data;
    
    // done job
    BPending job_done;
    int job_done_len;
    
    // state
    int state;
    
    DebugObject d_obj;
} PacketRecvInterface;

static void PacketRecvInterface_Init (PacketRecvInterface *i, int mtu, PacketRecvInterface_handler_recv handler_operation, void *user, BPendingGroup *pg);

static void PacketRecvInterface_Free (PacketRecvInterface *i);

static void PacketRecvInterface_Done (PacketRecvInterface *i, int data_len);

static int PacketRecvInterface_GetMTU (PacketRecvInterface *i);

static void PacketRecvInterface_Receiver_Init (PacketRecvInterface *i, PacketRecvInterface_handler_done handler_done, void *user);

static void PacketRecvInterface_Receiver_Recv (PacketRecvInterface *i, uint8_t *data);

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
    
    // set state
    i->state = PRI_STATE_NONE;
    
    DebugObject_Init(&i->d_obj);
}

void PacketRecvInterface_Free (PacketRecvInterface *i)
{
    DebugObject_Free(&i->d_obj);
    
    // free jobs
    BPending_Free(&i->job_done);
    BPending_Free(&i->job_operation);
}

void PacketRecvInterface_Done (PacketRecvInterface *i, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= i->mtu)
    ASSERT(i->state == PRI_STATE_BUSY)
    DebugObject_Access(&i->d_obj);
    
    // schedule done
    i->job_done_len = data_len;
    BPending_Set(&i->job_done);
    
    // set state
    i->state = PRI_STATE_DONE_PENDING;
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
    ASSERT(i->state == PRI_STATE_NONE)
    ASSERT(i->handler_done)
    DebugObject_Access(&i->d_obj);
    
    // schedule operation
    i->job_operation_data = data;
    BPending_Set(&i->job_operation);
    
    // set state
    i->state = PRI_STATE_OPERATION_PENDING;
}

#endif
