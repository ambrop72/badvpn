/**
 * @file BSocket.c
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

#ifndef BADVPN_USE_WINAPI
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <misc/debug.h>

#include <system/BSocket.h>

#define HANDLER_READ 0
#define HANDLER_WRITE 1
#define HANDLER_ACCEPT 2
#define HANDLER_CONNECT 3
#define HANDLER_ERROR 4

static int get_event_index (int event)
{
    switch (event) {
        case BSOCKET_READ:
            return HANDLER_READ;
        case BSOCKET_WRITE:
            return HANDLER_WRITE;
        case BSOCKET_ACCEPT:
            return HANDLER_ACCEPT;
        case BSOCKET_CONNECT:
            return HANDLER_CONNECT;
        case BSOCKET_ERROR:
            return HANDLER_ERROR;
        default:
            ASSERT(0)
            return 42;
    }
}

static int handler_events[] = {
    [HANDLER_READ] = BSOCKET_READ,
    [HANDLER_WRITE] = BSOCKET_WRITE,
    [HANDLER_ACCEPT] = BSOCKET_ACCEPT,
    [HANDLER_CONNECT] = BSOCKET_CONNECT,
    [HANDLER_ERROR] = BSOCKET_ERROR
};

static void init_handlers (BSocket *bs)
{
    bs->global_handler = NULL;
    
    for (int i = 0; i < BSOCKET_NUM_EVENTS; i++) {
        bs->handlers[i] = NULL;
    }
}

static int set_nonblocking (int s)
{
    #ifdef BADVPN_USE_WINAPI
    unsigned long bl = 1;
    int res = ioctlsocket(s, FIONBIO, &bl);
    #else
    int res = fcntl(s, F_SETFL, O_NONBLOCK);
    #endif
    return res;
}

static int set_pktinfo (int s)
{
    #ifdef BADVPN_USE_WINAPI
    DWORD opt = 1;
    int res = setsockopt(s, IPPROTO_IP, IP_PKTINFO, (char *)&opt, sizeof(opt));
    #else
    int opt = 1;
    int res = setsockopt(s, IPPROTO_IP, IP_PKTINFO, &opt, sizeof(opt));
    #endif
    return res;
}

static int set_pktinfo6 (int s)
{
    #ifdef BADVPN_USE_WINAPI
    DWORD opt = 1;
    int res = setsockopt(s, IPPROTO_IPV6, IPV6_PKTINFO, (char *)&opt, sizeof(opt));
    #else
    int opt = 1;
    int res = setsockopt(s, IPPROTO_IPV6, IPV6_RECVPKTINFO, &opt, sizeof(opt));
    #endif
    return res;
}

static void close_socket (int fd)
{
    #ifdef BADVPN_USE_WINAPI
    int res = closesocket(fd);
    #else
    int res = close(fd);
    #endif
    ASSERT_FORCE(res == 0)
}

static int translate_error (int error)
{
    #ifdef BADVPN_USE_WINAPI
    
    switch (error) {
        case WSAEADDRNOTAVAIL:
            return BSOCKET_ERROR_ADDRESS_NOT_AVAILABLE;
        case WSAEADDRINUSE:
            return BSOCKET_ERROR_ADDRESS_IN_USE;
        case WSAECONNRESET:
            return BSOCKET_ERROR_CONNECTION_RESET;
        case WSAETIMEDOUT:
            return BSOCKET_ERROR_CONNECTION_TIMED_OUT;
        case WSAECONNREFUSED:
            return BSOCKET_ERROR_CONNECTION_REFUSED;
    }
    
    #else
    
    switch (error) {
        case EADDRNOTAVAIL:
            return BSOCKET_ERROR_ADDRESS_NOT_AVAILABLE;
        case EADDRINUSE:
            return BSOCKET_ERROR_ADDRESS_IN_USE;
        case EACCES:
        case EPERM:
            return BSOCKET_ERROR_ACCESS_DENIED;
        case ECONNREFUSED:
            return BSOCKET_ERROR_CONNECTION_REFUSED;
        case ECONNRESET:
            return BSOCKET_ERROR_CONNECTION_RESET;
        case ENETUNREACH:
            return BSOCKET_ERROR_NETWORK_UNREACHABLE;
        case ETIMEDOUT:
            return BSOCKET_ERROR_CONNECTION_TIMED_OUT;
        case ENOMEM:
            return BSOCKET_ERROR_NO_MEMORY;
    }
    
    #endif
    
    return BSOCKET_ERROR_UNKNOWN;
}

struct sys_addr {
    #ifdef BADVPN_USE_WINAPI
    int len;
    #else
    socklen_t len;
    #endif
    union {
        struct sockaddr generic;
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
        #ifndef BADVPN_USE_WINAPI
        struct sockaddr_ll packet;
        #endif
    } addr;
};

static void addr_socket_to_sys (struct sys_addr *out, BAddr *addr)
{
    switch (addr->type) {
        case BADDR_TYPE_IPV4:
            out->len = sizeof(out->addr.ipv4);
            memset(&out->addr.ipv4, 0, sizeof(out->addr.ipv4));
            out->addr.ipv4.sin_family = AF_INET;
            out->addr.ipv4.sin_port = addr->ipv4.port;
            out->addr.ipv4.sin_addr.s_addr = addr->ipv4.ip;
            break;
        case BADDR_TYPE_IPV6:
            out->len = sizeof(out->addr.ipv6);
            memset(&out->addr.ipv6, 0, sizeof(out->addr.ipv6));
            out->addr.ipv6.sin6_family = AF_INET6;
            out->addr.ipv6.sin6_port = addr->ipv6.port;
            out->addr.ipv6.sin6_flowinfo = 0;
            memcpy(out->addr.ipv6.sin6_addr.s6_addr, addr->ipv6.ip, 16);
            out->addr.ipv6.sin6_scope_id = 0;
            break;
        #ifndef BADVPN_USE_WINAPI
        case BADDR_TYPE_PACKET:
            ASSERT(addr->packet.header_type == BADDR_PACKET_HEADER_TYPE_ETHERNET)
            memset(&out->addr.packet, 0, sizeof(out->addr.packet));
            out->len = sizeof(out->addr.packet);
            out->addr.packet.sll_family = AF_PACKET;
            out->addr.packet.sll_protocol = addr->packet.phys_proto;
            out->addr.packet.sll_ifindex = addr->packet.interface_index;
            out->addr.packet.sll_hatype = 1; // linux/if_arp.h: #define ARPHRD_ETHER 1
            switch (addr->packet.packet_type) {
                case BADDR_PACKET_PACKET_TYPE_HOST:
                    out->addr.packet.sll_pkttype = PACKET_HOST;
                    break;
                case BADDR_PACKET_PACKET_TYPE_BROADCAST:
                    out->addr.packet.sll_pkttype = PACKET_BROADCAST;
                    break;
                case BADDR_PACKET_PACKET_TYPE_MULTICAST:
                    out->addr.packet.sll_pkttype = PACKET_MULTICAST;
                    break;
                case BADDR_PACKET_PACKET_TYPE_OTHERHOST:
                    out->addr.packet.sll_pkttype = PACKET_OTHERHOST;
                    break;
                case BADDR_PACKET_PACKET_TYPE_OUTGOING:
                    out->addr.packet.sll_pkttype = PACKET_OUTGOING;
                    break;
                default:
                    ASSERT(0);
            }
            out->addr.packet.sll_halen = 6;
            memcpy(out->addr.packet.sll_addr, addr->packet.phys_addr, 6);
            break;
        #endif
        default:
            ASSERT(0)
            break;
    }
}

static void addr_sys_to_socket (BAddr *out, struct sys_addr *addr)
{
    switch (addr->addr.generic.sa_family) {
        case AF_INET:
            ASSERT(addr->len == sizeof(struct sockaddr_in))
            BAddr_InitIPv4(out, addr->addr.ipv4.sin_addr.s_addr, addr->addr.ipv4.sin_port);
            break;
        case AF_INET6:
            ASSERT(addr->len == sizeof(struct sockaddr_in6))
            BAddr_InitIPv6(out, addr->addr.ipv6.sin6_addr.s6_addr, addr->addr.ipv6.sin6_port);
            break;
        default:
            BAddr_InitNone(out);
            break;
    }
}

static void dispatch_event (BSocket *bs)
{
    ASSERT(!bs->global_handler)
    ASSERT(bs->current_event_index >= 0)
    ASSERT(bs->current_event_index < BSOCKET_NUM_EVENTS)
    ASSERT(((bs->ready_events)&~(bs->waitEvents)) == 0)
    
    do {
        // get event
        int ev_index = bs->current_event_index;
        int ev_mask = handler_events[ev_index];
        int ev_dispatch = (bs->ready_events&ev_mask);
        
        // jump to next event, clear this event
        bs->current_event_index++;
        bs->ready_events &= ~ev_mask;
        
        ASSERT(!(bs->ready_events) || bs->current_event_index < BSOCKET_NUM_EVENTS)
        
        if (ev_dispatch) {
            // if there are more events to dispatch, schedule job
            if (bs->ready_events) {
                BPending_Set(&bs->job);
            }
            
            // dispatch this event
            bs->handlers[ev_index](bs->handlers_user[ev_index], ev_mask);
            return;
        }
    } while (bs->current_event_index < BSOCKET_NUM_EVENTS);
    
    ASSERT(!bs->ready_events)
}

static void job_handler (BSocket *bs)
{
    ASSERT(!bs->global_handler) // BSocket_RemoveGlobalEventHandler unsets the job
    ASSERT(bs->current_event_index >= 0)
    ASSERT(bs->current_event_index < BSOCKET_NUM_EVENTS)
    ASSERT(((bs->ready_events)&~(bs->waitEvents)) == 0) // BSocket_DisableEvent clears events from ready_events
    DebugObject_Access(&bs->d_obj);
    
    dispatch_event(bs);
    return;
}

static void dispatch_events (BSocket *bs, int events)
{
    ASSERT((events&~(bs->waitEvents)) == 0)
    
    // reset recv number
    bs->recv_num = 0;
    
    if (bs->global_handler) {
        if (events) {
            bs->global_handler(bs->global_handler_user, events);
        }
        return;
    }
    
    bs->ready_events = events;
    bs->current_event_index = 0;
    
    dispatch_event(bs);
    return;
}

#ifdef BADVPN_USE_WINAPI

static long get_wsa_events (int sock_events)
{
    long res = 0;
    
    if ((sock_events&BSOCKET_READ)) {
        res |= FD_READ | FD_CLOSE;
    }
    if ((sock_events&BSOCKET_WRITE)) {
        res |= FD_WRITE | FD_CLOSE;
    }
    if ((sock_events&BSOCKET_ACCEPT)) {
        res |= FD_ACCEPT | FD_CLOSE;
    }
    if ((sock_events&BSOCKET_CONNECT)) {
        res |= FD_CONNECT | FD_CLOSE;
    }
    
    return res;
}

static void handle_handler (BSocket *bs)
{
    DebugObject_Access(&bs->d_obj);
    
    // enumerate network events and reset event
    WSANETWORKEVENTS events;
    int res = WSAEnumNetworkEvents(bs->socket, bs->event, &events);
    ASSERT_FORCE(res == 0)
    
    int returned_events = 0;
    
    if ((bs->waitEvents&BSOCKET_READ) && ((events.lNetworkEvents&FD_READ) || (events.lNetworkEvents&FD_CLOSE))) {
        returned_events |= BSOCKET_READ;
    }
    
    if ((bs->waitEvents&BSOCKET_WRITE) && ((events.lNetworkEvents&FD_WRITE) || (events.lNetworkEvents&FD_CLOSE))) {
        returned_events |= BSOCKET_WRITE;
    }
    
    if ((bs->waitEvents&BSOCKET_ACCEPT) && ((events.lNetworkEvents&FD_ACCEPT) || (events.lNetworkEvents&FD_CLOSE))) {
        returned_events |= BSOCKET_ACCEPT;
    }
    
    if ((bs->waitEvents&BSOCKET_CONNECT) && ((events.lNetworkEvents&FD_CONNECT) || (events.lNetworkEvents&FD_CLOSE))) {
        returned_events |= BSOCKET_CONNECT;
        
        // read connection attempt result
        ASSERT(bs->connecting_status == 1)
        bs->connecting_status = 2;
        if (events.iErrorCode[FD_CONNECT_BIT] == 0) {
            bs->connecting_result = BSOCKET_ERROR_NONE;
        } else {
            bs->connecting_result = translate_error(events.iErrorCode[FD_CONNECT_BIT]);
        }
    }
    
    if ((bs->waitEvents&BSOCKET_ERROR) && (events.lNetworkEvents&FD_CLOSE)) {
        returned_events |= BSOCKET_ERROR;
    }
    
    dispatch_events(bs, returned_events);
    return;
}

#else

static int get_reactor_fd_events (int sock_events)
{
    int res = 0;
    
    if ((sock_events&BSOCKET_READ) || (sock_events&BSOCKET_ACCEPT)) {
        res |= BREACTOR_READ;
    }
    if ((sock_events&BSOCKET_WRITE) || (sock_events&BSOCKET_CONNECT)) {
        res |= BREACTOR_WRITE;
    }

    return res;
}

static void file_descriptor_handler (BSocket *bs, int events)
{
    DebugObject_Access(&bs->d_obj);
    
    int returned_events = 0;
    
    if ((bs->waitEvents&BSOCKET_READ) && ((events&BREACTOR_READ) || (events&BREACTOR_ERROR))) {
        returned_events |= BSOCKET_READ;
    }
    
    if ((bs->waitEvents&BSOCKET_WRITE) && ((events&BREACTOR_WRITE) || (events&BREACTOR_ERROR))) {
        returned_events |= BSOCKET_WRITE;
    }
    
    if ((bs->waitEvents&BSOCKET_ACCEPT) && ((events&BREACTOR_READ) || (events&BREACTOR_ERROR))) {
        returned_events |= BSOCKET_ACCEPT;
    }
    
    if ((bs->waitEvents&BSOCKET_CONNECT) && ((events&BREACTOR_WRITE) || (events&BREACTOR_ERROR))) {
        returned_events |= BSOCKET_CONNECT;
        
        // read connection attempt result
        ASSERT(bs->connecting_status == 1)
        bs->connecting_status = 2;
        int result;
        socklen_t result_len = sizeof(result);
        int res = getsockopt(bs->socket, SOL_SOCKET, SO_ERROR, &result, &result_len);
        ASSERT_FORCE(res == 0)
        if (result == 0) {
            bs->connecting_result = BSOCKET_ERROR_NONE;
        } else {
            bs->connecting_result = translate_error(result);
        }
    }
    
    if ((bs->waitEvents&BSOCKET_ERROR) && (events&BREACTOR_ERROR)) {
        returned_events |= BSOCKET_ERROR;
    }
    
    dispatch_events(bs, returned_events);
    return;
}

#endif

static int init_event_backend (BSocket *bs)
{
    #ifdef BADVPN_USE_WINAPI
    if ((bs->event = WSACreateEvent()) == WSA_INVALID_EVENT) {
        return 0;
    }
    BHandle_Init(&bs->bhandle, bs->event, (BHandle_handler)handle_handler, bs);
    if (!BReactor_AddHandle(bs->bsys, &bs->bhandle)) {
        ASSERT_FORCE(WSACloseEvent(bs->event))
        return 0;
    }
    BReactor_EnableHandle(bs->bsys, &bs->bhandle);
    #else
    BFileDescriptor_Init(&bs->fd, bs->socket, (BFileDescriptor_handler)file_descriptor_handler, bs);
    if (!BReactor_AddFileDescriptor(bs->bsys, &bs->fd)) {
        return 0;
    }
    #endif
    
    return 1;
}

static void free_event_backend (BSocket *bs)
{
    #ifdef BADVPN_USE_WINAPI
    BReactor_RemoveHandle(bs->bsys, &bs->bhandle);
    ASSERT_FORCE(WSACloseEvent(bs->event))
    #else
    BReactor_RemoveFileDescriptor(bs->bsys, &bs->fd);
    #endif
}

static void update_event_backend (BSocket *bs)
{
    #ifdef BADVPN_USE_WINAPI
    ASSERT_FORCE(WSAEventSelect(bs->socket, bs->event, get_wsa_events(bs->waitEvents)) == 0)
    #else
    BReactor_SetFileDescriptorEvents(bs->bsys, &bs->fd, get_reactor_fd_events(bs->waitEvents));
    #endif
}

static int limit_recv (BSocket *bs)
{
    if (bs->recv_max > 0) {
        if (bs->recv_num >= bs->recv_max) {
            return 1;
        }
        bs->recv_num++;
    }
    
    return 0;
}

static int setup_pktinfo (int socket, int type, int domain)
{
    if (type == BSOCKET_TYPE_DGRAM) {
        switch (domain) {
            case BADDR_TYPE_IPV4:
                if (set_pktinfo(socket)) {
                    return 0;
                }
                break;
            case BADDR_TYPE_IPV6:
                if (set_pktinfo6(socket)) {
                    return 0;
                }
                break;
        }
    }
    
    return 1;
}

static void setup_winsock_exts (int socket, int type, BSocket *bs)
{
    #ifdef BADVPN_USE_WINAPI
    
    if (type == BSOCKET_TYPE_DGRAM) {
        DWORD out_bytes;
        
        // obtain WSARecvMsg
        GUID guid_recv = WSAID_WSARECVMSG;
        if (WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_recv, sizeof(guid_recv), &bs->WSARecvMsg, sizeof(bs->WSARecvMsg), &out_bytes, NULL, NULL) != 0) {
            bs->WSARecvMsg = NULL;
        }
        
        // obtain WSASendMsg
        GUID guid_send = WSAID_WSASENDMSG;
        if (WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_send, sizeof(guid_send), &bs->WSASendMsg, sizeof(bs->WSASendMsg), &out_bytes, NULL, NULL) != 0) {
            bs->WSASendMsg = NULL;
        }
    }
    
    #endif
}

int BSocket_GlobalInit (void)
{
    #ifdef BADVPN_USE_WINAPI
    
    WORD requested = MAKEWORD(2, 2);
    WSADATA wsadata;
    if (WSAStartup(requested, &wsadata) != 0) {
        goto fail0;
    }
    if (wsadata.wVersion != requested) {
        goto fail1;
    }
    
    return 0;
    
fail1:
    WSACleanup();
fail0:
    return -1;
    
    #else
    
    return 0;
    
    #endif
}

int BSocket_Init (BSocket *bs, BReactor *bsys, int domain, int type)
{
    // NOTE: if you change something here, you might also have to change BSocket_Accept
    
    // translate domain
    int sys_domain;
    switch (domain) {
        case BADDR_TYPE_IPV4:
            sys_domain = AF_INET;
            break;
        case BADDR_TYPE_IPV6:
            sys_domain = AF_INET6;
            break;
        #ifndef BADVPN_USE_WINAPI
        case BADDR_TYPE_UNIX:
            sys_domain = AF_UNIX;
            break;
        case BADDR_TYPE_PACKET:
            sys_domain = AF_PACKET;
            break;
        #endif
        default:
            ASSERT(0)
            return -1;
    }

    // translate type
    int sys_type;
    switch (type) {
        case BSOCKET_TYPE_STREAM:
            sys_type = SOCK_STREAM;
            break;
        case BSOCKET_TYPE_DGRAM:
            sys_type = SOCK_DGRAM;
            break;
        default:
            ASSERT(0)
            return -1;
    }

    // create socket
    int fd = socket(sys_domain, sys_type, 0);
    if (fd < 0) {
        DEBUG("socket() failed");
        goto fail0;
    }

    // set socket nonblocking
    if (set_nonblocking(fd) != 0) {
        DEBUG("set_nonblocking failed");
        goto fail1;
    }
    
    // set pktinfo if needed
    if (!setup_pktinfo(fd, type, domain)) {
        DEBUG("setup_pktinfo failed");
        goto fail1;
    }
    
    // setup winsock exts
    setup_winsock_exts(fd, type, bs);
    
    bs->bsys = bsys;
    bs->type = type;
    bs->domain = domain;
    bs->socket = fd;
    bs->error = BSOCKET_ERROR_NONE;
    init_handlers(bs);
    bs->waitEvents = 0;
    bs->connecting_status = 0;
    bs->recv_max = BSOCKET_DEFAULT_RECV_MAX;
    bs->recv_num = 0;
    bs->ready_events = 0; // just initialize it so we can clear them safely from BSocket_DisableEvent
    
    // init job
    BPending_Init(&bs->job, BReactor_PendingGroup(bsys), (BPending_handler)job_handler, bs);
    
    // initialize event backend
    if (!init_event_backend(bs)) {
        DEBUG("WARNING: init_event_backend failed");
        goto fail2;
    }
    
    DebugObject_Init(&bs->d_obj);
    
    return 0;
    
fail2:
    BPending_Free(&bs->job);
fail1:
    close_socket(fd);
fail0:
    return -1;
}

void BSocket_Free (BSocket *bs)
{
    DebugObject_Free(&bs->d_obj);
    
    // free event backend
    free_event_backend(bs);
    
    // free job
    BPending_Free(&bs->job);
    
    // close socket
    close_socket(bs->socket);
}

void BSocket_SetRecvMax (BSocket *bs, int max)
{
    DebugObject_Access(&bs->d_obj);
    
    ASSERT(max > 0 || max == -1)
    
    bs->recv_max = max;
    bs->recv_num = 0;
}

int BSocket_GetError (BSocket *bs)
{
    DebugObject_Access(&bs->d_obj);
    
    return bs->error;
}

void BSocket_AddGlobalEventHandler (BSocket *bs, BSocket_handler handler, void *user)
{
    DebugObject_Access(&bs->d_obj);
    
    ASSERT(handler)
    ASSERT(!bs->global_handler)
    for (int i = 0; i < BSOCKET_NUM_EVENTS; i++) {
        ASSERT(!bs->handlers[i])
    }
    
    bs->global_handler = handler;
    bs->global_handler_user = user;
    
    // stop event dispatching job
    BPending_Unset(&bs->job);
}

void BSocket_RemoveGlobalEventHandler (BSocket *bs)
{
    ASSERT(bs->global_handler)
    DebugObject_Access(&bs->d_obj);
    
    bs->global_handler = NULL;
    bs->waitEvents = 0;
}

void BSocket_SetGlobalEvents (BSocket *bs, int events)
{
    ASSERT(bs->global_handler)
    DebugObject_Access(&bs->d_obj);
    
    // update events
    bs->waitEvents = events;
    
    // give new events to event backend
    update_event_backend(bs);
}

void BSocket_AddEventHandler (BSocket *bs, uint8_t event, BSocket_handler handler, void *user)
{
    ASSERT(handler)
    ASSERT(!bs->global_handler)
    DebugObject_Access(&bs->d_obj);
    
    // get index
    int i = get_event_index(event);
    
    // event must not have handler
    ASSERT(!bs->handlers[i])
    
    // change handler
    bs->handlers[i] = handler;
    bs->handlers_user[i] = user;
}

void BSocket_RemoveEventHandler (BSocket *bs, uint8_t event)
{
    DebugObject_Access(&bs->d_obj);
    
    // get table index
    int i = get_event_index(event);
    
    // event must have handler
    ASSERT(bs->handlers[i])
    
    // disable event if enabled
    if (bs->waitEvents&event) {
        BSocket_DisableEvent(bs, event);
    }
    
    // change handler
    bs->handlers[i] = NULL;
}

void BSocket_EnableEvent (BSocket *bs, uint8_t event)
{
    DebugObject_Access(&bs->d_obj);
    
    #ifndef NDEBUG
    // check event and incompatible events
    switch (event) {
        case BSOCKET_READ:
        case BSOCKET_WRITE:
            ASSERT(!(bs->waitEvents&BSOCKET_ACCEPT))
            ASSERT(!(bs->waitEvents&BSOCKET_CONNECT))
            break;
        case BSOCKET_ACCEPT:
            ASSERT(!(bs->waitEvents&BSOCKET_READ))
            ASSERT(!(bs->waitEvents&BSOCKET_WRITE))
            ASSERT(!(bs->waitEvents&BSOCKET_CONNECT))
            break;
        case BSOCKET_CONNECT:
            ASSERT(!(bs->waitEvents&BSOCKET_READ))
            ASSERT(!(bs->waitEvents&BSOCKET_WRITE))
            ASSERT(!(bs->waitEvents&BSOCKET_ACCEPT))
            break;
        case BSOCKET_ERROR:
            break;
        default:
            ASSERT(0)
            break;
    }
    #endif
    
    // event must have handler
    ASSERT(bs->handlers[get_event_index(event)])
    
    // event must not be enabled
    ASSERT(!(bs->waitEvents&event))
    
    // update events
    bs->waitEvents |= event;
    
    // give new events to event backend
    update_event_backend(bs);
}

void BSocket_DisableEvent (BSocket *bs, uint8_t event)
{
    DebugObject_Access(&bs->d_obj);
    
    // check event and get index
    int index = get_event_index(event);
    
    // event must have handler
    ASSERT(bs->handlers[index])
    
    // event must be enabled
    ASSERT(bs->waitEvents&event)
    
    // update events
    bs->waitEvents &= ~event;
    bs->ready_events &= ~event;
    
    // give new events to event backend
    update_event_backend(bs);
}

int BSocket_Connect (BSocket *bs, BAddr *addr)
{
    ASSERT(addr)
    ASSERT(!BAddr_IsInvalid(addr))
    ASSERT(bs->connecting_status == 0)
    DebugObject_Access(&bs->d_obj);
    
    struct sys_addr sysaddr;
    addr_socket_to_sys(&sysaddr, addr);

    if (connect(bs->socket, &sysaddr.addr.generic, sysaddr.len) < 0) {
        int error;
        #ifdef BADVPN_USE_WINAPI
        switch ((error = WSAGetLastError())) {
            case WSAEWOULDBLOCK:
                bs->connecting_status = 1;
                bs->error = BSOCKET_ERROR_IN_PROGRESS;
                return -1;
        }
        #else
        switch ((error = errno)) {
            case EINPROGRESS:
                bs->connecting_status = 1;
                bs->error = BSOCKET_ERROR_IN_PROGRESS;
                return -1;
        }
        #endif
        
        bs->error = translate_error(error);
        return -1;
    }

    bs->error = BSOCKET_ERROR_NONE;
    return 0;
}

int BSocket_GetConnectResult (BSocket *bs)
{
    ASSERT(bs->connecting_status == 2)
    DebugObject_Access(&bs->d_obj);

    bs->connecting_status = 0;
    
    return bs->connecting_result;
}

int BSocket_Bind (BSocket *bs, BAddr *addr)
{
    ASSERT(addr)
    ASSERT(!BAddr_IsInvalid(addr))
    DebugObject_Access(&bs->d_obj);
    
    struct sys_addr sysaddr;
    addr_socket_to_sys(&sysaddr, addr);
    
    if (bs->type == BSOCKET_TYPE_STREAM) {
        #ifdef BADVPN_USE_WINAPI
        BOOL optval = TRUE;
        #else
        int optval = 1;
        #endif
        if (setsockopt(bs->socket, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval)) < 0) {
            DEBUG("WARNING: setsockopt failed");
        }
    }
    
    if (bind(bs->socket, &sysaddr.addr.generic, sysaddr.len) < 0) {
        #ifdef BADVPN_USE_WINAPI
        int error = WSAGetLastError();
        #else
        int error = errno;
        #endif
        
        bs->error = translate_error(error);
        return -1;
    }
    
    bs->error = BSOCKET_ERROR_NONE;
    return 0;
}

int BSocket_Listen (BSocket *bs, int backlog)
{
    ASSERT(bs->type == BSOCKET_TYPE_STREAM)
    DebugObject_Access(&bs->d_obj);
    
    if (backlog < 0) {
        backlog = BSOCKET_DEFAULT_BACKLOG;
    }
    
    if (listen(bs->socket, backlog) < 0) {
        #ifdef BADVPN_USE_WINAPI
        int error = WSAGetLastError();
        #else
        int error = errno;
        #endif
        
        bs->error = translate_error(error);
        return -1;
    }

    bs->error = BSOCKET_ERROR_NONE;
    return 0;
}

int BSocket_Accept (BSocket *bs, BSocket *newsock, BAddr *addr)
{
    ASSERT(bs->type == BSOCKET_TYPE_STREAM)
    DebugObject_Access(&bs->d_obj);
    
    struct sys_addr sysaddr;
    sysaddr.len = sizeof(sysaddr.addr);
    
    int fd = accept(bs->socket, &sysaddr.addr.generic, &sysaddr.len);
    if (fd < 0) {
        int error;
        #ifdef BADVPN_USE_WINAPI
        switch ((error = WSAGetLastError())) {
            case WSAEWOULDBLOCK:
                bs->error = BSOCKET_ERROR_LATER;
                return -1;
        }
        #else
        switch ((error = errno)) {
            case EAGAIN:
            #if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
            #endif
                bs->error = BSOCKET_ERROR_LATER;
                return -1;
        }
        #endif
        
        bs->error = translate_error(error);
        return -1;
    }
    
    if (!newsock) {
        close_socket(fd);
    } else {
        // set nonblocking
        if (set_nonblocking(fd) != 0) {
            DEBUG("WARNING: set_nonblocking failed");
            goto fail0;
        }
        
        // set pktinfo if needed
        if (!setup_pktinfo(fd, bs->type, bs->domain)) {
            DEBUG("setup_pktinfo failed");
            goto fail0;
        }
        
        // setup winsock exts
        setup_winsock_exts(fd, bs->type, newsock);
        
        newsock->bsys = bs->bsys;
        newsock->type = bs->type;
        newsock->domain = bs->domain;
        newsock->socket = fd;
        newsock->error = BSOCKET_ERROR_NONE;
        init_handlers(newsock);
        newsock->waitEvents = 0;
        newsock->connecting_status = 0;
        newsock->recv_max = BSOCKET_DEFAULT_RECV_MAX;
        newsock->recv_num = 0;
        newsock->ready_events = 0; // just initialize it so we can clear them safely from BSocket_DisableEvent
        
        // init job
        BPending_Init(&newsock->job, BReactor_PendingGroup(bs->bsys), (BPending_handler)job_handler, newsock);
    
        if (!init_event_backend(newsock)) {
            DEBUG("WARNING: init_event_backend failed");
            goto fail1;
        }
        
        // init debug object
        DebugObject_Init(&newsock->d_obj);
    }
    
    // return client address
    if (addr) {
        addr_sys_to_socket(addr, &sysaddr);
    }
    
    bs->error = BSOCKET_ERROR_NONE;
    return 0;
    
fail1:
    BPending_Free(&newsock->job);
fail0:
    close_socket(fd);
    bs->error = BSOCKET_ERROR_UNKNOWN;
    return -1;
}

int BSocket_Send (BSocket *bs, uint8_t *data, int len)
{
    ASSERT(len >= 0)
    ASSERT(bs->type == BSOCKET_TYPE_STREAM)
    DebugObject_Access(&bs->d_obj);
    
    #ifdef BADVPN_USE_WINAPI
    int flags = 0;
    #else
    int flags = MSG_NOSIGNAL;
    #endif
    
    int bytes = send(bs->socket, data, len, flags);
    if (bytes < 0) {
        int error;
        #ifdef BADVPN_USE_WINAPI
        switch ((error = WSAGetLastError())) {
            case WSAEWOULDBLOCK:
                bs->error = BSOCKET_ERROR_LATER;
                return -1;
        }
        #else
        switch ((error = errno)) {
            case EAGAIN:
            #if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
            #endif
                bs->error = BSOCKET_ERROR_LATER;
                return -1;
        }
        #endif
        
        bs->error = translate_error(error);
        return -1;
    }
    
    bs->error = BSOCKET_ERROR_NONE;
    return bytes;
}

int BSocket_Recv (BSocket *bs, uint8_t *data, int len)
{
    ASSERT(len >= 0)
    ASSERT(bs->type == BSOCKET_TYPE_STREAM)
    DebugObject_Access(&bs->d_obj);
    
    if (limit_recv(bs)) {
        bs->error = BSOCKET_ERROR_LATER;
        return -1;
    }
    
    int bytes = recv(bs->socket, data, len, 0);
    if (bytes < 0) {
        int error;
        #ifdef BADVPN_USE_WINAPI
        switch ((error = WSAGetLastError())) {
            case WSAEWOULDBLOCK:
                bs->error = BSOCKET_ERROR_LATER;
                return -1;
        }
        #else
        switch ((error = errno)) {
            case EAGAIN:
            #if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
            #endif
                bs->error = BSOCKET_ERROR_LATER;
                return -1;
        }
        #endif
        
        bs->error = translate_error(error);
        return -1;
    }
    
    bs->error = BSOCKET_ERROR_NONE;
    return bytes;
}

int BSocket_SendToFrom (BSocket *bs, uint8_t *data, int len, BAddr *addr, BIPAddr *local_addr)
{
    ASSERT(len >= 0)
    ASSERT(addr)
    ASSERT(!BAddr_IsInvalid(addr))
    ASSERT(local_addr)
    ASSERT(bs->type == BSOCKET_TYPE_DGRAM)
    DebugObject_Access(&bs->d_obj);
    
    struct sys_addr remote_sysaddr;
    addr_socket_to_sys(&remote_sysaddr, addr);
    
    #ifdef BADVPN_USE_WINAPI
    
    WSABUF buf;
    buf.len = len;
    buf.buf = data;
    
    DWORD bytes;
    
    if (!bs->WSASendMsg) {
        if (WSASendTo(bs->socket, &buf, 1, &bytes, 0, &remote_sysaddr.addr.generic, remote_sysaddr.len, NULL, NULL) != 0) {
            int error;
            switch ((error = WSAGetLastError())) {
                case WSAEWOULDBLOCK:
                    bs->error = BSOCKET_ERROR_LATER;
                    return -1;
            }
            
            bs->error = translate_error(error);
            return -1;
        }
    } else {
        union {
            char in[WSA_CMSG_SPACE(sizeof(struct in_pktinfo))];
            char in6[WSA_CMSG_SPACE(sizeof(struct in6_pktinfo))];
        } cdata;
        
        WSAMSG msg;
        memset(&msg, 0, sizeof(msg));
        msg.name = &remote_sysaddr.addr.generic;
        msg.namelen = remote_sysaddr.len;
        msg.lpBuffers = &buf;
        msg.dwBufferCount = 1;
        msg.Control.buf = (char *)&cdata;
        msg.Control.len = sizeof(cdata);
        
        int sum = 0;
        
        WSACMSGHDR *cmsg = WSA_CMSG_FIRSTHDR(&msg);
        
        switch (local_addr->type) {
            case BADDR_TYPE_NONE:
                break;
            case BADDR_TYPE_IPV4: {
                memset(cmsg, 0, WSA_CMSG_SPACE(sizeof(struct in_pktinfo)));
                cmsg->cmsg_level = IPPROTO_IP;
                cmsg->cmsg_type = IP_PKTINFO;
                cmsg->cmsg_len = WSA_CMSG_LEN(sizeof(struct in_pktinfo));
                struct in_pktinfo *pktinfo = (struct in_pktinfo *)WSA_CMSG_DATA(cmsg);
                pktinfo->ipi_addr.s_addr = local_addr->ipv4;
                sum += WSA_CMSG_SPACE(sizeof(struct in_pktinfo));
            } break;
            case BADDR_TYPE_IPV6: {
                memset(cmsg, 0, WSA_CMSG_SPACE(sizeof(struct in6_pktinfo)));
                cmsg->cmsg_level = IPPROTO_IPV6;
                cmsg->cmsg_type = IPV6_PKTINFO;
                cmsg->cmsg_len = WSA_CMSG_LEN(sizeof(struct in6_pktinfo));
                struct in6_pktinfo *pktinfo = (struct in6_pktinfo *)WSA_CMSG_DATA(cmsg);
                memcpy(pktinfo->ipi6_addr.s6_addr, local_addr->ipv6, 16);
                sum += WSA_CMSG_SPACE(sizeof(struct in6_pktinfo));
            } break;
            default:
                ASSERT(0);
        }
        
        msg.Control.len = sum;
        
        if (bs->WSASendMsg(bs->socket, &msg, 0, &bytes, NULL, NULL) != 0) {
            int error;
            switch ((error = WSAGetLastError())) {
                case WSAEWOULDBLOCK:
                    bs->error = BSOCKET_ERROR_LATER;
                    return -1;
            }
            
            bs->error = translate_error(error);
            return -1;
        }
    }
    
    #else
    
    struct iovec iov;
    iov.iov_base = data;
    iov.iov_len = len;
    
    union {
        char in[CMSG_SPACE(sizeof(struct in_pktinfo))];
        char in6[CMSG_SPACE(sizeof(struct in6_pktinfo))];
    } cdata;
    
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &remote_sysaddr.addr.generic;
    msg.msg_namelen = remote_sysaddr.len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = &cdata;
    msg.msg_controllen = sizeof(cdata);
    
    int sum = 0;
    
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    
    switch (local_addr->type) {
        case BADDR_TYPE_NONE:
            break;
        case BADDR_TYPE_IPV4: {
            memset(cmsg, 0, CMSG_SPACE(sizeof(struct in_pktinfo)));
            cmsg->cmsg_level = IPPROTO_IP;
            cmsg->cmsg_type = IP_PKTINFO;
            cmsg->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));
            struct in_pktinfo *pktinfo = (struct in_pktinfo *)CMSG_DATA(cmsg);
            pktinfo->ipi_spec_dst.s_addr = local_addr->ipv4;
            sum += CMSG_SPACE(sizeof(struct in_pktinfo));
        } break;
        case BADDR_TYPE_IPV6: {
            memset(cmsg, 0, CMSG_SPACE(sizeof(struct in6_pktinfo)));
            cmsg->cmsg_level = IPPROTO_IPV6;
            cmsg->cmsg_type = IPV6_PKTINFO;
            cmsg->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
            struct in6_pktinfo *pktinfo = (struct in6_pktinfo *)CMSG_DATA(cmsg);
            memcpy(pktinfo->ipi6_addr.s6_addr, local_addr->ipv6, 16);
            sum += CMSG_SPACE(sizeof(struct in6_pktinfo));
        } break;
        default:
            ASSERT(0);
    }
    
    msg.msg_controllen = sum;
    
    int bytes = sendmsg(bs->socket, &msg, MSG_NOSIGNAL);
    if (bytes < 0) {
        int error;
        switch ((error = errno)) {
            case EAGAIN:
            #if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
            #endif
                bs->error = BSOCKET_ERROR_LATER;
                return -1;
        }
        
        bs->error = translate_error(error);
        return -1;
    }
    
    #endif
    
    bs->error = BSOCKET_ERROR_NONE;
    return bytes;
}

int BSocket_RecvFromTo (BSocket *bs, uint8_t *data, int len, BAddr *addr, BIPAddr *local_addr)
{
    ASSERT(len >= 0)
    ASSERT(addr)
    ASSERT(local_addr)
    ASSERT(bs->type == BSOCKET_TYPE_DGRAM)
    DebugObject_Access(&bs->d_obj);
    
    if (limit_recv(bs)) {
        bs->error = BSOCKET_ERROR_LATER;
        return -1;
    }
    
    struct sys_addr remote_sysaddr;
    remote_sysaddr.len = sizeof(remote_sysaddr.addr);
    
    #ifdef BADVPN_USE_WINAPI
    
    WSABUF buf;
    buf.len = len;
    buf.buf = data;
    
    DWORD bytes;
    
    WSAMSG msg;
    
    if (!bs->WSARecvMsg) {
        DWORD flags = 0;
        INT fromlen = remote_sysaddr.len;
        if (WSARecvFrom(bs->socket, &buf, 1, &bytes, &flags, &remote_sysaddr.addr.generic, &fromlen, NULL, NULL) != 0) {
            int error;
            switch ((error = WSAGetLastError())) {
                case WSAEWOULDBLOCK:
                    bs->error = BSOCKET_ERROR_LATER;
                    return -1;
            }
            
            bs->error = translate_error(error);
            return -1;
        }
        
        remote_sysaddr.len = fromlen;
    } else {
        union {
            char in[WSA_CMSG_SPACE(sizeof(struct in_pktinfo))];
            char in6[WSA_CMSG_SPACE(sizeof(struct in6_pktinfo))];
        } cdata;
        
        memset(&msg, 0, sizeof(msg));
        msg.name = &remote_sysaddr.addr.generic;
        msg.namelen = remote_sysaddr.len;
        msg.lpBuffers = &buf;
        msg.dwBufferCount = 1;
        msg.Control.buf = (char *)&cdata;
        msg.Control.len = sizeof(cdata);
        
        if (bs->WSARecvMsg(bs->socket, &msg, &bytes, NULL, NULL) != 0) {
            int error;
            switch ((error = WSAGetLastError())) {
                case WSAEWOULDBLOCK:
                    bs->error = BSOCKET_ERROR_LATER;
                    return -1;
            }
            
            bs->error = translate_error(error);
            return -1;
        }
        
        remote_sysaddr.len = msg.namelen;
    }
    
    #else
        
    struct iovec iov;
    iov.iov_base = data;
    iov.iov_len = len;
    
    union {
        char in[CMSG_SPACE(sizeof(struct in_pktinfo))];
        char in6[CMSG_SPACE(sizeof(struct in6_pktinfo))];
    } cdata;
    
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &remote_sysaddr.addr.generic;
    msg.msg_namelen = remote_sysaddr.len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = &cdata;
    msg.msg_controllen = sizeof(cdata);
    
    int bytes = recvmsg(bs->socket, &msg, 0);
    if (bytes < 0) {
        int error;
        switch ((error = errno)) {
            case EAGAIN:
            #if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
            #endif
                bs->error = BSOCKET_ERROR_LATER;
                return -1;
        }
        
        bs->error = translate_error(error);
        return -1;
    }
    
    remote_sysaddr.len = msg.msg_namelen;
    
    #endif
    
    addr_sys_to_socket(addr, &remote_sysaddr);
    BIPAddr_InitInvalid(local_addr);
    
    #ifdef BADVPN_USE_WINAPI
    
    if (bs->WSARecvMsg) {
        WSACMSGHDR *cmsg;
        for (cmsg = WSA_CMSG_FIRSTHDR(&msg); cmsg; cmsg = WSA_CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
                struct in_pktinfo *pktinfo = (struct in_pktinfo *)WSA_CMSG_DATA(cmsg);
                BIPAddr_InitIPv4(local_addr, pktinfo->ipi_addr.s_addr);
            }
            else if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
                struct in6_pktinfo *pktinfo = (struct in6_pktinfo *)WSA_CMSG_DATA(cmsg);
                BIPAddr_InitIPv6(local_addr, pktinfo->ipi6_addr.s6_addr);
            }
        }
    }
    
    #else
    
    struct cmsghdr *cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
            struct in_pktinfo *pktinfo = (struct in_pktinfo *)CMSG_DATA(cmsg);
            BIPAddr_InitIPv4(local_addr, pktinfo->ipi_addr.s_addr);
        }
        else if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
            struct in6_pktinfo *pktinfo = (struct in6_pktinfo *)CMSG_DATA(cmsg);
            BIPAddr_InitIPv6(local_addr, pktinfo->ipi6_addr.s6_addr);
        }
    }
    
    #endif
    
    bs->error = BSOCKET_ERROR_NONE;
    return bytes;
}

int BSocket_GetPeerName (BSocket *bs, BAddr *addr)
{
    ASSERT(addr)
    DebugObject_Access(&bs->d_obj);
    
    struct sys_addr sysaddr;
    sysaddr.len = sizeof(sysaddr.addr);
    
    if (getpeername(bs->socket, &sysaddr.addr.generic, &sysaddr.len) < 0) {
        bs->error = BSOCKET_ERROR_UNKNOWN;
        return -1;
    }
    
    addr_sys_to_socket(addr, &sysaddr);
    
    bs->error = BSOCKET_ERROR_NONE;
    return 0;
}

#ifndef BADVPN_USE_WINAPI

static int create_unix_sysaddr (struct sockaddr_un *addr, size_t *addr_len, const char *path)
{
    size_t path_len = strlen(path);
    
    if (path_len == 0) {
        DEBUG("path empty");
        return 0;
    }
    
    addr->sun_family = AF_UNIX;
    
    if (path_len >= sizeof(addr->sun_path)) {
        DEBUG("path too long");
        return 0;
    }
    
    strcpy(addr->sun_path, path);
    
    *addr_len = offsetof(struct sockaddr_un, sun_path) + path_len + 1;
    
    return 1;
}

int BSocket_BindUnix (BSocket *bs, const char *path)
{
    DebugObject_Access(&bs->d_obj);
    
    struct sockaddr_un sys_addr;
    size_t addr_len;
    
    if (!create_unix_sysaddr(&sys_addr, &addr_len, path)) {
        bs->error = BSOCKET_ERROR_UNKNOWN;
        return -1;
    }
    
    if (bind(bs->socket, (struct sockaddr *)&sys_addr, addr_len) < 0) {
        bs->error = translate_error(errno);
        return -1;
    }
    
    bs->error = BSOCKET_ERROR_NONE;
    return 0;
}

int BSocket_ConnectUnix (BSocket *bs, const char *path)
{
    DebugObject_Access(&bs->d_obj);
    
    struct sockaddr_un sys_addr;
    size_t addr_len;
    
    if (!create_unix_sysaddr(&sys_addr, &addr_len, path)) {
        bs->error = BSOCKET_ERROR_UNKNOWN;
        return -1;
    }
    
    if (connect(bs->socket, (struct sockaddr *)&sys_addr, addr_len) < 0) {
        bs->error = translate_error(errno);
        return -1;
    }
    
    bs->error = BSOCKET_ERROR_NONE;
    return 0;
}

#endif

BReactor * BSocket_Reactor (BSocket *bs)
{
    DebugObject_Access(&bs->d_obj);
    
    return bs->bsys;
}
