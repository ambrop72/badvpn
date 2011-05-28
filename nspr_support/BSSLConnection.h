/**
 * @file BSSLConnection.h
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

#ifndef BADVPN_BSSLCONNECTION_H
#define BADVPN_BSSLCONNECTION_H

#include <prio.h>

#include <misc/debug.h>
#include <misc/debugerror.h>
#include <base/DebugObject.h>
#include <system/BReactor.h>
#include <flow/StreamPassInterface.h>
#include <flow/StreamRecvInterface.h>

#define BSSLCONNECTION_EVENT_UP 1
#define BSSLCONNECTION_EVENT_ERROR 2

#define BSSLCONNECTION_BUF_SIZE 4096

typedef void (*BSSLConnection_handler) (void *user, int event);

struct BSSLConnection_backend;

typedef struct {
    PRFileDesc *prfd;
    BReactor *reactor;
    void *user;
    BSSLConnection_handler handler;
    struct BSSLConnection_backend *backend;
    int have_error;
    int up;
    BPending init_job;
    StreamPassInterface send_if;
    StreamRecvInterface recv_if;
    BPending recv_job;
    const uint8_t *send_data;
    int send_len;
    uint8_t *recv_data;
    int recv_avail;
    DebugError d_err;
    DebugObject d_obj;
} BSSLConnection;

struct BSSLConnection_backend {
    StreamPassInterface *send_if;
    StreamRecvInterface *recv_if;
    BSSLConnection *con;
    uint8_t send_buf[BSSLCONNECTION_BUF_SIZE];
    int send_pos;
    int send_len;
    uint8_t recv_buf[BSSLCONNECTION_BUF_SIZE];
    int recv_busy;
    int recv_pos;
    int recv_len;
};

int BSSLConnection_GlobalInit (void) WARN_UNUSED;
int BSSLConnection_MakeBackend (PRFileDesc *prfd, StreamPassInterface *send_if, StreamRecvInterface *recv_if) WARN_UNUSED;

void BSSLConnection_Init (BSSLConnection *o, PRFileDesc *prfd, int force_handshake, BReactor *reactor, void *user,
                          BSSLConnection_handler handler);
void BSSLConnection_Free (BSSLConnection *o);
StreamPassInterface * BSSLConnection_GetSendIf (BSSLConnection *o);
StreamRecvInterface * BSSLConnection_GetRecvIf (BSSLConnection *o);

#endif
