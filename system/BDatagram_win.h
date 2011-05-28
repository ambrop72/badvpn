/**
 * @file BDatagram_win.h
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

struct BDatagram_sys_addr {
    int len;
    union {
        struct sockaddr generic;
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
    } addr;
};

struct BDatagram_s {
    BReactor *reactor;
    void *user;
    BDatagram_handler handler;
    SOCKET sock;
    LPFN_WSASENDMSG fnWSASendMsg;
    LPFN_WSARECVMSG fnWSARecvMsg;
    int aborted;
    struct {
        BReactorIOCPOverlapped olap;
        int have_addrs;
        BAddr remote_addr;
        BIPAddr local_addr;
        int inited;
        int mtu;
        PacketPassInterface iface;
        BPending job;
        int data_len;
        uint8_t *data;
        int data_busy;
        struct BDatagram_sys_addr sysaddr;
        union {
            char in[WSA_CMSG_SPACE(sizeof(struct in_pktinfo))];
            char in6[WSA_CMSG_SPACE(sizeof(struct in6_pktinfo))];
        } cdata;
        WSAMSG msg;
    } send;
    struct {
        BReactorIOCPOverlapped olap;
        int started;
        int have_addrs;
        BAddr remote_addr;
        BIPAddr local_addr;
        int inited;
        int mtu;
        PacketRecvInterface iface;
        BPending job;
        int data_have;
        uint8_t *data;
        int data_busy;
        struct BDatagram_sys_addr sysaddr;
        union {
            char in[WSA_CMSG_SPACE(sizeof(struct in_pktinfo))];
            char in6[WSA_CMSG_SPACE(sizeof(struct in6_pktinfo))];
        } cdata;
        WSAMSG msg;
    } recv;
    DebugError d_err;
    DebugObject d_obj;
};
