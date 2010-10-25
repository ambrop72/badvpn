/**
 * @file PRStreamSource.c
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

#include <prerror.h>

#include <misc/debug.h>

#include <nspr_support/PRStreamSource.h>

static void report_error (PRStreamSource *s, int error)
{
    #ifndef NDEBUG
    s->in_error = 1;
    DEAD_ENTER(s->dead)
    #endif
    
    FlowErrorReporter_ReportError(&s->rep, &error);
    
    #ifndef NDEBUG
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(s->dead);
    #endif
}

static int output_handler_recv (PRStreamSource *s, uint8_t *data, int data_avail)
{
    ASSERT(s->out_avail == -1)
    ASSERT(data_avail > 0)
    ASSERT(!s->in_error)
    
    PRInt32 res = PR_Read(s->bprfd->prfd, data, data_avail);
    if (res < 0) {
        PRErrorCode error = PR_GetError();
        if (error == PR_WOULD_BLOCK_ERROR) {
            s->out_avail = data_avail;
            s->out = data;
            BPRFileDesc_EnableEvent(s->bprfd, PR_POLL_READ);
            return 0;
        }
        report_error(s, PRSTREAMSOURCE_ERROR_NSPR);
        return -1;
    }
    
    if (res == 0) {
        report_error(s, PRSTREAMSOURCE_ERROR_CLOSED);
        return -1;
    }
    
    return res;
}

static void prfd_handler (PRStreamSource *s, PRInt16 event)
{
    ASSERT(s->out_avail > 0)
    ASSERT(event == PR_POLL_READ)
    ASSERT(!s->in_error)
    
    PRInt32 res = PR_Read(s->bprfd->prfd, s->out, s->out_avail);
    if (res < 0) {
        PRErrorCode error = PR_GetError();
        if (error == PR_WOULD_BLOCK_ERROR) {
            BPRFileDesc_EnableEvent(s->bprfd, PR_POLL_READ);
            return;
        }
        report_error(s, PRSTREAMSOURCE_ERROR_NSPR);
        return;
    }
    
    if (res == 0) {
        report_error(s, PRSTREAMSOURCE_ERROR_CLOSED);
        return;
    }
    
    s->out_avail = -1;
    
    StreamRecvInterface_Done(&s->output, res);
    return;
}

void PRStreamSource_Init (PRStreamSource *s, FlowErrorReporter rep, BPRFileDesc *bprfd)
{
    // init arguments
    s->rep = rep;
    s->bprfd = bprfd;
    
    // init dead var
    DEAD_INIT(s->dead);
    
    // add socket event handler
    BPRFileDesc_AddEventHandler(s->bprfd, PR_POLL_READ, (BPRFileDesc_handler)prfd_handler, s);
    
    // init output
    StreamRecvInterface_Init(&s->output, (StreamRecvInterface_handler_recv)output_handler_recv, s);
    
    // have no output packet
    s->out_avail = -1;
    
    // init debugging
    #ifndef NDEBUG
    s->in_error = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&s->d_obj);
}

void PRStreamSource_Free (PRStreamSource *s)
{
    // free debug object
    DebugObject_Free(&s->d_obj);
    
    // free output
    StreamRecvInterface_Free(&s->output);
    
    // remove socket event handler
    BPRFileDesc_RemoveEventHandler(s->bprfd, PR_POLL_READ);
    
    // free dead var
    DEAD_KILL(s->dead);
}

StreamRecvInterface * PRStreamSource_GetOutput (PRStreamSource *s)
{
    return &s->output;
}
