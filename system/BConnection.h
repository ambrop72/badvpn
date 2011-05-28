/**
 * @file BConnection.h
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

#ifndef BADVPN_SYSTEM_BCONNECTION
#define BADVPN_SYSTEM_BCONNECTION

#include <misc/debug.h>
#include <flow/StreamPassInterface.h>
#include <flow/StreamRecvInterface.h>
#include <system/BAddr.h>
#include <system/BReactor.h>
#include <system/BNetwork.h>



int BConnection_AddressSupported (BAddr addr);



struct BListener_s;
typedef struct BListener_s BListener;

typedef void (*BListener_handler) (void *user);

int BListener_Init (BListener *o, BAddr addr, BReactor *reactor, void *user,
                    BListener_handler handler) WARN_UNUSED;
void BListener_Free (BListener *o);



struct BConnector_s;
typedef struct BConnector_s BConnector;

typedef void (*BConnector_handler) (void *user, int is_error);

int BConnector_Init (BConnector *o, BAddr addr, BReactor *reactor, void *user,
                     BConnector_handler handler) WARN_UNUSED;
void BConnector_Free (BConnector *o);



#define BCONNECTION_SOURCE_TYPE_LISTENER 1
#define BCONNECTION_SOURCE_TYPE_CONNECTOR 2
#define BCONNECTION_SOURCE_TYPE_PIPE 3

struct BConnection_source {
    int type;
    union {
        struct {
            BListener *listener;
            BAddr *out_addr;
        } listener;
        struct {
            BConnector *connector;
        } connector;
#ifndef BADVPN_USE_WINAPI
        struct {
            int pipefd;
        } pipe;
#endif
    } u;
};

#define BCONNECTION_SOURCE_LISTENER(_listener, _out_addr) \
    ((struct BConnection_source){ \
        .type = BCONNECTION_SOURCE_TYPE_LISTENER, \
        .u.listener.listener = (_listener), \
        .u.listener.out_addr = (_out_addr) \
    })

#define BCONNECTION_SOURCE_CONNECTOR(_connector) \
    ((struct BConnection_source){ \
        .type = BCONNECTION_SOURCE_TYPE_CONNECTOR, \
        .u.connector.connector = (_connector) \
    })

#ifndef BADVPN_USE_WINAPI
#define BCONNECTION_SOURCE_PIPE(_pipefd) \
    ((struct BConnection_source){ \
        .type = BCONNECTION_SOURCE_TYPE_PIPE, \
        .u.pipe.pipefd = (_pipefd) \
    })
#endif



struct BConnection_s;
typedef struct BConnection_s BConnection;

#define BCONNECTION_EVENT_ERROR 1
#define BCONNECTION_EVENT_RECVCLOSED 2

typedef void (*BConnection_handler) (void *user, int event);

int BConnection_Init (BConnection *o, struct BConnection_source source, BReactor *reactor, void *user,
                      BConnection_handler handler) WARN_UNUSED;
void BConnection_Free (BConnection *o);
void BConnection_SetHandlers (BConnection *o, void *user, BConnection_handler handler_event);
int BConnection_SetSendBuffer (BConnection *o, int buf_size);

void BConnection_SendAsync_Init (BConnection *o);
void BConnection_SendAsync_Free (BConnection *o);
StreamPassInterface * BConnection_SendAsync_GetIf (BConnection *o);

void BConnection_RecvAsync_Init (BConnection *o);
void BConnection_RecvAsync_Free (BConnection *o);
StreamRecvInterface * BConnection_RecvAsync_GetIf (BConnection *o);



#ifdef BADVPN_USE_WINAPI
#include "BConnection_win.h"
#else
#include "BConnection_unix.h"
#endif

#endif
