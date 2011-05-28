/**
 * @file BConnection_unix.h
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

#include <misc/debugerror.h>
#include <base/DebugObject.h>

#define BCONNECTION_SEND_LIMIT 2
#define BCONNECTION_RECV_LIMIT 2
#define BCONNECTION_LISTEN_BACKLOG 128

struct BListener_s {
    BReactor *reactor;
    void *user;
    BListener_handler handler;
    int fd;
    BFileDescriptor bfd;
    BPending default_job;
    DebugObject d_obj;
};

struct BConnector_s {
    BReactor *reactor;
    void *user;
    BConnector_handler handler;
    BPending job;
    int fd;
    int connected;
    int have_bfd;
    BFileDescriptor bfd;
    DebugObject d_obj;
};

struct BConnection_s {
    BReactor *reactor;
    void *user;
    BConnection_handler handler;
    int fd;
    int close_fd;
    BFileDescriptor bfd;
    int wait_events;
    struct {
        BReactorLimit limit;
        int inited;
        StreamPassInterface iface;
        BPending job;
        int busy;
        const uint8_t *busy_data;
        int busy_data_len;
    } send;
    struct {
        BReactorLimit limit;
        int inited;
        int closed;
        StreamRecvInterface iface;
        BPending job;
        int busy;
        uint8_t *busy_data;
        int busy_data_avail;
    } recv;
    DebugError d_err;
    DebugObject d_obj;
};
