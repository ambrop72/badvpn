/**
 * @file FastPacketSource.h
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

#ifndef _FASTPACKETSOURCE_H
#define _FASTPACKETSOURCE_H

#include <stdint.h>
#include <string.h>

#include <misc/dead.h>
#include <misc/debug.h>
#include <system/BPending.h>
#include <flow/PacketPassInterface.h>

typedef struct {
    dead_t dead;
    PacketPassInterface *output;
    int psize;
    uint8_t *data;
    int data_len;
    BPending start_job;
} FastPacketSource;

static void _FastPacketSource_send (FastPacketSource *s)
{
    while (1) {
        DEAD_ENTER(s->dead)
        int res = PacketPassInterface_Sender_Send(s->output, s->data, s->data_len);
        if (DEAD_LEAVE(s->dead)) {
            return;
        }
        ASSERT(res == 0 || res == 1)
        if (res == 0) {
            return;
        }
    }
}

static void _FastPacketSource_output_handler_done (FastPacketSource *s)
{
    _FastPacketSource_send(s);
    return;
}

static void _FastPacketSource_job_handler (FastPacketSource *s)
{
    _FastPacketSource_send(s);
    return;
}

static void FastPacketSource_Init (FastPacketSource *s, PacketPassInterface *output, uint8_t *data, int data_len, BPendingGroup *pg)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= PacketPassInterface_GetMTU(output));
    
    // init arguments
    s->output = output;
    s->data = data;
    s->data_len = data_len;
    
    // init dead var
    DEAD_INIT(s->dead);
    
    // init output
    PacketPassInterface_Sender_Init(s->output, (PacketPassInterface_handler_done)_FastPacketSource_output_handler_done, s);
    
    // init start job
    BPending_Init(&s->start_job, pg, (BPending_handler)_FastPacketSource_job_handler, s);
    BPending_Set(&s->start_job);
}

static void FastPacketSource_Free (FastPacketSource *s)
{
    // free start job
    BPending_Free(&s->start_job);
    
    // free dead var
    DEAD_KILL(s->dead);
}

#endif
