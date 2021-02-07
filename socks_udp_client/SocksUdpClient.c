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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <misc/balloc.h>
#include <misc/offset.h>
#include <misc/byteorder.h>
#include <misc/compare.h>
#include <misc/socks_proto.h>
#include <misc/debug.h>
#include <misc/bsize.h>
#include <base/BLog.h>
#include <system/BAddr.h>

#include <socks_udp_client/SocksUdpClient.h>

#include <generated/blog_channel_SocksUdpClient.h>

static const int DnsPort = 53;

static int addr_comparator (void *unused, BAddr *v1, BAddr *v2);
static struct SocksUdpClient_connection * find_connection (SocksUdpClient *o, BAddr addr);
static void socks_state_handler (struct SocksUdpClient_connection *con, int event);
static void datagram_state_handler (struct SocksUdpClient_connection *con, int event);
static void send_monitor_handler (struct SocksUdpClient_connection *con);
static void recv_if_handler_send (
    struct SocksUdpClient_connection *con, uint8_t *data, int data_len);
static struct SocksUdpClient_connection * connection_init (
    SocksUdpClient *o, BAddr local_addr, BAddr first_remote_addr,
    const uint8_t *first_data, int first_data_len);
static void connection_free (struct SocksUdpClient_connection *con);
static void connection_send (struct SocksUdpClient_connection *con,
    BAddr remote_addr, const uint8_t *data, int data_len);
static void first_job_handler (struct SocksUdpClient_connection *con);
static int compute_socks_mtu (int udp_mtu);
static int get_dns_id (BAddr *remote_addr, const uint8_t *data, int data_len);

int addr_comparator (void *unused, BAddr *v1, BAddr *v2)
{
    return BAddr_CompareOrder(v1, v2);
}

struct SocksUdpClient_connection * find_connection (SocksUdpClient *o, BAddr addr)
{
    BAVLNode *tree_node = BAVL_LookupExact(&o->connections_tree, &addr);
    if (!tree_node) {
        return NULL;
    }
    
    return UPPER_OBJECT(tree_node, struct SocksUdpClient_connection, connections_tree_node);
}

void socks_state_handler (struct SocksUdpClient_connection *con, int event)
{
    DebugObject_Access(&con->client->d_obj);

    switch (event) {
        case BSOCKSCLIENT_EVENT_CONNECTED: {
            // Get the local address of the SOCKS TCP connection.
            BAddr tcp_local_addr;
            if (!BSocksClient_GetLocalAddr(&con->socks, &tcp_local_addr)) {
                BLog(BLOG_ERROR, "Failed to get TCP local address.");
                return connection_free(con);
            }

            // Sanity check the address type (required by SetPort below).
            if (tcp_local_addr.type != BADDR_TYPE_IPV4 &&
                tcp_local_addr.type != BADDR_TYPE_IPV6)
            {
                BLog(BLOG_ERROR, "Bad address type in TCP local address.");
                return connection_free(con);
            }

            // Bind the UDP socket to the same IP address and let the kernel pick the port.
            BAddr udp_bound_addr = tcp_local_addr;
            BAddr_SetPort(&udp_bound_addr, 0);
            if (!BDatagram_Bind(&con->socket, udp_bound_addr)) {
                BLog(BLOG_ERROR, "Failed to bind the UDP socket.");
                return connection_free(con);
            }
            
            // Update udp_bound_addr to the actual address that was bound.
            if (!BDatagram_GetLocalAddr(&con->socket, &udp_bound_addr)) {
                BLog(BLOG_ERROR, "Failed to get UDP bound address.");
                return connection_free(con);
            }

            // Set the DST.ADDR for SOCKS.
            BSocksClient_SetDestAddr(&con->socks, udp_bound_addr);
        } break;

        case BSOCKSCLIENT_EVENT_UP: {
            // The remote address to send datagrams to is the BND.ADDR provided by the
            // SOCKS server.
            BAddr remote_addr = BSocksClient_GetBindAddr(&con->socks);

            // Don't bother setting a source address for datagrams since we are bound.
            BIPAddr local_addr;
            BIPAddr_InitInvalid(&local_addr);

            // Set the addresses for BDatagram.
            // This will unblock the queue of outgoing packets.
            BDatagram_SetSendAddrs(&con->socket, remote_addr, local_addr);
        } break;

        case BSOCKSCLIENT_EVENT_ERROR: {
            char local_buffer[BADDR_MAX_PRINT_LEN];
            BAddr_Print(&con->local_addr, local_buffer);
            BLog(BLOG_ERROR,
                "SOCKS error event for %s, removing connection.", local_buffer);

            connection_free(con);
        } break;

        case BSOCKSCLIENT_EVENT_ERROR_CLOSED: {
            char local_buffer[BADDR_MAX_PRINT_LEN];
            BAddr_Print(&con->local_addr, local_buffer);
            BLog(BLOG_WARNING,
                "SOCKS closed event for %s, removing connection.", local_buffer);

            connection_free(con);
        } break;
    }
}

void datagram_state_handler (struct SocksUdpClient_connection *con, int event)
{
    DebugObject_Access(&con->client->d_obj);

    if (event == BDATAGRAM_EVENT_ERROR) {
        char local_buffer[BADDR_MAX_PRINT_LEN];
        BAddr_Print(&con->local_addr, local_buffer);
        BLog(BLOG_ERROR,
            "Low-level datagram error %s, removing connection.", local_buffer);

        // Remove the connection. Note that BDatagram requires that we free
        // the BDatagram after an error is reported.
        connection_free(con);
    }
}

void send_monitor_handler (struct SocksUdpClient_connection *con)
{
    DebugObject_Access(&con->client->d_obj);
    
    char local_buffer[BADDR_MAX_PRINT_LEN];
    BAddr_Print(&con->local_addr, local_buffer);
    BLog(BLOG_INFO,
        "Removing connection for %s due to inactivity.", local_buffer);

    // The connection has passed its idle timeout. Remove it.
    connection_free(con);
}

void recv_if_handler_send (
    struct SocksUdpClient_connection *con, uint8_t *data, int data_len)
{
    DebugObject_Access(&con->client->d_obj);
    SocksUdpClient *o = con->client;
    ASSERT(data_len >= 0)
    ASSERT(data_len <= o->socks_mtu)
    
    // accept packet
    PacketPassInterface_Done(&con->recv_if);
    
    // check header
    struct socks_udp_header header;
    if (data_len < sizeof(header)) {
        BLog(BLOG_ERROR, "Missing SOCKS-UDP header.");
        return;
    }
    memcpy(&header, data, sizeof(header));
    data += sizeof(header);
    data_len -= sizeof(header);
    
    // parse address
    BAddr remote_addr;
    switch (header.atyp) {
        case SOCKS_ATYP_IPV4: {
            struct socks_addr_ipv4 addr_ipv4;
            if (data_len < sizeof(addr_ipv4)) {
                BLog(BLOG_ERROR, "Missing IPv4 address.");
                return;
            }
            memcpy(&addr_ipv4, data, sizeof(addr_ipv4));
            data += sizeof(addr_ipv4);
            data_len -= sizeof(addr_ipv4);
            remote_addr.type = BADDR_TYPE_IPV4;
            remote_addr.ipv4.ip = addr_ipv4.addr;
            remote_addr.ipv4.port = addr_ipv4.port;
        } break;
        case SOCKS_ATYP_IPV6: {
            struct socks_addr_ipv6 addr_ipv6;
            if (data_len < sizeof(addr_ipv6)) {
                BLog(BLOG_ERROR, "Missing IPv6 address.");
                return;
            }
            memcpy(&addr_ipv6, data, sizeof(addr_ipv6));
            data += sizeof(addr_ipv6);
            data_len -= sizeof(addr_ipv6);
            remote_addr.type = BADDR_TYPE_IPV6;
            memcpy(remote_addr.ipv6.ip, addr_ipv6.addr, sizeof(remote_addr.ipv6.ip));
            remote_addr.ipv6.port = addr_ipv6.port;
        } break;
        default: {
            BLog(BLOG_ERROR, "Bad address type");
            return;
        } break;
    }
    
    // check remaining data
    if (data_len > o->udp_mtu) {
        BLog(BLOG_ERROR, "too much data");
        return;
    }
    
    // pass packet to user
    SocksUdpClient *client = con->client;
    client->handler_received(client->user, con->local_addr, remote_addr, data, data_len);

    // Was this connection used for a DNS query?
    if (con->dns_id >= 0) {
        // Get the DNS transaction ID of the response.
        int recv_dns_id = get_dns_id(&remote_addr, data, data_len);

        // Does the transaction ID matche that of the request?
        if (recv_dns_id == con->dns_id) {
            // We have now forwarded the response, so this connection is no longer needed.
            char local_buffer[BADDR_MAX_PRINT_LEN];
            BAddr_Print(&con->local_addr, local_buffer);
            BLog(BLOG_DEBUG,
                "Removing connection for %s after the DNS response.", local_buffer);

            connection_free(con);
        } else {
            BLog(BLOG_INFO, "DNS client port received an unexpected non-DNS packet, "
                "disabling DNS optimization.");
            
            con->dns_id = -1;
        }
    }
}

struct SocksUdpClient_connection * connection_init (
    SocksUdpClient *o, BAddr local_addr, BAddr first_remote_addr,
    const uint8_t *first_data, int first_data_len)
{
    ASSERT(o->num_connections <= o->max_connections)
    ASSERT(!find_connection(o, local_addr))
    
    char local_buffer[BADDR_MAX_PRINT_LEN];
    BAddr_Print(&local_addr, local_buffer);
    BLog(BLOG_DEBUG, "Creating connection for %s.", local_buffer);
    
    // allocate structure
    struct SocksUdpClient_connection *con =
        (struct SocksUdpClient_connection *)BAlloc(sizeof(*con));
    if (!con) {
        BLog(BLOG_ERROR, "BAlloc connection failed");
        goto fail0;
    }
    
    // set basic things
    con->client = o;
    con->local_addr = local_addr;

    // store first outgoing packet
    con->first_data = BAlloc(first_data_len);
    if (!con->first_data) {
        BLog(BLOG_ERROR, "BAlloc first data failed");
        goto fail1;
    }
    memcpy(con->first_data, first_data, first_data_len);
    con->first_data_len = first_data_len;
    con->first_remote_addr = first_remote_addr;
    
    // Get the DNS transaction ID from the packet, if any.
    con->dns_id = get_dns_id(&first_remote_addr, first_data, first_data_len);
    
    BPendingGroup *pg = BReactor_PendingGroup(o->reactor);
    
    // Init first job, to send the first packet asynchronously. This has to happen
    // asynchronously because con->send_writer (a BufferWriter) cannot accept writes until
    // after it is linked with its PacketBuffer (con->send_buffer), which happens
    // asynchronously.
    BPending_Init(&con->first_job, pg, (BPending_handler)first_job_handler, con);
    // Add the first job to the pending set. BPending acts as a LIFO stack, and
    // first_job_handler needs to run after async actions that occur in PacketBuffer_Init,
    // so we need to put first_job on the stack first.
    BPending_Set(&con->first_job);
    
    // Create a datagram socket
    if (!BDatagram_Init(&con->socket, con->local_addr.type, o->reactor, con,
                        (BDatagram_handler)datagram_state_handler))
    {
        BLog(BLOG_ERROR, "Failed to create a UDP socket");
        goto fail2;
    }
    
    // We will set the DST.ADDR for SOCKS later (BSOCKSCLIENT_EVENT_CONNECTED).
    BAddr dummy_dst_addr;
    BAddr_InitNone(&dummy_dst_addr);

    // Initiate connection to socks server
    if (!BSocksClient_Init(&con->socks, o->server_addr, o->auth_info, o->num_auth_info,
        dummy_dst_addr, true, (BSocksClient_handler)socks_state_handler, con, o->reactor))
    {
        BLog(BLOG_ERROR, "Failed to initialize SOCKS client");
        goto fail3;
    }
    
    // Since we use o->socks_mtu for send and receive pipelines, we can handle maximally
    // sized packets (o->udp_mtu) including the SOCKS-UDP header.

    // Send pipeline: send_writer -> send_buffer -> send_monitor -> send_if -> socket.
    BDatagram_SendAsync_Init(&con->socket, o->socks_mtu);
    PacketPassInactivityMonitor_Init(&con->send_monitor,
        BDatagram_SendAsync_GetIf(&con->socket), o->reactor, o->keepalive_time,
        (PacketPassInactivityMonitor_handler)send_monitor_handler, con);
    BufferWriter_Init(&con->send_writer, o->socks_mtu, pg);
    if (!PacketBuffer_Init(&con->send_buffer, BufferWriter_GetOutput(&con->send_writer),
        PacketPassInactivityMonitor_GetInput(&con->send_monitor), o->send_buf_size, pg))
    {
        BLog(BLOG_ERROR, "Send buffer init failed");
        goto fail4;
    }
    
    // Receive pipeline: socket -> recv_buffer -> recv_if
    BDatagram_RecvAsync_Init(&con->socket, o->socks_mtu);
    PacketPassInterface_Init(&con->recv_if, o->socks_mtu,
        (PacketPassInterface_handler_send)recv_if_handler_send, con, pg);
    if (!SinglePacketBuffer_Init(&con->recv_buffer,
        BDatagram_RecvAsync_GetIf(&con->socket), &con->recv_if, pg))
    {
        BLog(BLOG_ERROR, "Receive buffer init failed");
        goto fail5;
    }
    
    // Insert to connections tree, it must succeed because of the assert.
    int inserted = BAVL_Insert(&o->connections_tree, &con->connections_tree_node, NULL);
    ASSERT(inserted)
    B_USE(inserted)
    
    // increment number of connections
    o->num_connections++;
    
    return con;
    
fail5:
    PacketPassInterface_Free(&con->recv_if);
    BDatagram_RecvAsync_Free(&con->socket);
    PacketBuffer_Free(&con->send_buffer);
fail4:
    BufferWriter_Free(&con->send_writer);
    PacketPassInactivityMonitor_Free(&con->send_monitor);
    BDatagram_SendAsync_Free(&con->socket);
    BSocksClient_Free(&con->socks);
fail3:
    BDatagram_Free(&con->socket);
fail2:
    BPending_Free(&con->first_job);
    BFree(con->first_data);
fail1:
    BFree(con);
fail0:
    return NULL;
}

void connection_free (struct SocksUdpClient_connection *con)
{
    SocksUdpClient *o = con->client;
    
    // decrement number of connections
    ASSERT(o->num_connections > 0)
    o->num_connections--;
    
    // remove from connections tree
    BAVL_Remove(&o->connections_tree, &con->connections_tree_node);
    
    // Free UDP receive pipeline components
    SinglePacketBuffer_Free(&con->recv_buffer);
    PacketPassInterface_Free(&con->recv_if);
    BDatagram_RecvAsync_Free(&con->socket);
    
    // Free UDP send pipeline components
    PacketBuffer_Free(&con->send_buffer);
    BufferWriter_Free(&con->send_writer);
    PacketPassInactivityMonitor_Free(&con->send_monitor);
    BDatagram_SendAsync_Free(&con->socket);
    
    // Free SOCKS client
    BSocksClient_Free(&con->socks);
    
    // Free UDP socket
    BDatagram_Free(&con->socket);
    
    // Free first job
    BPending_Free(&con->first_job);

    // Free first outgoing packet
    BFree(con->first_data);

    // Free structure
    BFree(con);
}

void connection_send (struct SocksUdpClient_connection *con,
    BAddr remote_addr, const uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= con->client->udp_mtu)
    
    if (con->dns_id >= 0) {
        // So far, this connection has only sent a single DNS query.
        int new_dns_id = get_dns_id(&remote_addr, data, data_len);
        if (new_dns_id != con->dns_id) {
            BLog(BLOG_DEBUG, "Client reused DNS query port. Disabling DNS optimization.");
            con->dns_id = -1;
        }
    }
    
    // Check if we're sending to an IPv4 or IPv6 destination.
    int atyp;
    size_t address_size;
    // write address
    switch (remote_addr.type) {
        case BADDR_TYPE_IPV4: {
            atyp = SOCKS_ATYP_IPV4;
            address_size = sizeof(struct socks_addr_ipv4);
        } break;
        case BADDR_TYPE_IPV6: {
            atyp = SOCKS_ATYP_IPV6;
            address_size = sizeof(struct socks_addr_ipv6);
        } break;
        default: {
            BLog(BLOG_ERROR, "Bad address type in outgoing packet.");
            return;
        } break;
    }
    
    // Determine total packet size in the buffer.
    // This cannot exceed o->socks_mtu because data_len is required to not exceed
    // o->udp_mtu and o->socks_mtu is calculated to accomodate any UDP packet not
    // not exceeding o->udp_mtu.
    size_t total_len = sizeof(struct socks_udp_header) + address_size + data_len;
    ASSERT(total_len <= con->client->socks_mtu)

    // Get a pointer to write the packet to.
    uint8_t *out_data_begin;
    if (!BufferWriter_StartPacket(&con->send_writer, &out_data_begin)) {
        BLog(BLOG_ERROR, "Send buffer is full.");
        return;
    }
    uint8_t *out_data = out_data_begin;

    // Write header
    struct socks_udp_header header;
    header.rsv = 0;
    header.frag = 0;
    header.atyp = atyp;
    memcpy(out_data, &header, sizeof(header));
    out_data += sizeof(header);

    // Write address
    switch (atyp) {
        case SOCKS_ATYP_IPV4: {
            struct socks_addr_ipv4 addr_ipv4;
            addr_ipv4.addr = remote_addr.ipv4.ip;
            addr_ipv4.port = remote_addr.ipv4.port;
            memcpy(out_data, &addr_ipv4, sizeof(addr_ipv4));
            out_data += sizeof(addr_ipv4);
        } break;
        case SOCKS_ATYP_IPV6: {
            struct socks_addr_ipv6 addr_ipv6;
            memcpy(addr_ipv6.addr, remote_addr.ipv6.ip, sizeof(addr_ipv6.addr));
            addr_ipv6.port = remote_addr.ipv6.port;
            memcpy(out_data, &addr_ipv6, sizeof(addr_ipv6));
            out_data += sizeof(addr_ipv6);
        } break;
    }

    // Write payload
    memcpy(out_data, data, data_len);
    out_data += data_len;

    ASSERT(out_data - out_data_begin == total_len)

    // Submit packet to buffer
    BufferWriter_EndPacket(&con->send_writer, total_len);
}

void first_job_handler (struct SocksUdpClient_connection *con)
{
    DebugObject_Access(&con->client->d_obj);
    ASSERT(con->first_data)
    
    // Send the first packet.
    connection_send(con, con->first_remote_addr, con->first_data, con->first_data_len);

    // Release the first packet buffer.
    BFree(con->first_data);
    con->first_data = NULL;
    con->first_data_len = 0;
}

int compute_socks_mtu (int udp_mtu)
{
    bsize_t bs = bsize_add(
        bsize_fromint(udp_mtu),
        bsize_add(
            bsize_fromsize(sizeof(struct socks_udp_header)),
            bsize_fromsize(sizeof(struct socks_addr_ipv6))
        )
    );
    int s;
    return bsize_toint(bs, &s) ? s : -1;
}

// Get the DNS transaction ID, or -1 if this does not look like a DNS packet.
int get_dns_id (BAddr *remote_addr, const uint8_t *data, int data_len)
{
    if (ntoh16(BAddr_GetPort(remote_addr)) == DnsPort && data_len >= 2) {
        return (data[0] << 8) | data[1];
    } else {
        return -1;
    }
}

int SocksUdpClient_Init (SocksUdpClient *o, int udp_mtu, int max_connections,
    int send_buf_size, btime_t keepalive_time, BAddr server_addr,
    const struct BSocksClient_auth_info *auth_info, size_t num_auth_info,
    BReactor *reactor, void *user, SocksUdpClient_handler_received handler_received)
{
    ASSERT(udp_mtu >= 0)
    ASSERT(max_connections > 0)
    ASSERT(send_buf_size > 0)
    
    // init simple things
    o->server_addr = server_addr;
    o->auth_info = auth_info;
    o->num_auth_info = num_auth_info;
    o->num_connections = 0;
    o->max_connections = max_connections;
    o->send_buf_size = send_buf_size;
    o->udp_mtu = udp_mtu;
    o->keepalive_time = keepalive_time;
    o->reactor = reactor;
    o->user = user;
    o->handler_received = handler_received;

    // calculate full MTU with SOCKS header
    o->socks_mtu = compute_socks_mtu(udp_mtu);
    if (o->socks_mtu < 0) {
        BLog(BLOG_ERROR, "SocksUdpClient_Init: MTU too large.");
        goto fail0;
    }
    
    // init connections tree
    BAVL_Init(&o->connections_tree,
        OFFSET_DIFF(struct SocksUdpClient_connection, local_addr, connections_tree_node),
        (BAVL_comparator)addr_comparator, NULL);
    
    DebugObject_Init(&o->d_obj);
    return 1;

fail0:
    return 0;
}

void SocksUdpClient_Free (SocksUdpClient *o)
{
    DebugObject_Free(&o->d_obj);

    // free connections
    while (!BAVL_IsEmpty(&o->connections_tree)) {
        BAVLNode *node = BAVL_GetFirst(&o->connections_tree);
        struct SocksUdpClient_connection *con =
            UPPER_OBJECT(node, struct SocksUdpClient_connection, connections_tree_node);
        connection_free(con);
    }
}

void SocksUdpClient_SubmitPacket (SocksUdpClient *o,
    BAddr local_addr, BAddr remote_addr, const uint8_t *data, int data_len)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(local_addr.type == BADDR_TYPE_IPV4 || local_addr.type == BADDR_TYPE_IPV6)
    ASSERT(remote_addr.type == BADDR_TYPE_IPV4 || remote_addr.type == BADDR_TYPE_IPV6)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= o->udp_mtu)
    
    // lookup connection
    struct SocksUdpClient_connection *con = find_connection(o, local_addr);
    if (!con) {
        if (o->num_connections >= o->max_connections) {
            // Drop the packet.
            BLog(BLOG_WARNING, "Dropping UDP packet, reached max number of connections.");
            return;
        }
        // create new connection and enqueue the packet
        connection_init(o, local_addr, remote_addr, data, data_len);
    } else {
        // send packet
        connection_send(con, remote_addr, data, data_len);
    }
}
