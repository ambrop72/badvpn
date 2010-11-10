/**
 * @file StreamRecvInterface.h
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
 * Interface allowing a stream receiver to receive stream data from a stream sender.
 * 
 * Note that this interface behaves exactly the same and has the same code as
 * {@link StreamRecvInterface} if names and its external semantics are disregarded.
 * If you modify this file, you should probably modify {@link StreamRecvInterface}
 * too.
 */

#ifndef BADVPN_FLOW_STREAMRECVINTERFACE_H
#define BADVPN_FLOW_STREAMRECVINTERFACE_H

#include <stdint.h>
#include <stddef.h>

#include <misc/dead.h>
#include <misc/debug.h>
#include <misc/debugin.h>
#include <system/DebugObject.h>
#include <system/BPending.h>

typedef void (*StreamRecvInterface_handler_recv) (void *user, uint8_t *data, int data_len);

typedef void (*StreamRecvInterface_handler_done) (void *user, int data_len);

typedef struct {
    // provider data
    StreamRecvInterface_handler_recv handler_operation;
    void *user_provider;
    
    // user data
    StreamRecvInterface_handler_done handler_done;
    void *user_user;
    
    // jobs
    BPending job_operation;
    BPending job_done;
    
    // packet supplied by user
    uint8_t *buf;
    int buf_len;
    
    // length supplied by done
    int done_len;
    
    DebugObject d_obj;
    #ifndef NDEBUG
    DebugIn d_in_operation;
    DebugIn d_in_done;
    dead_t d_dead;
    int d_user_busy;
    #endif
} StreamRecvInterface;

static void StreamRecvInterface_Init (StreamRecvInterface *i, StreamRecvInterface_handler_recv handler_operation, void *user, BPendingGroup *pg);

static void StreamRecvInterface_Free (StreamRecvInterface *i);

static void StreamRecvInterface_Done (StreamRecvInterface *i, int data_len);

static void StreamRecvInterface_Receiver_Init (StreamRecvInterface *i, StreamRecvInterface_handler_done handler_done, void *user);

static void StreamRecvInterface_Receiver_Recv (StreamRecvInterface *i, uint8_t *data, int data_len);

void _StreamRecvInterface_job_operation (StreamRecvInterface *i);
void _StreamRecvInterface_job_done (StreamRecvInterface *i);

void StreamRecvInterface_Init (StreamRecvInterface *i, StreamRecvInterface_handler_recv handler_operation, void *user, BPendingGroup *pg)
{
    // init arguments
    i->handler_operation = handler_operation;
    i->user_provider = user;
    
    // set no user
    i->handler_done = NULL;
    
    // init jobs
    BPending_Init(&i->job_operation, pg, (BPending_handler)_StreamRecvInterface_job_operation, i);
    BPending_Init(&i->job_done, pg, (BPending_handler)_StreamRecvInterface_job_done, i);
    
    DebugObject_Init(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_Init(&i->d_in_operation);
    DebugIn_Init(&i->d_in_done);
    DEAD_INIT(i->d_dead);
    i->d_user_busy = 0;
    #endif
}

void StreamRecvInterface_Free (StreamRecvInterface *i)
{
    #ifndef NDEBUG
    DEAD_KILL(i->d_dead);
    #endif
    DebugObject_Free(&i->d_obj);
    
    // free jobs
    BPending_Free(&i->job_done);
    BPending_Free(&i->job_operation);
}

void StreamRecvInterface_Done (StreamRecvInterface *i, int data_len)
{
    ASSERT(data_len > 0)
    ASSERT(data_len <= i->buf_len)
    ASSERT(i->d_user_busy)
    ASSERT(i->buf_len > 0)
    ASSERT(i->handler_done)
    ASSERT(!BPending_IsSet(&i->job_operation))
    DebugObject_Access(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_AmOut(&i->d_in_done);
    #endif
    
    i->done_len = data_len;
    
    BPending_Set(&i->job_done);
}

void StreamRecvInterface_Receiver_Init (StreamRecvInterface *i, StreamRecvInterface_handler_done handler_done, void *user)
{
    ASSERT(handler_done)
    ASSERT(!i->handler_done)
    DebugObject_Access(&i->d_obj);
    
    i->handler_done = handler_done;
    i->user_user = user;
}

void StreamRecvInterface_Receiver_Recv (StreamRecvInterface *i, uint8_t *data, int data_len)
{
    ASSERT(data_len > 0)
    ASSERT(data)
    ASSERT(!i->d_user_busy)
    ASSERT(i->handler_done)
    DebugObject_Access(&i->d_obj);
    #ifndef NDEBUG
    DebugIn_AmOut(&i->d_in_operation);
    #endif
    
    i->buf = data;
    i->buf_len = data_len;
    
    #ifndef NDEBUG
    i->d_user_busy = 1;
    #endif
    
    BPending_Set(&i->job_operation);
}

#endif
