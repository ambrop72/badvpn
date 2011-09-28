/**
 * @file BArpProbe.h
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

#ifndef BADVPN_BARPPROBE_H
#define BADVPN_BARPPROBE_H

#include <stdint.h>

#include <misc/debug.h>
#include <misc/debugerror.h>
#include <misc/arp_proto.h>
#include <misc/ethernet_proto.h>
#include <base/DebugObject.h>
#include <system/BDatagram.h>
#include <system/BReactor.h>

#define BARPPROBE_INITIAL_WAITRECV 1000
#define BARPPROBE_INITIAL_NUM_ATTEMPTS 6
#define BARPPROBE_NOEXIST_WAITRECV 15000
#define BARPPROBE_EXIST_WAITSEND 15000
#define BARPPROBE_EXIST_WAITRECV 10000
#define BARPPROBE_EXIST_NUM_NOREPLY 2
#define BARPPROBE_EXIST_PANIC_WAITRECV 1000
#define BARPPROBE_EXIST_PANIC_NUM_NOREPLY 6

#define BARPPROBE_EVENT_EXIST 1
#define BARPPROBE_EVENT_NOEXIST 2
#define BARPPROBE_EVENT_ERROR 3

typedef void (*BArpProbe_handler) (void *user, int event);

typedef struct {
    uint32_t addr;
    BReactor *reactor;
    void *user;
    BArpProbe_handler handler;
    BDatagram dgram;
    uint8_t if_mac[6];
    PacketPassInterface *send_if;
    int send_sending;
    struct arp_packet send_packet;
    PacketRecvInterface *recv_if;
    struct arp_packet recv_packet;
    BTimer timer;
    int state;
    int num_missed;
    DebugError d_err;
    DebugObject d_obj;
} BArpProbe;

int BArpProbe_Init (BArpProbe *o, const char *ifname, uint32_t addr, BReactor *reactor, void *user, BArpProbe_handler handler) WARN_UNUSED;
void BArpProbe_Free (BArpProbe *o);

#endif
