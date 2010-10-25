/**
 * @file PRStreamSink.c
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

#include <nspr_support/PRStreamSink.h>

static void report_error (PRStreamSink *s, int error)
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

static int input_handler_send (PRStreamSink *s, uint8_t *data, int data_len)
{
    ASSERT(s->in_len == -1)
    ASSERT(data_len > 0)
    ASSERT(!s->in_error)
    
    int res = PR_Write(s->bprfd->prfd, data, data_len);
    if (res < 0) {
        PRErrorCode error = PR_GetError();
        if (error == PR_WOULD_BLOCK_ERROR) {
            s->in_len = data_len;
            s->in = data;
            BPRFileDesc_EnableEvent(s->bprfd, PR_POLL_WRITE);
            return 0;
        }
        report_error(s, PRSTREAMSINK_ERROR_NSPR);
        return -1;
    }
    
    ASSERT(res > 0)
    
    return res;
}

static void prfd_handler (PRStreamSink *s, PRInt16 event)
{
    ASSERT(s->in_len > 0)
    ASSERT(event == PR_POLL_WRITE)
    ASSERT(!s->in_error)
    
    int res = PR_Write(s->bprfd->prfd, s->in, s->in_len);
    if (res < 0) {
        PRErrorCode error = PR_GetError();
        if (error == PR_WOULD_BLOCK_ERROR) {
            BPRFileDesc_EnableEvent(s->bprfd, PR_POLL_WRITE);
            return;
        }
        report_error(s, PRSTREAMSINK_ERROR_NSPR);
        return;
    }
    
    ASSERT(res > 0)
    
    s->in_len = -1;
    
    StreamPassInterface_Done(&s->input, res);
    return;
}

void PRStreamSink_Init (PRStreamSink *s, FlowErrorReporter rep, BPRFileDesc *bprfd)
{
    // init arguments
    s->rep = rep;
    s->bprfd = bprfd;
    
    // init dead var
    DEAD_INIT(s->dead);
    
    // add socket event handler
    BPRFileDesc_AddEventHandler(s->bprfd, PR_POLL_WRITE, (BPRFileDesc_handler)prfd_handler, s);
    
    // init input
    StreamPassInterface_Init(&s->input, (StreamPassInterface_handler_send)input_handler_send, s);
    
    // have no input packet
    s->in_len = -1;
    
    // init debugging
    #ifndef NDEBUG
    s->in_error = 0;
    #endif
    
    // init debug object
    DebugObject_Init(&s->d_obj);
}

void PRStreamSink_Free (PRStreamSink *s)
{
    // free debug object
    DebugObject_Free(&s->d_obj);
    
    // free input
    StreamPassInterface_Free(&s->input);
    
    // remove socket event handler
    BPRFileDesc_RemoveEventHandler(s->bprfd, PR_POLL_WRITE);
    
    // free dead var
    DEAD_KILL(s->dead);
}

StreamPassInterface * PRStreamSink_GetInput (PRStreamSink *s)
{
    return &s->input;
}
