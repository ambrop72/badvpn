/**
 * @file BConnection_win.h
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

#include <windows.h>
#include <winsock2.h>
#ifdef BADVPN_USE_SHIPPED_MSWSOCK
#    include <misc/mswsock.h>
#else
#    include <mswsock.h>
#endif

#include <misc/debugerror.h>
#include <base/DebugObject.h>

struct BListener_addrbuf_stub {
    union {
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
    } addr;
    uint8_t extra[16];
};

struct BListener_s {
    BReactor *reactor;
    void *user;
    BListener_handler handler;
    int sys_family;
    SOCKET sock;
    LPFN_ACCEPTEX fnAcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS fnGetAcceptExSockaddrs;
    BReactorIOCPOverlapped olap;
    SOCKET newsock;
    uint8_t addrbuf[2 * sizeof(struct BListener_addrbuf_stub)];
    BPending next_job;
    int busy;
    int ready;
    DebugObject d_obj;
};

struct BConnector_s {
    BReactor *reactor;
    void *user;
    BConnector_handler handler;
    SOCKET sock;
    LPFN_CONNECTEX fnConnectEx;
    BReactorIOCPOverlapped olap;
    int busy;
    int ready;
    DebugObject d_obj;
};

struct BConnection_s {
    BReactor *reactor;
    void *user;
    BConnection_handler handler;
    SOCKET sock;
    int aborted;
    struct {
        BReactorIOCPOverlapped olap;
        int inited;
        StreamPassInterface iface;
        int busy;
        int busy_data_len;
    } send;
    struct {
        BReactorIOCPOverlapped olap;
        int closed;
        int inited;
        StreamRecvInterface iface;
        int busy;
        int busy_data_len;
    } recv;
    DebugError d_err;
    DebugObject d_obj;
};
