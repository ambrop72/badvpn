/**
 * @file BDatagram_unix.h
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

#define BDATAGRAM_SEND_LIMIT 2
#define BDATAGRAM_RECV_LIMIT 2

struct BDatagram_s {
    BReactor *reactor;
    void *user;
    BDatagram_handler handler;
    int fd;
    BFileDescriptor bfd;
    int wait_events;
    struct {
        BReactorLimit limit;
        int have_addrs;
        BAddr remote_addr;
        BIPAddr local_addr;
        int inited;
        int mtu;
        PacketPassInterface iface;
        BPending job;
        int busy;
        const uint8_t *busy_data;
        int busy_data_len;
    } send;
    struct {
        BReactorLimit limit;
        int started;
        int have_addrs;
        BAddr remote_addr;
        BIPAddr local_addr;
        int inited;
        int mtu;
        PacketRecvInterface iface;
        BPending job;
        int busy;
        uint8_t *busy_data;
    } recv;
    DebugError d_err;
    DebugObject d_obj;
};
