/**
 * @file server.h
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

#include <stdint.h>

#include <protocol/scproto.h>
#include <structure/LinkedList2.h>
#include <structure/BAVL.h>
#include <system/BSocket.h>
#include <flow/StreamSocketSource.h>
#include <flow/PacketProtoDecoder.h>
#include <flow/PacketStreamSender.h>
#include <flow/StreamSocketSink.h>
#include <flow/PacketPassPriorityQueue.h>
#include <flow/PacketPassFairQueue.h>
#include <flow/PacketProtoFlow.h>
#include <nspr_support/BPRFileDesc.h>
#include <nspr_support/PRStreamSource.h>
#include <nspr_support/PRStreamSink.h>

// name of the program
#define PROGRAM_NAME "server"

// maxiumum number of connected clients. Must be <=2^16.
#define MAX_CLIENTS 30
// client output control flow buffer size in packets
// it must hold: initdata, newclient's, endclient's (if other peers die when informing them)
// make it big enough to hold the initial packet burst (initdata, newclient's),
#define CLIENT_CONTROL_BUFFER_MIN_PACKETS (1 + 2*(MAX_CLIENTS - 1))
// size of client-to-client buffers in packets
#define CLIENT_PEER_FLOW_BUFFER_MIN_PACKETS 10
// after how long of not hearing anything from the client we disconnect it
#define CLIENT_NO_DATA_TIME_LIMIT 30000

// maxiumum listen addresses
#define MAX_LISTEN_ADDRS 16


// initializing
#define INITSTATUS_INIT 0
// performing SSL handshake
#define INITSTATUS_HANDSHAKE 1
// waiting for clienthello
#define INITSTATUS_WAITHELLO 2
// initialisation was complete
#define INITSTATUS_COMPLETE 3

#define INITSTATUS_HASLINK(status) ((status) == INITSTATUS_WAITHELLO || (status) == INITSTATUS_COMPLETE)

struct client_data;

struct peer_flow {
    // source client
    struct client_data *src_client;
    // destination client
    struct client_data *dest_client;
    peerid_t dest_client_id;
    // node in source client hash table (by destination), only when src_client != NULL
    BAVLNode src_tree_node;
    // node in source client list, only when src_client != NULL
    LinkedList2Node src_list_node;
    // node in destination client list
    LinkedList2Node dest_list_node;
    // output chain
    PacketPassFairQueueFlow qflow;
    PacketProtoFlow oflow;
    BufferWriter *input;
    int packet_len;
    uint8_t *packet;
};

struct peer_know {
    struct client_data *from;
    struct client_data *to;
    LinkedList2Node from_node;
    LinkedList2Node to_node;
};

struct client_data {
    // socket
    BSocket sock;
    BAddr addr;
    
    // SSL file descriptor
    PRFileDesc bottom_prfd;
    PRFileDesc *ssl_prfd;
    BPRFileDesc ssl_bprfd;
    
    // initialization state
    int initstatus;
    
    // client data if using SSL
    uint8_t cert[SCID_NEWCLIENT_MAX_CERT_LEN];
    int cert_len;
    uint8_t cert_old[SCID_NEWCLIENT_MAX_CERT_LEN];
    int cert_old_len;
    char *common_name;
    
    // client version
    int version;
    
    // no data timer
    BTimer disconnect_timer;
    
    // client ID
    peerid_t id;
    
    // node in clients linked list
    LinkedList2Node list_node;
    // node in clients tree (by ID)
    BAVLNode tree_node;
    
    // knowledge lists
    LinkedList2 know_out_list;
    LinkedList2 know_in_list;
    
    // flows from us
    LinkedList2 peer_out_flows_list;
    BAVL peer_out_flows_tree;
    
    // whether it's being removed
    int dying;
    BPending dying_job;
    
    // publish job
    BPending publish_job;
    LinkedList2Iterator publish_it;
    
    // error domain
    FlowErrorDomain domain;
    
    // input
    union {
        StreamSocketSource plain;
        PRStreamSource ssl;
    } input_source;
    PacketProtoDecoder input_decoder;
    PacketPassInterface input_interface;
    
    // output common
    union {
        StreamSocketSink plain;
        PRStreamSink ssl;
    } output_sink;
    PacketStreamSender output_sender;
    PacketPassPriorityQueue output_priorityqueue;
    
    // output control flow
    PacketPassPriorityQueueFlow output_control_qflow;
    PacketProtoFlow output_control_oflow;
    BufferWriter *output_control_input;
    int output_control_packet_len;
    uint8_t *output_control_packet;
    
    // output peers flow
    PacketPassPriorityQueueFlow output_peers_qflow;
    PacketPassFairQueue output_peers_fairqueue;
    LinkedList2 output_peers_flows;
};
