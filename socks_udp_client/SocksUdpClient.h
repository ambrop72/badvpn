/*
 * Copyright (C) 2018 Jigsaw Operations LLC
 * Copyright (C) 2019 Ambroz Bizjak (modifications)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BADVPN_SOCKS_UDP_CLIENT_SOCKSUDPCLIENT_H
#define BADVPN_SOCKS_UDP_CLIENT_SOCKSUDPCLIENT_H

#include <stddef.h>
#include <stdint.h>

#include <base/BPending.h>
#include <base/DebugObject.h>
#include <flow/BufferWriter.h>
#include <flow/PacketBuffer.h>
#include <flow/SinglePacketBuffer.h>
#include <flow/PacketPassInterface.h>
#include <flowextra/PacketPassInactivityMonitor.h>
#include <socksclient/BSocksClient.h>
#include <structure/BAVL.h>
#include <system/BAddr.h>
#include <system/BDatagram.h>
#include <system/BReactor.h>
#include <system/BTime.h>

typedef void (*SocksUdpClient_handler_received) (
    void *user, BAddr local_addr, BAddr remote_addr, const uint8_t *data, int data_len);

typedef struct {
    BAddr server_addr;
    const struct BSocksClient_auth_info *auth_info;
    size_t num_auth_info;
    int num_connections;
    int max_connections;
    int send_buf_size;
    int udp_mtu;
    int socks_mtu;
    btime_t keepalive_time;
    BReactor *reactor;
    void *user;
    SocksUdpClient_handler_received handler_received;
    BAVL connections_tree;  // By local_addr
    DebugObject d_obj;
} SocksUdpClient;

struct SocksUdpClient_connection {
    SocksUdpClient *client;
    BAddr local_addr;
    BSocksClient socks;
    BufferWriter send_writer;
    PacketBuffer send_buffer;
    PacketPassInactivityMonitor send_monitor;
    PacketPassInterface send_if;
    BDatagram socket;
    PacketPassInterface recv_if;
    SinglePacketBuffer recv_buffer;
    // The first_* members represent the initial packet, which has to be stored so it can
    // wait for send_writer to become ready.
    uint8_t *first_data;
    int first_data_len;
    BAddr first_remote_addr;
    // If all packets sent so far have been sent to the same IP, port 53, with the
    // same DNS ID, then this is that ID.  Otherwise, it is -1.  This is used to
    // close ephemeral DNS query connections once a response is received.
    int dns_id;
    BPending first_job;
    BAVLNode connections_tree_node;
};

/**
 * Initializes the SOCKS5-UDP client object.
 * 
 * This function only initialzies the object and does not perform network access.
 * 
 * @param o the object
 * @param udp_mtu the maximum size of packets that will be sent through the tunnel
 * @param max_connections how many local ports to track before dropping packets
 * @param send_buf_size maximum number of buffered outgoing packets per connection
 * @param keepalive_time how long to track an idle local port before forgetting it
 * @param server_addr SOCKS5 server address
 * @param auth_info List of authentication info for BSocksClient. The pointer must remain
 *        valid while this object exists, the data is not copied.
 * @param num_auth_info Number of the above.
 * @param reactor reactor we live in
 * @param user value passed to handler
 * @param handler_received handler for incoming UDP packets
 * @return 1 on success, 0 on failure
 */
int SocksUdpClient_Init (SocksUdpClient *o, int udp_mtu, int max_connections,
    int send_buf_size, btime_t keepalive_time, BAddr server_addr,
    const struct BSocksClient_auth_info *auth_info, size_t num_auth_info,
    BReactor *reactor, void *user, SocksUdpClient_handler_received handler_received);

/**
 * Frees the SOCKS5-UDP client object.
 *
 * @param o the object
 */
void SocksUdpClient_Free (SocksUdpClient *o);

/**
 * Submit a packet to be sent through the proxy.
 *
 * This will reuse an existing connection for packets from local_addr, or create one if
 * there is none. If the number of live connections exceeds max_connections, or if the
 * number of buffered packets from this port exceeds a limit, packets will be dropped
 * silently.
 * 
 * As a resource optimization, if a connection has only been used to send one DNS query,
 * then the connection will be closed and freed once the reply is received.
 * 
 * @param o the object
 * @param local_addr the UDP packet's source address, and the expected destination for
 *        replies
 * @param remote_addr the destination of the packet after it exits the proxy
 * @param data the packet contents. Caller retains ownership.
 * @param data_len number of bytes in the data
 */
void SocksUdpClient_SubmitPacket (SocksUdpClient *o,
    BAddr local_addr, BAddr remote_addr, const uint8_t *data, int data_len);

#endif
