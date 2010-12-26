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

#include <misc/debug.h>
#include <system/DebugObject.h>
#include <system/BPending.h>

#define PPI_STATE_NONE 1
#define PPI_STATE_OPERATION_PENDING 2
#define PPI_STATE_BUSY 3
#define PPI_STATE_DONE_PENDING 4

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
    
    // operation job
    BPending job_operation;
    uint8_t *job_operation_data;
    int job_operation_len;
    
    // done job
    BPending job_done;
    
    // state
    int state;
    
    DebugObject d_obj;
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
    
    // set state
    i->state = PPI_STATE_NONE;
    
    DebugObject_Init(&i->d_obj);
}

void PacketPassInterface_Free (PacketPassInterface *i)
{
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
    ASSERT(i->state == PPI_STATE_BUSY)
    DebugObject_Access(&i->d_obj);
    
    // schedule done
    BPending_Set(&i->job_done);
    
    // set state
    i->state = PPI_STATE_DONE_PENDING;
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
    ASSERT(i->state == PPI_STATE_NONE)
    ASSERT(i->handler_done)
    DebugObject_Access(&i->d_obj);
    
    // schedule operation
    i->job_operation_data = data;
    i->job_operation_len = data_len;
    BPending_Set(&i->job_operation);
    
    // set state
    i->state = PPI_STATE_OPERATION_PENDING;
}

void PacketPassInterface_Sender_Cancel (PacketPassInterface *i)
{
    ASSERT(i->state == PPI_STATE_OPERATION_PENDING || i->state == PPI_STATE_BUSY || i->state == PPI_STATE_DONE_PENDING)
    ASSERT(i->handler_cancel)
    DebugObject_Access(&i->d_obj);
    
    int prev_state = i->state;
    
    // unset jobs
    BPending_Unset(&i->job_operation);
    BPending_Unset(&i->job_done);
    
    // set state
    i->state = PPI_STATE_NONE;
    
    if (prev_state == PPI_STATE_BUSY) {
        i->handler_cancel(i->user_provider);
        return;
    }
}

int PacketPassInterface_HasCancel (PacketPassInterface *i)
{
    DebugObject_Access(&i->d_obj);
    
    return !!i->handler_cancel;
}

#endif
