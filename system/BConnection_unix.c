/**
 * @file BConnection_unix.c
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

#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <misc/nonblocking.h>
#include <base/BLog.h>

#include "BConnection.h"

#include <generated/blog_channel_BConnection.h>

#define MAX_UNIX_SOCKET_PATH 200

struct sys_addr {
    socklen_t len;
    union {
        struct sockaddr generic;
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
    } addr;
};

struct unix_addr {
    socklen_t len;
    union {
        struct sockaddr_un addr;
        uint8_t bytes[offsetof(struct sockaddr_un, sun_path) + MAX_UNIX_SOCKET_PATH + 1];
    } u;
};

static int build_unix_address (struct unix_addr *out, const char *socket_path);
static void addr_socket_to_sys (struct sys_addr *out, BAddr addr);
static void addr_sys_to_socket (BAddr *out, struct sys_addr addr);
static void listener_fd_handler (BListener *o, int events);
static void listener_default_job_handler (BListener *o);
static void connector_fd_handler (BConnector *o, int events);
static void connector_job_handler (BConnector *o);
static void connection_report_error (BConnection *o);
static void connection_send (BConnection *o);
static void connection_recv (BConnection *o);
static void connection_fd_handler (BConnection *o, int events);
static void connection_send_job_handler (BConnection *o);
static void connection_recv_job_handler (BConnection *o);
static void connection_send_if_handler_send (BConnection *o, uint8_t *data, int data_len);
static void connection_recv_if_handler_recv (BConnection *o, uint8_t *data, int data_len);

static int build_unix_address (struct unix_addr *out, const char *socket_path)
{
    ASSERT(socket_path);
    
    if (strlen(socket_path) > MAX_UNIX_SOCKET_PATH) {
        return 0;
    }
    
    out->len = offsetof(struct sockaddr_un, sun_path) + strlen(socket_path) + 1;
    out->u.addr.sun_family = AF_UNIX;
    strcpy(out->u.addr.sun_path, socket_path);
    
    return 1;
}

static void addr_socket_to_sys (struct sys_addr *out, BAddr addr)
{
    switch (addr.type) {
        case BADDR_TYPE_IPV4: {
            out->len = sizeof(out->addr.ipv4);
            memset(&out->addr.ipv4, 0, sizeof(out->addr.ipv4));
            out->addr.ipv4.sin_family = AF_INET;
            out->addr.ipv4.sin_port = addr.ipv4.port;
            out->addr.ipv4.sin_addr.s_addr = addr.ipv4.ip;
        } break;
        
        case BADDR_TYPE_IPV6: {
            out->len = sizeof(out->addr.ipv6);
            memset(&out->addr.ipv6, 0, sizeof(out->addr.ipv6));
            out->addr.ipv6.sin6_family = AF_INET6;
            out->addr.ipv6.sin6_port = addr.ipv6.port;
            out->addr.ipv6.sin6_flowinfo = 0;
            memcpy(out->addr.ipv6.sin6_addr.s6_addr, addr.ipv6.ip, 16);
            out->addr.ipv6.sin6_scope_id = 0;
        } break;
        
        default: ASSERT(0);
    }
}

static void addr_sys_to_socket (BAddr *out, struct sys_addr addr)
{
    switch (addr.addr.generic.sa_family) {
        case AF_INET: {
            ASSERT(addr.len == sizeof(struct sockaddr_in))
            BAddr_InitIPv4(out, addr.addr.ipv4.sin_addr.s_addr, addr.addr.ipv4.sin_port);
        } break;
        
        case AF_INET6: {
            ASSERT(addr.len == sizeof(struct sockaddr_in6))
            BAddr_InitIPv6(out, addr.addr.ipv6.sin6_addr.s6_addr, addr.addr.ipv6.sin6_port);
        } break;
        
        default: {
            BAddr_InitNone(out);
        } break;
    }
}

static void listener_fd_handler (BListener *o, int events)
{
    DebugObject_Access(&o->d_obj);
    
    // set default job
    BPending_Set(&o->default_job);
    
    // call handler
    o->handler(o->user);
    return;
}

static void listener_default_job_handler (BListener *o)
{
    DebugObject_Access(&o->d_obj);
    
    BLog(BLOG_ERROR, "discarding connection");
    
    // accept
    int newfd = accept(o->fd, NULL, NULL);
    if (newfd < 0) {
        BLog(BLOG_ERROR, "accept failed");
        return;
    }
    
    // close new fd
    if (close(newfd) < 0) {
        BLog(BLOG_ERROR, "close failed");
    }
}

static void connector_fd_handler (BConnector *o, int events)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->fd >= 0)
    ASSERT(!o->connected)
    ASSERT(o->have_bfd)
    
    // free BFileDescriptor
    BReactor_RemoveFileDescriptor(o->reactor, &o->bfd);
    
    // set have no BFileDescriptor
    o->have_bfd = 0;
    
    // read connection result
    int result;
    socklen_t result_len = sizeof(result);
    if (getsockopt(o->fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) {
        BLog(BLOG_ERROR, "getsockopt failed");
        goto fail0;
    }
    ASSERT_FORCE(result_len == sizeof(result))
    
    if (result != 0) {
        BLog(BLOG_ERROR, "connection failed");
        goto fail0;
    }
    
    // set connected
    o->connected = 1;
    
fail0:
    // call handler
    o->handler(o->user, !o->connected);
    return;
}

static void connector_job_handler (BConnector *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->fd >= 0)
    ASSERT(o->connected)
    ASSERT(!o->have_bfd)
    
    // call handler
    o->handler(o->user, 0);
    return;
}

static void connection_report_error (BConnection *o)
{
    DebugError_AssertNoError(&o->d_err);
    ASSERT(o->handler)
    
    // report error
    DEBUGERROR(&o->d_err, o->handler(o->user, BCONNECTION_EVENT_ERROR));
    return;
}

static void connection_send (BConnection *o)
{
    DebugError_AssertNoError(&o->d_err);
    ASSERT(o->send.inited)
    ASSERT(o->send.busy)
    
    // limit
    if (!BReactorLimit_Increment(&o->send.limit)) {
        // wait for fd
        o->wait_events |= BREACTOR_WRITE;
        BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->wait_events);
        return;
    }
    
    // send
    int bytes = write(o->fd, o->send.busy_data, o->send.busy_data_len);
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // wait for fd
            o->wait_events |= BREACTOR_WRITE;
            BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->wait_events);
            return;
        }
        
        BLog(BLOG_ERROR, "send failed");
        connection_report_error(o);
        return;
    }
    
    ASSERT(bytes > 0)
    ASSERT(bytes <= o->send.busy_data_len)
    
    // set not busy
    o->send.busy = 0;
    
    // done
    StreamPassInterface_Done(&o->send.iface, bytes);
}

static void connection_recv (BConnection *o)
{
    DebugError_AssertNoError(&o->d_err);
    ASSERT(o->recv.inited)
    ASSERT(o->recv.busy)
    ASSERT(!o->recv.closed)
    
    // limit
    if (!BReactorLimit_Increment(&o->recv.limit)) {
        // wait for fd
        o->wait_events |= BREACTOR_READ;
        BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->wait_events);
        return;
    }
    
    // recv
    int bytes = read(o->fd, o->recv.busy_data, o->recv.busy_data_avail);
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // wait for fd
            o->wait_events |= BREACTOR_READ;
            BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->wait_events);
            return;
        }
        
        BLog(BLOG_ERROR, "recv failed");
        connection_report_error(o);
        return;
    }
    
    if (bytes == 0) {
        // set recv closed
        o->recv.closed = 1;
        
        // report recv closed
        o->handler(o->user, BCONNECTION_EVENT_RECVCLOSED);
        return;
    }
    
    ASSERT(bytes > 0)
    ASSERT(bytes <= o->recv.busy_data_avail)
    
    // set not busy
    o->recv.busy = 0;
    
    // done
    StreamRecvInterface_Done(&o->recv.iface, bytes);
}

static void connection_fd_handler (BConnection *o, int events)
{
    DebugObject_Access(&o->d_obj);
    DebugError_AssertNoError(&o->d_err);
    
    // clear handled events
    o->wait_events &= ~events;
    BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->wait_events);
    
    int have_send = 0;
    int have_recv = 0;
    
    if ((events & BREACTOR_WRITE) || ((events & BREACTOR_ERROR) && o->send.inited && o->send.busy)) {
        ASSERT(o->send.inited)
        ASSERT(o->send.busy)
        
        have_send = 1;
    }
    
    if ((events & BREACTOR_READ) || ((events & BREACTOR_ERROR) && o->recv.inited && o->recv.busy && !o->recv.closed)) {
        ASSERT(o->recv.inited)
        ASSERT(o->recv.busy)
        ASSERT(!o->recv.closed)
        
        have_recv = 1;
    }
    
    if (have_send) {
        if (have_recv) {
            BPending_Set(&o->recv.job);
        }
        
        connection_send(o);
        return;
    }
    
    if (have_recv) {
        connection_recv(o);
        return;
    }
    
    BLog(BLOG_ERROR, "fd error event");
    connection_report_error(o);
    return;
}

static void connection_send_job_handler (BConnection *o)
{
    DebugObject_Access(&o->d_obj);
    DebugError_AssertNoError(&o->d_err);
    ASSERT(o->send.inited)
    ASSERT(o->send.busy)
    
    connection_send(o);
    return;
}

static void connection_recv_job_handler (BConnection *o)
{
    DebugObject_Access(&o->d_obj);
    DebugError_AssertNoError(&o->d_err);
    ASSERT(o->recv.inited)
    ASSERT(o->recv.busy)
    ASSERT(!o->recv.closed)
    
    connection_recv(o);
    return;
}

static void connection_send_if_handler_send (BConnection *o, uint8_t *data, int data_len)
{
    DebugObject_Access(&o->d_obj);
    DebugError_AssertNoError(&o->d_err);
    ASSERT(o->send.inited)
    ASSERT(!o->send.busy)
    ASSERT(data_len > 0)
    
    // remember data
    o->send.busy_data = data;
    o->send.busy_data_len = data_len;
    
    // set busy
    o->send.busy = 1;
    
    connection_send(o);
    return;
}

static void connection_recv_if_handler_recv (BConnection *o, uint8_t *data, int data_avail)
{
    DebugObject_Access(&o->d_obj);
    DebugError_AssertNoError(&o->d_err);
    ASSERT(o->recv.inited)
    ASSERT(!o->recv.busy)
    ASSERT(!o->recv.closed)
    ASSERT(data_avail > 0)
    
    // remember data
    o->recv.busy_data = data;
    o->recv.busy_data_avail = data_avail;
    
    // set busy
    o->recv.busy = 1;
    
    connection_recv(o);
    return;
}

int BConnection_AddressSupported (BAddr addr)
{
    BAddr_Assert(&addr);
    
    return (addr.type == BADDR_TYPE_IPV4 || addr.type == BADDR_TYPE_IPV6);
}

int BListener_Init (BListener *o, BAddr addr, BReactor *reactor, void *user,
                    BListener_handler handler)
{
    ASSERT(BConnection_AddressSupported(addr))
    ASSERT(handler)
    BNetwork_Assert();
    
    // init arguments
    o->reactor = reactor;
    o->user = user;
    o->handler = handler;
    
    // convert address
    struct sys_addr sysaddr;
    addr_socket_to_sys(&sysaddr, addr);
    
    // init fd
    if ((o->fd = socket(sysaddr.addr.generic.sa_family, SOCK_STREAM, 0)) < 0) {
        BLog(BLOG_ERROR, "socket failed");
        goto fail0;
    }
    
    // set non-blocking
    if (!badvpn_set_nonblocking(o->fd)) {
        BLog(BLOG_ERROR, "badvpn_set_nonblocking failed");
        goto fail1;
    }
    
    // set SO_REUSEADDR
    int optval = 1;
    if (setsockopt(o->fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        BLog(BLOG_ERROR, "setsockopt(SO_REUSEADDR) failed");
    }
    
    // bind
    if (bind(o->fd, &sysaddr.addr.generic, sysaddr.len) < 0) {
        BLog(BLOG_ERROR, "bind failed");
        goto fail1;
    }
    
    // listen
    if (listen(o->fd, BCONNECTION_LISTEN_BACKLOG) < 0) {
        BLog(BLOG_ERROR, "listen failed");
        goto fail1;
    }
    
    // init BFileDescriptor
    BFileDescriptor_Init(&o->bfd, o->fd, (BFileDescriptor_handler)listener_fd_handler, o);
    if (!BReactor_AddFileDescriptor(o->reactor, &o->bfd)) {
        BLog(BLOG_ERROR, "BReactor_AddFileDescriptor failed");
        goto fail1;
    }
    BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, BREACTOR_READ);
    
    // init default job
    BPending_Init(&o->default_job, BReactor_PendingGroup(o->reactor), (BPending_handler)listener_default_job_handler, o);
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail1:
    if (close(o->fd) < 0) {
        BLog(BLOG_ERROR, "close failed");
    }
fail0:
    return 0;
}

int BListener_InitUnix (BListener *o, const char *socket_path, BReactor *reactor, void *user,
                        BListener_handler handler)
{
    ASSERT(socket_path)
    ASSERT(handler)
    BNetwork_Assert();
    
    // init arguments
    o->reactor = reactor;
    o->user = user;
    o->handler = handler;
    
    // build address
    struct unix_addr addr;
    if (!build_unix_address(&addr, socket_path)) {
        BLog(BLOG_ERROR, "build_unix_address failed");
        goto fail0;
    }
    
    // init fd
    if ((o->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        BLog(BLOG_ERROR, "socket failed");
        goto fail0;
    }
    
    // set non-blocking
    if (!badvpn_set_nonblocking(o->fd)) {
        BLog(BLOG_ERROR, "badvpn_set_nonblocking failed");
        goto fail1;
    }
    
    // bind
    if (bind(o->fd, (struct sockaddr *)&addr.u.addr, addr.len) < 0) {
        BLog(BLOG_ERROR, "bind failed");
        goto fail1;
    }
    
    // listen
    if (listen(o->fd, BCONNECTION_LISTEN_BACKLOG) < 0) {
        BLog(BLOG_ERROR, "listen failed");
        goto fail1;
    }
    
    // init BFileDescriptor
    BFileDescriptor_Init(&o->bfd, o->fd, (BFileDescriptor_handler)listener_fd_handler, o);
    if (!BReactor_AddFileDescriptor(o->reactor, &o->bfd)) {
        BLog(BLOG_ERROR, "BReactor_AddFileDescriptor failed");
        goto fail1;
    }
    BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, BREACTOR_READ);
    
    // init default job
    BPending_Init(&o->default_job, BReactor_PendingGroup(o->reactor), (BPending_handler)listener_default_job_handler, o);
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail1:
    if (close(o->fd) < 0) {
        BLog(BLOG_ERROR, "close failed");
    }
fail0:
    return 0;
}

void BListener_Free (BListener *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free default job
    BPending_Free(&o->default_job);
    
    // free BFileDescriptor
    BReactor_RemoveFileDescriptor(o->reactor, &o->bfd);
    
    // free fd
    if (close(o->fd) < 0) {
        BLog(BLOG_ERROR, "close failed");
    }
}

int BConnector_Init (BConnector *o, BAddr addr, BReactor *reactor, void *user,
                     BConnector_handler handler)
{
    ASSERT(BConnection_AddressSupported(addr))
    ASSERT(handler)
    BNetwork_Assert();
    
    // init arguments
    o->reactor = reactor;
    o->user = user;
    o->handler = handler;
    
    // convert address
    struct sys_addr sysaddr;
    addr_socket_to_sys(&sysaddr, addr);
    
    // init job
    BPending_Init(&o->job, BReactor_PendingGroup(o->reactor), (BPending_handler)connector_job_handler, o);
    
    // init fd
    if ((o->fd = socket(sysaddr.addr.generic.sa_family, SOCK_STREAM, 0)) < 0) {
        BLog(BLOG_ERROR, "socket failed");
        goto fail1;
    }
    
    // set fd non-blocking
    if (!badvpn_set_nonblocking(o->fd)) {
        BLog(BLOG_ERROR, "badvpn_set_nonblocking failed");
        goto fail2;
    }
    
    // connect fd
    int res = connect(o->fd, &sysaddr.addr.generic, sysaddr.len);
    if (res < 0 && errno != EINPROGRESS) {
        BLog(BLOG_ERROR, "connect failed");
        goto fail2;
    }
    
    // set not connected
    o->connected = 0;
    
    // set have no BFileDescriptor
    o->have_bfd = 0;
    
    if (res < 0) {
        // init BFileDescriptor
        BFileDescriptor_Init(&o->bfd, o->fd, (BFileDescriptor_handler)connector_fd_handler, o);
        if (!BReactor_AddFileDescriptor(o->reactor, &o->bfd)) {
            BLog(BLOG_ERROR, "BReactor_AddFileDescriptor failed");
            goto fail2;
        }
        BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, BREACTOR_WRITE);
        
        // set have BFileDescriptor
        o->have_bfd = 1;
    } else {
        // set connected
        o->connected = 1;
        
        // set job
        BPending_Set(&o->job);
    }
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail2:
    if (close(o->fd) < 0) {
        BLog(BLOG_ERROR, "close failed");
    }
fail1:
    BPending_Free(&o->job);
    return 0;
}

void BConnector_Free (BConnector *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free BFileDescriptor
    if (o->have_bfd) {
        BReactor_RemoveFileDescriptor(o->reactor, &o->bfd);
    }
    
    // close fd
    if (o->fd != -1) {
        if (close(o->fd) < 0) {
            BLog(BLOG_ERROR, "close failed");
        }
    }
    
    // free job
    BPending_Free(&o->job);
}

int BConnection_Init (BConnection *o, struct BConnection_source source, BReactor *reactor, void *user,
                      BConnection_handler handler)
{
    switch (source.type) {
        case BCONNECTION_SOURCE_TYPE_LISTENER: {
            BListener *listener = source.u.listener.listener;
            DebugObject_Access(&listener->d_obj);
            ASSERT(BPending_IsSet(&listener->default_job))
        } break;
        case BCONNECTION_SOURCE_TYPE_CONNECTOR: {
            BConnector *connector = source.u.connector.connector;
            DebugObject_Access(&connector->d_obj);
            ASSERT(connector->fd >= 0)
            ASSERT(connector->connected)
            ASSERT(!connector->have_bfd)
            ASSERT(!BPending_IsSet(&connector->job))
        } break;
        case BCONNECTION_SOURCE_TYPE_PIPE: {
            ASSERT(source.u.pipe.pipefd >= 0)
        } break;
        default: ASSERT(0);
    }
    ASSERT(handler)
    BNetwork_Assert();
    
    // init arguments
    o->reactor = reactor;
    o->user = user;
    o->handler = handler;
    
    switch (source.type) {
        case BCONNECTION_SOURCE_TYPE_LISTENER: {
            BListener *listener = source.u.listener.listener;
            
            // unset listener's default job
            BPending_Unset(&listener->default_job);
            
            // accept
            struct sys_addr sysaddr;
            sysaddr.len = sizeof(sysaddr.addr);
            if ((o->fd = accept(listener->fd, &sysaddr.addr.generic, &sysaddr.len)) < 0) {
                BLog(BLOG_ERROR, "accept failed");
                goto fail0;
            }
            o->close_fd = 1;
            
            // set non-blocking
            if (!badvpn_set_nonblocking(o->fd)) {
                BLog(BLOG_ERROR, "badvpn_set_nonblocking failed");
                goto fail1;
            }
            
            // return address
            if (source.u.listener.out_addr) {
                addr_sys_to_socket(source.u.listener.out_addr, sysaddr);
            }
        } break;
        
        case BCONNECTION_SOURCE_TYPE_CONNECTOR: {
            BConnector *connector = source.u.connector.connector;
            
            // grab fd from connector
            o->fd = connector->fd;
            connector->fd = -1;
            o->close_fd = 1;
        } break;
        
        case BCONNECTION_SOURCE_TYPE_PIPE: {
            // use user-provided fd
            o->fd = source.u.pipe.pipefd;
            o->close_fd = 0;
            
            // set non-blocking
            if (!badvpn_set_nonblocking(o->fd)) {
                BLog(BLOG_ERROR, "badvpn_set_nonblocking failed");
                goto fail1;
            }
        } break;
    }
    
    // init BFileDescriptor
    BFileDescriptor_Init(&o->bfd, o->fd, (BFileDescriptor_handler)connection_fd_handler, o);
    if (!BReactor_AddFileDescriptor(o->reactor, &o->bfd)) {
        BLog(BLOG_ERROR, "BReactor_AddFileDescriptor failed");
        goto fail1;
    }
    
    // set no wait events
    o->wait_events = 0;
    
    // init limits
    BReactorLimit_Init(&o->send.limit, o->reactor, BCONNECTION_SEND_LIMIT);
    BReactorLimit_Init(&o->recv.limit, o->reactor, BCONNECTION_RECV_LIMIT);
    
    // set send and recv not inited
    o->send.inited = 0;
    o->recv.inited = 0;
    
    // set recv not closed
    o->recv.closed = 0;
    
    DebugError_Init(&o->d_err, BReactor_PendingGroup(o->reactor));
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail1:
    if (o->close_fd) {
        if (close(o->fd) < 0) {
            BLog(BLOG_ERROR, "close failed");
        }
    }
fail0:
    return 0;
}

void BConnection_Free (BConnection *o)
{
    DebugObject_Free(&o->d_obj);
    DebugError_Free(&o->d_err);
    ASSERT(!o->recv.inited)
    ASSERT(!o->send.inited)
    
    // free limits
    BReactorLimit_Free(&o->recv.limit);
    BReactorLimit_Free(&o->send.limit);
    
    // free BFileDescriptor
    BReactor_RemoveFileDescriptor(o->reactor, &o->bfd);
    
    // close fd
    if (o->close_fd) {
        if (close(o->fd) < 0) {
            BLog(BLOG_ERROR, "close failed");
        }
    }
}

void BConnection_SetHandlers (BConnection *o, void *user, BConnection_handler handler)
{
    DebugObject_Access(&o->d_obj);
    
    // set handlers
    o->user = user;
    o->handler = handler;
}

int BConnection_SetSendBuffer (BConnection *o, int buf_size)
{
    DebugObject_Access(&o->d_obj);
    
    if (setsockopt(o->fd, SOL_SOCKET, SO_SNDBUF, (void *)&buf_size, sizeof(buf_size)) < 0) {
        BLog(BLOG_ERROR, "setsockopt failed");
        return 0;
    }
    
    return 1;
}

void BConnection_SendAsync_Init (BConnection *o)
{
    DebugObject_Access(&o->d_obj);
    DebugError_AssertNoError(&o->d_err);
    ASSERT(!o->send.inited)
    
    // init interface
    StreamPassInterface_Init(&o->send.iface, (StreamPassInterface_handler_send)connection_send_if_handler_send, o, BReactor_PendingGroup(o->reactor));
    
    // init job
    BPending_Init(&o->send.job, BReactor_PendingGroup(o->reactor), (BPending_handler)connection_send_job_handler, o);
    
    // set not busy
    o->send.busy = 0;
    
    // set inited
    o->send.inited = 1;
}

void BConnection_SendAsync_Free (BConnection *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->send.inited)
    
    // update events
    o->wait_events &= ~BREACTOR_WRITE;
    BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->wait_events);
    
    // free job
    BPending_Free(&o->send.job);
    
    // free interface
    StreamPassInterface_Free(&o->send.iface);
    
    // set not inited
    o->send.inited = 0;
}

StreamPassInterface * BConnection_SendAsync_GetIf (BConnection *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->send.inited)
    
    return &o->send.iface;
}

void BConnection_RecvAsync_Init (BConnection *o)
{
    DebugObject_Access(&o->d_obj);
    DebugError_AssertNoError(&o->d_err);
    ASSERT(!o->recv.inited)
    ASSERT(!o->recv.closed)
    
    // init interface
    StreamRecvInterface_Init(&o->recv.iface, (StreamRecvInterface_handler_recv)connection_recv_if_handler_recv, o, BReactor_PendingGroup(o->reactor));
    
    // init job
    BPending_Init(&o->recv.job, BReactor_PendingGroup(o->reactor), (BPending_handler)connection_recv_job_handler, o);
    
    // set not busy
    o->recv.busy = 0;
    
    // set inited
    o->recv.inited = 1;
}

void BConnection_RecvAsync_Free (BConnection *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->recv.inited)
    
    // update events
    o->wait_events &= ~BREACTOR_READ;
    BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->wait_events);
    
    // free job
    BPending_Free(&o->recv.job);
    
    // free interface
    StreamRecvInterface_Free(&o->recv.iface);
    
    // set not inited
    o->recv.inited = 0;
}

StreamRecvInterface * BConnection_RecvAsync_GetIf (BConnection *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->recv.inited)
    
    return &o->recv.iface;
}
