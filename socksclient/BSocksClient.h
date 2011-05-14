/**
 * @file BSocksClient.h
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
 * SOCKS5 client. TCP only, no authentication.
 */

#ifndef BADVPN_SOCKS_BSOCKSCLIENT_H
#define BADVPN_SOCKS_BSOCKSCLIENT_H

#include <stdint.h>

#include <misc/debug.h>
#include <misc/debugerror.h>
#include <misc/socks_proto.h>
#include <base/DebugObject.h>
#include <system/BSocket.h>
#include <flow/PacketStreamSender.h>
#include <flowextra/StreamSocketSink.h>
#include <flowextra/StreamSocketSource.h>

#define BSOCKSCLIENT_EVENT_ERROR 1
#define BSOCKSCLIENT_EVENT_UP 2
#define BSOCKSCLIENT_EVENT_ERROR_CLOSED 3

typedef void (*BSocksClient_handler) (void *user, int event);

typedef struct {
    BAddr dest_addr;
    BSocksClient_handler handler;
    void *user;
    BReactor *reactor;
    int state;
    FlowErrorDomain domain;
    BSocket sock;
    union {
        struct {
            PacketPassInterface *send_if;
            PacketStreamSender send_sender;
            StreamSocketSink send_sink;
            StreamSocketSource recv_source;
            StreamRecvInterface *recv_if;
            union {
                struct {
                    struct socks_client_hello_header header;
                    struct socks_client_hello_method method;
                } __attribute__((packed)) client_hello;
                struct socks_server_hello server_hello;
                struct {
                    struct socks_request_header header;
                    union {
                        struct socks_addr_ipv4 ipv4;
                        struct socks_addr_ipv6 ipv6;
                    } addr;
                } __attribute__((packed)) request;
                struct {
                    struct socks_reply_header header;
                    union {
                        struct socks_addr_ipv4 ipv4;
                        struct socks_addr_ipv6 ipv6;
                    } addr;
                } __attribute__((packed)) reply;
            } msg;
            uint8_t *recv_dest;
            int recv_len;
            int recv_total;
        } control;
        struct {
            StreamSocketSink send_sink;
            StreamSocketSource recv_source;
        } up;
    };
    DebugObject d_obj;
    DebugError d_err;
} BSocksClient;

int BSocksClient_Init (BSocksClient *o, BAddr server_addr, BAddr dest_addr, BSocksClient_handler handler, void *user, BReactor *reactor) WARN_UNUSED;
void BSocksClient_Free (BSocksClient *o);
StreamPassInterface * BSocksClient_GetSendInterface (BSocksClient *o);
StreamRecvInterface * BSocksClient_GetRecvInterface (BSocksClient *o);

#endif
