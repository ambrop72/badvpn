/**
 * @file BSSLConnection.c
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
#include <ssl.h>

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include <base/BLog.h>

#include "BSSLConnection.h"

#include <generated/blog_channel_BSSLConnection.h>

static void connection_init_job_handler (BSSLConnection *o);
static void connection_init_up (BSSLConnection *o);
static void connection_try_io (BSSLConnection *o);
static void connection_recv_job_handler (BSSLConnection *o);
static void connection_try_send (BSSLConnection *o);
static void connection_try_recv (BSSLConnection *o);
static void connection_send_if_handler_send (BSSLConnection *o, uint8_t *data, int data_len);
static void connection_recv_if_handler_recv (BSSLConnection *o, uint8_t *data, int data_len);

int bprconnection_initialized = 0;
PRDescIdentity bprconnection_identity;

static PRFileDesc * get_bottom (PRFileDesc *layer)
{
    while (layer->lower) {
        layer = layer->lower;
    }
    
    return layer;
}

static PRStatus method_close (PRFileDesc *fd)
{
    struct BSSLConnection_backend *b = (struct BSSLConnection_backend *)fd->secret;
    ASSERT(!b->con)
    
    // free backend
    free(b);
    
    // set no secret
    fd->secret = NULL;
    
    return PR_SUCCESS;
}

static PRInt32 method_read (PRFileDesc *fd, void *buf, PRInt32 amount)
{
    struct BSSLConnection_backend *b = (struct BSSLConnection_backend *)fd->secret;
    ASSERT(amount > 0)
    
    // if we are receiving into buffer or buffer has no data left, refuse recv
    if (b->recv_busy || b->recv_pos == b->recv_len) {
        // start receiving if not already
        if (!b->recv_busy) {
            // set recv busy
            b->recv_busy = 1;
            
            // receive into buffer
            StreamRecvInterface_Receiver_Recv(b->recv_if, b->recv_buf, BSSLCONNECTION_BUF_SIZE);
        }
        
        PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
        return -1;
    }
    
    // limit amount to available data
    if (amount > b->recv_len - b->recv_pos) {
        amount = b->recv_len - b->recv_pos;
    }
    
    // copy data
    memcpy(buf, b->recv_buf + b->recv_pos, amount);
    
    // update buffer
    b->recv_pos += amount;
    
    return amount;
}

static PRInt32 method_write (PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    struct BSSLConnection_backend *b = (struct BSSLConnection_backend *)fd->secret;
    ASSERT(amount > 0)
    
    // if there is data in buffer, refuse send
    if (b->send_pos < b->send_len) {
        PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
        return -1;
    }
    
    // limit amount to buffer size
    if (amount > BSSLCONNECTION_BUF_SIZE) {
        amount = BSSLCONNECTION_BUF_SIZE;
    }
    
    // init buffer
    memcpy(b->send_buf, buf, amount);
    b->send_pos = 0;
    b->send_len = amount;
    
    // start sending
    StreamPassInterface_Sender_Send(b->send_if, b->send_buf + b->send_pos, b->send_len - b->send_pos);
    
    return amount;
}

static PRStatus method_shutdown (PRFileDesc *fd, PRIntn how)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

static PRInt32 method_recv (PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    ASSERT(flags == 0)
    
    return method_read(fd, buf, amount);
}

static PRInt32 method_send (PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    ASSERT(flags == 0)
    
    return method_write(fd, buf, amount);
}

static PRInt16 method_poll (PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    *out_flags = 0;
    return in_flags;
}

static PRStatus method_getpeername (PRFileDesc *fd, PRNetAddr *addr)
{
    memset(addr, 0, sizeof(*addr));
    addr->raw.family = PR_AF_INET;
    return PR_SUCCESS;
}

static PRStatus method_getsocketoption (PRFileDesc *fd, PRSocketOptionData *data)
{
    switch (data->option) {
        case PR_SockOpt_Nonblocking:
            data->value.non_blocking = PR_TRUE;
            return PR_SUCCESS;
    }
    
    PR_SetError(PR_UNKNOWN_ERROR, 0);
    return PR_FAILURE;
}

static PRStatus method_setsocketoption (PRFileDesc *fd, const PRSocketOptionData *data)
{
    PR_SetError(PR_UNKNOWN_ERROR, 0);
    return PR_FAILURE;
}

static PRIntn _PR_InvalidIntn (void)
{
    ASSERT(0)
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PRInt32 _PR_InvalidInt32 (void)
{
    ASSERT(0)
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PRInt64 _PR_InvalidInt64 (void)
{
    ASSERT(0)
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PROffset32 _PR_InvalidOffset32 (void)
{
    ASSERT(0)
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PROffset64 _PR_InvalidOffset64 (void)
{
    ASSERT(0)
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PRStatus _PR_InvalidStatus (void)
{
    ASSERT(0)
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

static PRFileDesc *_PR_InvalidDesc (void)
{
    ASSERT(0)
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return NULL;
}

static PRIOMethods methods = {
    (PRDescType)0,
    method_close,
    method_read,
    method_write,
    (PRAvailableFN)_PR_InvalidInt32,
    (PRAvailable64FN)_PR_InvalidInt64,
    (PRFsyncFN)_PR_InvalidStatus,
    (PRSeekFN)_PR_InvalidOffset32,
    (PRSeek64FN)_PR_InvalidOffset64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    (PRWritevFN)_PR_InvalidInt32,
    (PRConnectFN)_PR_InvalidStatus,
    (PRAcceptFN)_PR_InvalidDesc,
    (PRBindFN)_PR_InvalidStatus,
    (PRListenFN)_PR_InvalidStatus,
    method_shutdown,
    method_recv,
    method_send,
    (PRRecvfromFN)_PR_InvalidInt32,
    (PRSendtoFN)_PR_InvalidInt32,
    method_poll,
    (PRAcceptreadFN)_PR_InvalidInt32,
    (PRTransmitfileFN)_PR_InvalidInt32,
    (PRGetsocknameFN)_PR_InvalidStatus,
    method_getpeername,
    (PRReservedFN)_PR_InvalidIntn,
    (PRReservedFN)_PR_InvalidIntn,
    method_getsocketoption,
    method_setsocketoption,
    (PRSendfileFN)_PR_InvalidInt32,
    (PRConnectcontinueFN)_PR_InvalidStatus,
    (PRReservedFN)_PR_InvalidIntn,
    (PRReservedFN)_PR_InvalidIntn,
    (PRReservedFN)_PR_InvalidIntn,
    (PRReservedFN)_PR_InvalidIntn
};

static void backend_send_if_handler_done (struct BSSLConnection_backend *b, int data_len)
{
    ASSERT(b->send_len > 0)
    ASSERT(b->send_pos < b->send_len)
    ASSERT(data_len > 0)
    ASSERT(data_len <= b->send_len - b->send_pos)
    
    // update buffer
    b->send_pos += data_len;
    
    // send more if needed
    if (b->send_pos < b->send_len) {
        StreamPassInterface_Sender_Send(b->send_if, b->send_buf + b->send_pos, b->send_len - b->send_pos);
        return;
    }
    
    // notify connection
    if (b->con && !b->con->have_error) {
        connection_try_io(b->con);
        return;
    }
}

static void backend_recv_if_handler_done (struct BSSLConnection_backend *b, int data_len)
{
    ASSERT(b->recv_busy)
    ASSERT(data_len > 0)
    ASSERT(data_len <= BSSLCONNECTION_BUF_SIZE)
    
    // init buffer
    b->recv_busy = 0;
    b->recv_pos = 0;
    b->recv_len = data_len;
    
    // notify connection
    if (b->con && !b->con->have_error) {
        connection_try_io(b->con);
        return;
    }
}

static void connection_report_error (BSSLConnection *o)
{
    ASSERT(!o->have_error)
    
    // set error
    o->have_error = 1;
    
    // report error
    DEBUGERROR(&o->d_err, o->handler(o->user, BSSLCONNECTION_EVENT_ERROR));
}

static void connection_init_job_handler (BSSLConnection *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(!o->have_error)
    ASSERT(!o->up)
    
    connection_try_io(o);
    return;
}

static void connection_init_up (BSSLConnection *o)
{
    // init send interface
    StreamPassInterface_Init(&o->send_if, (StreamPassInterface_handler_send)connection_send_if_handler_send, o, BReactor_PendingGroup(o->reactor));
    
    // init recv interface
    StreamRecvInterface_Init(&o->recv_if, (StreamRecvInterface_handler_recv)connection_recv_if_handler_recv, o, BReactor_PendingGroup(o->reactor));
    
    // init recv job
    BPending_Init(&o->recv_job, BReactor_PendingGroup(o->reactor), (BPending_handler)connection_recv_job_handler, o);
    
    // set no send data
    o->send_len = -1;
    
    // set no recv data
    o->recv_avail = -1;
    
    // set up
    o->up = 1;
}

static void connection_try_io (BSSLConnection *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(!o->have_error)
    
    if (!o->up) {
        // unset init job (in case backend called us before it executed)
        BPending_Unset(&o->init_job);
        
        // try handshake
        SECStatus res = SSL_ForceHandshake(o->prfd);
        if (res == SECFailure) {
            PRErrorCode error = PR_GetError();
            if (error == PR_WOULD_BLOCK_ERROR) {
                return;
            }
            
            BLog(BLOG_ERROR, "SSL_ForceHandshake failed (%"PRIi32")", error);
            
            connection_report_error(o);
            return;
        }
        
        // init up
        connection_init_up(o);
        
        // report up
        o->handler(o->user, BSSLCONNECTION_EVENT_UP);
        return;
    }
    
    if (o->send_len > 0) {
        if (o->recv_avail > 0) {
            BPending_Set(&o->recv_job);
        }
        
        connection_try_send(o);
        return;
    }
    
    if (o->recv_avail > 0) {
        connection_try_recv(o);
        return;
    }
}

static void connection_recv_job_handler (BSSLConnection *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(!o->have_error)
    ASSERT(o->up)
    ASSERT(o->recv_avail > 0)
    
    connection_try_recv(o);
    return;
}

static void connection_try_send (BSSLConnection *o)
{
    ASSERT(!o->have_error)
    ASSERT(o->up)
    ASSERT(o->send_len > 0)
    
    // send
    PRInt32 res = PR_Write(o->prfd, o->send_data, o->send_len);
    if (res < 0) {
        PRErrorCode error = PR_GetError();
        if (error == PR_WOULD_BLOCK_ERROR) {
            return;
        }
        
        BLog(BLOG_ERROR, "PR_Write failed (%"PRIi32")", error);
        
        connection_report_error(o);
        return;
    }
    
    ASSERT(res > 0)
    ASSERT(res <= o->send_len)
    
    // set no send data
    o->send_len = -1;
    
    // done
    StreamPassInterface_Done(&o->send_if, res);
}

static void connection_try_recv (BSSLConnection *o)
{
    ASSERT(!o->have_error)
    ASSERT(o->up)
    ASSERT(o->recv_avail > 0)
    
    // unset recv job
    BPending_Unset(&o->recv_job);
    
    // recv
    PRInt32 res = PR_Read(o->prfd, o->recv_data, o->recv_avail);
    if (res < 0) {
        PRErrorCode error = PR_GetError();
        if (error == PR_WOULD_BLOCK_ERROR) {
            return;
        }
        
        BLog(BLOG_ERROR, "PR_Read failed (%"PRIi32")", error);
        
        connection_report_error(o);
        return;
    }
    
    if (res == 0) {
        BLog(BLOG_ERROR, "PR_Read returned 0");
        
        connection_report_error(o);
        return;
    }
    
    ASSERT(res > 0)
    ASSERT(res <= o->recv_avail)
    
    // set no recv data
    o->recv_avail = -1;
    
    // done
    StreamRecvInterface_Done(&o->recv_if, res);
}

static void connection_send_if_handler_send (BSSLConnection *o, uint8_t *data, int data_len)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(!o->have_error)
    ASSERT(o->up)
    ASSERT(o->send_len == -1)
    ASSERT(data_len > 0)
    
    // limit amount for PR_Write
    if (data_len > INT32_MAX) {
        data_len = INT32_MAX;
    }
    
    // set send data
    o->send_data = data;
    o->send_len = data_len;
    
    // start sending
    connection_try_send(o);
    return;
}

static void connection_recv_if_handler_recv (BSSLConnection *o, uint8_t *data, int data_len)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(!o->have_error)
    ASSERT(o->up)
    ASSERT(o->recv_avail == -1)
    ASSERT(data_len > 0)
    
    // limit amount for PR_Read
    if (data_len > INT32_MAX) {
        data_len = INT32_MAX;
    }
    
    // set recv data
    o->recv_data = data;
    o->recv_avail = data_len;
    
    // start receiving
    connection_try_recv(o);
    return;
}

int BSSLConnection_GlobalInit (void)
{
    ASSERT(!bprconnection_initialized)
    
    if ((bprconnection_identity = PR_GetUniqueIdentity("BSSLConnection")) == PR_INVALID_IO_LAYER) {
        BLog(BLOG_ERROR, "PR_GetUniqueIdentity failed");
        return 0;
    }
    
    bprconnection_initialized = 1;
    
    return 1;
}

int BSSLConnection_MakeBackend (PRFileDesc *prfd, StreamPassInterface *send_if, StreamRecvInterface *recv_if)
{
    ASSERT(bprconnection_initialized)
    
    // allocate backend
    struct BSSLConnection_backend *b = malloc(sizeof(*b));
    if (!b) {
        BLog(BLOG_ERROR, "malloc failed");
        return 0;
    }
    
    // init arguments
    b->send_if = send_if;
    b->recv_if = recv_if;
    
    // init interfaces
    StreamPassInterface_Sender_Init(b->send_if, (StreamPassInterface_handler_done)backend_send_if_handler_done, b);
    StreamRecvInterface_Receiver_Init(b->recv_if, (StreamRecvInterface_handler_done)backend_recv_if_handler_done, b);
    
    // set no connection
    b->con = NULL;
    
    // init send buffer
    b->send_len = 0;
    b->send_pos = 0;
    
    // init recv buffer
    b->recv_busy = 0;
    b->recv_pos = 0;
    b->recv_len = 0;
    
    // init prfd
    memset(prfd, 0, sizeof(*prfd));
    prfd->methods = &methods;
    prfd->secret = (PRFilePrivate *)b;
    prfd->identity = bprconnection_identity;
    
    return 1;
}

void BSSLConnection_Init (BSSLConnection *o, PRFileDesc *prfd, int force_handshake, BReactor *reactor, void *user,
                          BSSLConnection_handler handler)
{
    ASSERT(force_handshake == 0 || force_handshake == 1)
    ASSERT(handler)
    ASSERT(bprconnection_initialized)
    ASSERT(get_bottom(prfd)->identity == bprconnection_identity)
    ASSERT(!((struct BSSLConnection_backend *)(get_bottom(prfd)->secret))->con)
    
    // init arguments
    o->prfd = prfd;
    o->reactor = reactor;
    o->user = user;
    o->handler = handler;
    
    // set backend
    o->backend = (struct BSSLConnection_backend *)(get_bottom(prfd)->secret);
    
    // set have no error
    o->have_error = 0;
    
    // init init job
    BPending_Init(&o->init_job, BReactor_PendingGroup(o->reactor), (BPending_handler)connection_init_job_handler, o);
    
    if (force_handshake) {
        // set not up
        o->up = 0;
        
        // set init job
        BPending_Set(&o->init_job);
    } else {
        // init up
        connection_init_up(o);
    }
    
    // set backend connection
    o->backend->con = o;
    
    DebugError_Init(&o->d_err, BReactor_PendingGroup(o->reactor));
    DebugObject_Init(&o->d_obj);
}

void BSSLConnection_Free (BSSLConnection *o)
{
    DebugObject_Free(&o->d_obj);
    DebugError_Free(&o->d_err);
    
    if (o->up) {
        // free recv job
        BPending_Free(&o->recv_job);
        
        // free recv interface
        StreamRecvInterface_Free(&o->recv_if);
        
        // free send interface
        StreamPassInterface_Free(&o->send_if);
    }
    
    // free init job
    BPending_Free(&o->init_job);
    
    // unset backend connection
    o->backend->con = NULL;
}

StreamPassInterface * BSSLConnection_GetSendIf (BSSLConnection *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->up)
    
    return &o->send_if;
}

StreamRecvInterface * BSSLConnection_GetRecvIf (BSSLConnection *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->up)
    
    return &o->recv_if;
}
