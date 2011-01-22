/**
 * @file BSocketPRFileDesc.c
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

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <prerror.h>
#include <prmem.h>

#include <misc/debug.h>
#include <misc/offset.h>
#include <system/BLog.h>

#include <nspr_support/BSocketPRFileDesc.h>

#include <generated/blog_channel_BSocketPRFileDesc.h>

#ifndef NDEBUG
int bsocketprfiledesc_initialized = 0;
#endif
PRDescIdentity bsocketprfiledesc_identity;

static int baddr_to_prnetaddr (PRNetAddr *out, BAddr addr)
{
    memset(out, 0, sizeof(PRNetAddr));
    
    switch (addr.type) {
        case BADDR_TYPE_IPV4:
            out->inet.family = PR_AF_INET;
            out->inet.port = addr.ipv4.port;
            out->inet.ip = addr.ipv4.ip;
            break;
        case BADDR_TYPE_IPV6:
            out->ipv6.family = PR_AF_INET6;
            out->ipv6.port = addr.ipv6.port;
            out->ipv6.flowinfo = 0;
            memcpy(&out->ipv6.ip, addr.ipv6.ip, 16);
            break;
        default:
            return 0;
    }
    
    return 1;
}

static PRStatus method_close (PRFileDesc *fd)
{
    return PR_SUCCESS;
}

static PRInt32 method_read (PRFileDesc *fd, void *buf, PRInt32 amount)
{
    ASSERT(amount >= 0)
    
    BSocket *bsock = (BSocket *)fd->secret;
    
    if (amount > INT_MAX) {
        amount = INT_MAX;
    }
    
    int res = BSocket_Recv(bsock, buf, amount);
    if (res < 0) {
        switch (BSocket_GetError(bsock)) {
            case BSOCKET_ERROR_LATER:
                PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
                return -1;
            default:
                BLog(BLOG_NOTICE, "BSocket_Recv failed (%d)", BSocket_GetError(bsock));
                PR_SetError(PR_UNKNOWN_ERROR, 0);
                return -1;
        }
    }
    
    return res;
}

static PRInt32 method_write (PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    ASSERT(amount >= 0)
    
    BSocket *bsock = (BSocket *)fd->secret;
    
    if (amount > INT_MAX) {
        amount = INT_MAX;
    }
    
    int res = BSocket_Send(bsock, (uint8_t *)buf, amount);
    ASSERT(res != 0)
    if (res < 0) {
        switch (BSocket_GetError(bsock)) {
            case BSOCKET_ERROR_LATER:
                PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
                return -1;
            default:
                BLog(BLOG_NOTICE, "BSocket_Send failed (%d)", BSocket_GetError(bsock));
                PR_SetError(PR_UNKNOWN_ERROR, 0);
                return -1;
        }
    }
    
    return res;
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
    BSocket *bsock = (BSocket *)fd->secret;
    
    BAddr baddr;
    if (BSocket_GetPeerName(bsock, &baddr) < 0) {
        PR_SetError(PR_UNKNOWN_ERROR, 0);
        return PR_FAILURE;
    }
    
    if (!baddr_to_prnetaddr(addr, baddr)) {
        PR_SetError(PR_UNKNOWN_ERROR, 0);
        return PR_FAILURE;
    }
    
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

int BSocketPRFileDesc_GlobalInit (void)
{
    ASSERT(!bsocketprfiledesc_initialized)
    
    if ((bsocketprfiledesc_identity = PR_GetUniqueIdentity("BSocketPRFileDesc")) == PR_INVALID_IO_LAYER) {
        return 0;
    }
    
    #ifndef NDEBUG
    bsocketprfiledesc_initialized = 1;
    #endif
    
    return 1;
}

void BSocketPRFileDesc_Create (PRFileDesc *prfd, BSocket *bsock)
{
    ASSERT(bsocketprfiledesc_initialized)
    
    memset(prfd, 0, sizeof(prfd));
    prfd->methods = &methods;
    prfd->secret = (PRFilePrivate *)bsock;
    prfd->identity = bsocketprfiledesc_identity;
}
