/**
 * @file client.c
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

/*
 NOTE:
 This program works with I/O inside the BPending job environment.
 A consequence of this is that in response to an input, we can't
 directly do any output, but instead have to schedule outputs.
 Because all the buffers used (e.g. server send buffer, data buffers in DataProto)
 are based on flow components, it is impossible to directly write two or more
 packets to a buffer.
 To, for instance, send two packets to a buffer, we have to first schedule
 writing the second packet (using BPending), then send the first one.
*/

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <protocol/msgproto.h>
#include <protocol/addr.h>
#include <protocol/dataproto.h>
#include <misc/version.h>
#include <misc/debug.h>
#include <misc/offset.h>
#include <misc/jenkins_hash.h>
#include <misc/byteorder.h>
#include <misc/ethernet_proto.h>
#include <misc/ipv4_proto.h>
#include <misc/igmp_proto.h>
#include <misc/nsskey.h>
#include <misc/loglevel.h>
#include <misc/loggers_string.h>
#include <structure/LinkedList2.h>
#include <security/BRandom.h>
#include <nspr_support/DummyPRFileDesc.h>
#include <nspr_support/BSocketPRFileDesc.h>
#include <system/BLog.h>
#include <system/BSignal.h>
#include <system/BTime.h>
#include <system/DebugObject.h>
#include <server_connection/ServerConnection.h>

#ifndef BADVPN_USE_WINAPI
#include <system/BLog_syslog.h>
#endif

#include <client/client.h>

#include <generated/blog_channel_client.h>

#define TRANSPORT_MODE_UDP 0
#define TRANSPORT_MODE_TCP 1

#define LOGGER_STDOUT 1
#define LOGGER_SYSLOG 2

// declares and initializes a pointer x to y
#define POINTER(x, y) typeof (y) *(x) = &(y);

// command-line options
struct {
    int help;
    int version;
    int logger;
    #ifndef BADVPN_USE_WINAPI
    char *logger_syslog_facility;
    char *logger_syslog_ident;
    #endif
    int loglevel;
    int loglevels[BLOG_NUM_CHANNELS];
    int ssl;
    char *nssdb;
    char *client_cert_name;
    char *server_name;
    char *server_addr;
    int num_bind_addrs;
    struct {
        char *addr;
        int num_ports;
        int num_ext_addrs;
        struct {
            char *addr;
            char *scope;
        } ext_addrs[MAX_EXT_ADDRS];
    } bind_addrs[MAX_BIND_ADDRS];
    char *tapdev;
    int transport_mode;
    int encryption_mode;
    int hash_mode;
    int otp_mode;
    int otp_num;
    int otp_num_warn;
    int fragmentation_latency;
    int peer_ssl;
    char *scopes[MAX_SCOPES];
    int num_scopes;
    int send_buffer_size;
    int send_buffer_relay_size;
} options;

// bind addresses
int num_bind_addrs;
struct {
    BAddr addr;
    int num_ports;
    int num_ext_addrs;
    struct {
        int server_reported_port;
        BAddr addr; // if server_reported_port>=0, defined only after hello received
        char scope[64];
    } ext_addrs[MAX_EXT_ADDRS];
} bind_addrs[MAX_BIND_ADDRS];

// TCP listeners
PasswordListener listeners[MAX_BIND_ADDRS];

// SPProto parameters (UDP only)
struct spproto_security_params sp_params;

// server address we connect to
BAddr server_addr;

// server name to use for SSL
char server_name[256];

// reactor
BReactor ss;

// client certificate if using SSL
CERTCertificate *client_cert;

// client private key if using SSL
SECKEYPrivateKey *client_key;

// device data
struct device_data device;

// data communication MTU
int data_mtu;

// peers list
LinkedList2 peers;
int num_peers;

// peers by ID hash table
HashTable peers_by_id;
uint32_t peers_by_id_initval;

// MAC addresses hash table
HashTable mac_table;
uint32_t mac_table_initval;

// multicast MAC address hash table
HashTable multicast_table;
uint32_t multicast_table_initval;

// multicast entries
LinkedList2 multicast_entries_free;
struct multicast_table_entry multicast_entries_data[MAX_PEERS*PEER_MAX_GROUPS];

// peers that can be user as relays
LinkedList2 relays;

// peers than need a relay
LinkedList2 waiting_relay_peers;

// server connection
ServerConnection server;

// whether server is ready
int server_ready;

// my ID, defined only after server_ready
peerid_t my_id;

// cleans everything up that can be cleaned in order to return
// from the event loop and exit
static void terminate (void);

// prints program name and version to standard output
static void print_help (const char *name);

// prints program name and version to standard output
static void print_version (void);

// parses the command line
static int parse_arguments (int argc, char *argv[]);

// processes certain command line options
static int process_arguments (void);

// handler for program termination request
static void signal_handler (void *unused);

// provides a buffer for sending a packet to the server
static int server_start_msg (void **data, peerid_t peer_id, int type, int len);

// submits a written packet to the server
static void server_end_msg (void);

// adds a new peer
static int peer_add (peerid_t id, int flags, const uint8_t *cert, int cert_len);

// removes a peer
static void peer_remove (struct peer_data *peer);

// deallocates peer resources
static void peer_dealloc (struct peer_data *peer);

// passes a message to the logger, prepending it info about the peer
static void peer_log (struct peer_data *peer, int level, const char *fmt, ...);

// see if we are the master relative to this peer
static int peer_am_master (struct peer_data *peer);

// initializes the link
static int peer_init_link (struct peer_data *peer);

// frees link resources
static void peer_free_link (struct peer_data *peer);

// creates a fresh link
static int peer_new_link (struct peer_data *peer);

// registers the peer as a relay provider
static void peer_enable_relay_provider (struct peer_data *peer);

// unregisters the peer as a relay provider
static void peer_disable_relay_provider (struct peer_data *peer);

// deallocates peer relay provider resources. Inserts relay users to the
// need relay list. Used while freeing a peer.
static void peer_dealloc_relay_provider (struct peer_data *peer);

// install relaying for a peer
static void peer_install_relay (struct peer_data *peer, struct peer_data *relay);

// uninstall relaying for a peer
static void peer_uninstall_relay (struct peer_data *peer);

// deallocates relaying for a peer. Used when the relay is beeing freed,
// and when uninstalling relaying after having released the connection.
static void peer_dealloc_relay (struct peer_data *peer);

// handle a peer that needs a relay
static void peer_need_relay (struct peer_data *peer);

// inserts the peer into the need relay list
static void peer_register_need_relay (struct peer_data *peer);

// removes the peer from the need relay list
static void peer_unregister_need_relay (struct peer_data *peer);

// handle a link setup failure
static int peer_reset (struct peer_data *peer);

// associates a MAC address with a peer
static void peer_add_mac_address (struct peer_data *peer, uint8_t *mac);

// associate an IPv4 multicast address with a peer
static void peer_join_group (struct peer_data *peer, uint32_t group);

// disassociate an IPv4 multicast address from a peer
static void peer_leave_group (struct peer_data *peer, uint32_t group);

// handle incoming peer messages
static void peer_msg (struct peer_data *peer, uint8_t *data, int data_len);

// handlers for different message types
static void peer_msg_youconnect (struct peer_data *peer, uint8_t *data, int data_len);
static void peer_msg_cannotconnect (struct peer_data *peer, uint8_t *data, int data_len);
static void peer_msg_cannotbind (struct peer_data *peer, uint8_t *data, int data_len);
static void peer_msg_seed (struct peer_data *peer, uint8_t *data, int data_len);
static void peer_msg_confirmseed (struct peer_data *peer, uint8_t *data, int data_len);
static void peer_msg_youretry (struct peer_data *peer, uint8_t *data, int data_len);

// handler from DatagramPeerIO when we should generate a new OTP send seed
static void peer_udp_pio_handler_seed_warning (struct peer_data *peer);

// handler from StreamPeerIO when an error occurs on the connection
static void peer_tcp_pio_handler_error (struct peer_data *peer);

// peer retry timer handler. The timer is used only on the master side,
// wither when we detect an error, or the peer reports an error.
static void peer_reset_timer_handler (struct peer_data *peer);

// PacketPassInterface handler for receiving packets from the link 
static void peer_recv_handler_send (struct peer_data *peer, uint8_t *data, int data_len);

static void local_recv_qflow_output_handler_done (struct peer_data *peer);

// start binding, according to the protocol
static int peer_start_binding (struct peer_data *peer);

// tries binding on one address, according to the protocol
static int peer_bind (struct peer_data *peer);

static int peer_bind_one_address (struct peer_data *peer, int addr_index, int *cont);

static int peer_connect (struct peer_data *peer, BAddr addr, uint8_t *encryption_key, uint64_t password);

// sends a message with no payload to the peer
static int peer_send_simple (struct peer_data *peer, int msgid);

static int peer_send_conectinfo (struct peer_data *peer, int addr_index, int port_adjust, uint8_t *enckey, uint64_t pass);

static int peer_udp_send_seed (struct peer_data *peer);

static int peer_send_confirmseed (struct peer_data *peer, uint16_t seed_id);

// submits a relayed frame for sending to the peer
static void peer_submit_relayed_frame (struct peer_data *peer, struct peer_data *source_peer, uint8_t *frame, int frame_len);

// handler for group timers
static void peer_group_timer_handler (struct peer_group_entry *entry);

// handler for peer DataProto up state changes
static void peer_dataproto_handler (struct peer_data *peer, int up);

// looks for a peer with the given ID
static struct peer_data * find_peer_by_id (peerid_t id);

// multicast table operations
static void multicast_table_add_entry (struct peer_group_entry *entry);
static void multicast_table_remove_entry (struct peer_group_entry *entry);

// hash table callback functions
static int peer_groups_table_key_comparator (uint32_t *group1, uint32_t *group2);
static int peer_groups_table_hash_function (uint32_t *group, int modulo);
static int mac_table_key_comparator (uint8_t *mac1, uint8_t *mac2);
static int mac_table_hash_function (uint8_t *mac, int modulo);
static int multicast_table_key_comparator (uint32_t *sig1, uint32_t *sig2);
static int multicast_table_hash_function (uint32_t *sig, int modulo);
static int peers_by_id_key_comparator (peerid_t *id1, peerid_t *id2);
static int peers_by_id_hash_function (peerid_t *id, int modulo);

// device error handler
static void device_error_handler (void *unused);

// PacketPassInterfacre handler for packets from the device
static void device_input_handler_send (void *unused, uint8_t *data, int data_len);

// submits a local frame for sending to the peer. The frame is taken from the device frame buffer.
static void submit_frame_to_peer (struct peer_data *peer, uint8_t *data, int data_len);

// submits the current frame to all peers
static void flood_frame (uint8_t *data, int data_len);

// inspects a frame read from the device and determines how
// it should be handled. Used for IGMP snooping.
static int hook_outgoing (uint8_t *pos, int len);

#define HOOK_OUT_DEFAULT 0
#define HOOK_OUT_FLOOD 1

// inpects an incoming frame. Used for IGMP snooping.
static void peer_hook_incoming (struct peer_data *peer, uint8_t *pos, int len);

// lowers every group entry timer to IGMP_LAST_MEMBER_QUERY_TIME if it's larger
static void lower_group_timers_to_lmqt (uint32_t group);

// check an IPv4 packet
static int check_ipv4_packet (uint8_t *data, int data_len, struct ipv4_header **out_header, uint8_t **out_payload, int *out_payload_len);

// assign relays to clients waiting for them
static void assign_relays (void);

// checks if the given address scope is known (i.e. we can connect to an address in it)
static char * address_scope_known (uint8_t *name, int name_len);

// handlers for server messages
static void server_handler_error (void *user);
static void server_handler_ready (void *user, peerid_t param_my_id, uint32_t ext_ip);
static void server_handler_newclient (void *user, peerid_t peer_id, int flags, const uint8_t *cert, int cert_len);
static void server_handler_endclient (void *user, peerid_t peer_id);
static void server_handler_message (void *user, peerid_t peer_id, uint8_t *data, int data_len);

// process job handlers
static void peer_job_send_seed_after_binding (struct peer_data *peer);

int main (int argc, char *argv[])
{
    if (argc <= 0) {
        return 1;
    }
    
    // parse command-line arguments
    if (!parse_arguments(argc, argv)) {
        fprintf(stderr, "Failed to parse arguments\n");
        print_help(argv[0]);
        goto fail0;
    }
    
    // handle --help and --version
    if (options.help) {
        print_version();
        print_help(argv[0]);
        return 0;
    }
    if (options.version) {
        print_version();
        return 0;
    }
    
    // initialize logger
    switch (options.logger) {
        case LOGGER_STDOUT:
            BLog_InitStdout();
            break;
        #ifndef BADVPN_USE_WINAPI
        case LOGGER_SYSLOG:
            if (!BLog_InitSyslog(options.logger_syslog_ident, options.logger_syslog_facility)) {
                fprintf(stderr, "Failed to initialize syslog logger\n");
                goto fail0;
            }
            break;
        #endif
        default:
            ASSERT(0);
    }
    
    // configure logger channels
    for (int i = 0; i < BLOG_NUM_CHANNELS; i++) {
        if (options.loglevels[i] >= 0) {
            BLog_SetChannelLoglevel(i, options.loglevels[i]);
        }
        else if (options.loglevel >= 0) {
            BLog_SetChannelLoglevel(i, options.loglevel);
        }
    }
    
    BLog(BLOG_NOTICE, "initializing "GLOBAL_PRODUCT_NAME" client "GLOBAL_VERSION);
    
    // initialize sockets
    if (BSocket_GlobalInit() < 0) {
        BLog(BLOG_ERROR, "BSocket_GlobalInit failed");
        goto fail1;
    }
    
    // init time
    BTime_Init();
    
    // process arguments
    if (!process_arguments()) {
        BLog(BLOG_ERROR, "Failed to process arguments");
        goto fail1;
    }
    
    // init reactor
    if (!BReactor_Init(&ss)) {
        BLog(BLOG_ERROR, "BReactor_Init failed");
        goto fail1;
    }
    
    // setup signal handler
    if (!BSignal_Init()) {
        BLog(BLOG_ERROR, "BSignal_Init failed");
        goto fail1b;
    }
    BSignal_Capture();
    if (!BSignal_SetHandler(&ss, signal_handler, NULL)) {
        BLog(BLOG_ERROR, "BSignal_SetHandler failed");
        goto fail1b;
    }
    
    if (options.ssl) {
        // init NSPR
        PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
        
        // register local NSPR file types
        if (!DummyPRFileDesc_GlobalInit()) {
            BLog(BLOG_ERROR, "DummyPRFileDesc_GlobalInit failed");
            goto fail2;
        }
        if (!BSocketPRFileDesc_GlobalInit()) {
            BLog(BLOG_ERROR, "BSocketPRFileDesc_GlobalInit failed");
            goto fail2;
        }
        
        // init NSS
        if (NSS_Init(options.nssdb) != SECSuccess) {
            BLog(BLOG_ERROR, "NSS_Init failed (%d)", (int)PR_GetError());
            goto fail2;
        }
        
        // set cipher policy
        if (NSS_SetDomesticPolicy() != SECSuccess) {
            BLog(BLOG_ERROR, "NSS_SetDomesticPolicy failed (%d)", (int)PR_GetError());
            goto fail3;
        }
        
        // init server cache
        if (SSL_ConfigServerSessionIDCache(0, 0, 0, NULL) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_ConfigServerSessionIDCache failed (%d)", (int)PR_GetError());
            goto fail3;
        }
        
        // open server certificate and private key
        if (!open_nss_cert_and_key(options.client_cert_name, &client_cert, &client_key)) {
            BLog(BLOG_ERROR, "Cannot open certificate and key");
            goto fail4;
        }
    }
    
    // init listeners
    int num_listeners = 0;
    if (options.transport_mode == TRANSPORT_MODE_TCP) {
        while (num_listeners < num_bind_addrs) {
            POINTER(addr, bind_addrs[num_listeners])
            if (!PasswordListener_Init(
                &listeners[num_listeners], &ss, addr->addr, 50, options.peer_ssl,
                (options.peer_ssl ? client_cert : NULL),
                (options.peer_ssl ? client_key : NULL)
            )) {
                BLog(BLOG_ERROR, "PasswordListener_Init failed");
                goto fail5;
            }
            num_listeners++;
        }
    }
    
    // init device
    if (!BTap_Init(&device.btap, &ss, options.tapdev, device_error_handler, NULL)) {
        BLog(BLOG_ERROR, "BTap_Init failed");
        goto fail5;
    }
    
    // remember device MTU
    device.mtu = sizeof(struct ethernet_header) + BTap_GetDeviceMTU(&device.btap);
    
    BLog(BLOG_INFO, "device MTU is %d", device.mtu);
    
    // init device input
    PacketPassInterface_Init(&device.input_interface, device.mtu, device_input_handler_send, NULL, BReactor_PendingGroup(&ss));
    if (!SinglePacketBuffer_Init(&device.input_buffer, BTap_GetOutput(&device.btap), &device.input_interface, BReactor_PendingGroup(&ss))) {
        goto fail5a;
    }
    
    // init device output
    PacketPassFairQueue_Init(&device.output_queue, BTap_GetInput(&device.btap), BReactor_PendingGroup(&ss));
    PacketPassFairQueue_EnableCancel(&device.output_queue);
    
    // calculate data MTU
    data_mtu = DATAPROTO_MAX_OVERHEAD + device.mtu;
    
    // init peers list
    LinkedList2_Init(&peers);
    num_peers = 0;
    
    // init peers by ID hash table
    BRandom_randomize((uint8_t *)&peers_by_id_initval, sizeof(peers_by_id_initval));
    if (!HashTable_Init(
        &peers_by_id,
        OFFSET_DIFF(struct peer_data, id, table_node),
        (HashTable_comparator)peers_by_id_key_comparator,
        (HashTable_hash_function)peers_by_id_hash_function,
        MAX_PEERS
    )) {
        BLog(BLOG_ERROR, "HashTable_Init failed");
        goto fail7;
    }
    
    // init MAC address table
    BRandom_randomize((uint8_t *)&mac_table_initval, sizeof(mac_table_initval));
    if (!HashTable_Init(
        &mac_table,
        OFFSET_DIFF(struct mac_table_entry, mac, table_node),
        (HashTable_comparator)mac_table_key_comparator,
        (HashTable_hash_function)mac_table_hash_function,
        MAX_PEERS * PEER_MAX_MACS
    )) {
        BLog(BLOG_ERROR, "HashTable_Init failed");
        goto fail8;
    }
    
    // init multicast MAC address table
    BRandom_randomize((uint8_t *)&multicast_table_initval, sizeof(multicast_table_initval));
    if (!HashTable_Init(
        &multicast_table,
        OFFSET_DIFF(struct multicast_table_entry, sig, table_node),
        (HashTable_comparator)multicast_table_key_comparator,
        (HashTable_hash_function)multicast_table_hash_function,
        MAX_PEERS * PEER_MAX_GROUPS
    )) {
        BLog(BLOG_ERROR, "HashTable_Init failed");
        goto fail9;
    }
    
    // init multicast entries
    LinkedList2_Init(&multicast_entries_free);
    int i;
    for (i = 0; i < MAX_PEERS * PEER_MAX_GROUPS; i++) {
        struct multicast_table_entry *multicast_entry = &multicast_entries_data[i];
        LinkedList2_Append(&multicast_entries_free, &multicast_entry->free_list_node);
    }
    
    // init relays list
    LinkedList2_Init(&relays);
    
    // init need relay list
    LinkedList2_Init(&waiting_relay_peers);
    
    // start connecting to server
    if (!ServerConnection_Init(
        &server, &ss, server_addr, SC_KEEPALIVE_INTERVAL, SERVER_BUFFER_MIN_PACKETS, options.ssl, client_cert, client_key, server_name, NULL,
        server_handler_error, server_handler_ready, server_handler_newclient, server_handler_endclient, server_handler_message
    )) {
        BLog(BLOG_ERROR, "ServerConnection_Init failed");
        goto fail10;
    }
    
    // set server not ready
    server_ready = 0;
    
    goto event_loop;
    
    // cleanup on error
fail10:
    HashTable_Free(&multicast_table);
fail9:
    HashTable_Free(&mac_table);
fail8:
    HashTable_Free(&peers_by_id);
fail7:
    PacketPassFairQueue_Free(&device.output_queue);
    SinglePacketBuffer_Free(&device.input_buffer);
fail5a:
    PacketPassInterface_Free(&device.input_interface);
    BTap_Free(&device.btap);
fail5:
    if (options.transport_mode == TRANSPORT_MODE_TCP) {
        while (num_listeners-- > 0) {
            PasswordListener_Free(&listeners[num_listeners]);
        }
    }
fail4a:
    if (options.ssl) {
        CERT_DestroyCertificate(client_cert);
        SECKEY_DestroyPrivateKey(client_key);
fail4:
        SSL_ShutdownServerSessionIDCache();
fail3:
        SSL_ClearSessionCache();
        ASSERT_FORCE(NSS_Shutdown() == SECSuccess)
fail2:
        ASSERT_FORCE(PR_Cleanup() == PR_SUCCESS)
        PL_ArenaFinish();
    }
    BSignal_RemoveHandler();
fail1b:
    BReactor_Free(&ss);
fail1:
    BLog(BLOG_ERROR, "initialization failed");
    BLog_Free();
fail0:
    // finish objects
    DebugObjectGlobal_Finish();
    return 1;
    
event_loop:
    // enter event loop
    BLog(BLOG_NOTICE, "entering event loop");
    int ret = BReactor_Exec(&ss);
    
    // free reactor
    BReactor_Free(&ss);
    
    // free logger
    BLog(BLOG_NOTICE, "exiting");
    BLog_Free();
    
    // finish objects
    DebugObjectGlobal_Finish();
    
    return ret;
}

void terminate (void)
{
    BLog(BLOG_NOTICE, "tearing down");
    
    // allow freeing local receive flows
    PacketPassFairQueue_PrepareFree(&device.output_queue);
    
    // free peers
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&peers)) {
        struct peer_data *peer = UPPER_OBJECT(node, struct peer_data, list_node);
        
        // free relaying
        if (peer->have_relaying) {
            struct peer_data *relay = peer->relaying_peer;
            ASSERT(relay->is_relay)
            ASSERT(relay->have_link)
            
            // free relay provider
            peer_dealloc_relay_provider(relay);
        }
        
        // free relay provider
        if (peer->is_relay) {
            peer_dealloc_relay_provider(peer);
        }
        
        // free relay source
        DataProtoRelaySource_FreeRelease(&peer->relay_source);
        
        // deallocate peer
        peer_dealloc(peer);
    }
    
    // free server
    ServerConnection_Free(&server);
    
    // free hash tables
    HashTable_Free(&multicast_table);
    HashTable_Free(&mac_table);
    HashTable_Free(&peers_by_id);
    
    // free device output
    PacketPassFairQueue_Free(&device.output_queue);
    
    // free device input
    SinglePacketBuffer_Free(&device.input_buffer);
    PacketPassInterface_Free(&device.input_interface);
    
    // free device
    BTap_Free(&device.btap);
    
    // free listeners
    if (options.transport_mode == TRANSPORT_MODE_TCP) {
        for (int i = num_bind_addrs - 1; i >= 0; i--) {
            PasswordListener_Free(&listeners[i]);
        }
    }
    
    if (options.ssl) {
        // free client certificate and private key
        CERT_DestroyCertificate(client_cert);
        SECKEY_DestroyPrivateKey(client_key);
        
        // free server cache
        ASSERT_FORCE(SSL_ShutdownServerSessionIDCache() == SECSuccess)
        
        // free client cache
        SSL_ClearSessionCache();
        
        // free NSS
        ASSERT_FORCE(NSS_Shutdown() == SECSuccess)
        
        // free NSPR
        ASSERT_FORCE(PR_Cleanup() == PR_SUCCESS)
        PL_ArenaFinish();
    }
    
    // remove signal handler
    BSignal_RemoveHandler();
    
    // exit reactor
    BReactor_Quit(&ss, 1);
}

void print_help (const char *name)
{
    printf(
        "Usage:\n"
        "    %s\n"
        "        [--help]\n"
        "        [--version]\n"
        "        [--logger <"LOGGERS_STRING">]\n"
        #ifndef BADVPN_USE_WINAPI
        "        (logger=syslog?\n"
        "            [--syslog-facility <string>]\n"
        "            [--syslog-ident <string>]\n"
        "        )\n"
        #endif
        "        [--loglevel <0-5/none/error/warning/notice/info/debug>]\n"
        "        [--channel-loglevel <channel-name> <0-5/none/error/warning/notice/info/debug>] ...\n"
        "        [--ssl --nssdb <string> --client-cert-name <string>]\n"
        "        [--server-name <string>]\n"
        "        --server-addr <addr>\n"
        "        [--tapdev <name>]\n"
        "        [--scope <scope_name>] ...\n"
        "        [\n"
        "            --bind-addr <addr>\n"
        "            (transport-mode=udp? --num-ports <num>)\n"
        "            [--ext-addr <addr / {server_reported}:port> <scope_name>] ...\n"
        "        ] ...\n"
        "        --transport-mode <udp/tcp>\n"
        "        (transport-mode=udp?\n"
        "            --encryption-mode <blowfish/aes/none>\n"
        "            --hash-mode <md5/sha1/none>\n"
        "            [--otp <blowfish/aes> <num> <num-warn>]\n"
        "            [--fragmentation-latency <milliseconds>]\n"
        "        )\n"
        "        (transport-mode=tcp?\n"
        "            (ssl? [--peer-ssl])\n"
        "        )\n"
        "        [--send-buffer-size <num-packets>]\n"
        "        [--send-buffer-relay-size <num-packets>]\n"
        "Address format is a.b.c.d:port (IPv4) or [addr]:port (IPv6).\n",
        name
    );
}

void print_version (void)
{
    printf(GLOBAL_PRODUCT_NAME" "PROGRAM_NAME" "GLOBAL_VERSION"\n"GLOBAL_COPYRIGHT_NOTICE"\n");
}

int parse_arguments (int argc, char *argv[])
{
    if (argc <= 0) {
        return 0;
    }
    
    options.help = 0;
    options.version = 0;
    options.logger = LOGGER_STDOUT;
    #ifndef BADVPN_USE_WINAPI
    options.logger_syslog_facility = "daemon";
    options.logger_syslog_ident = argv[0];
    #endif
    options.loglevel = -1;
    for (int i = 0; i < BLOG_NUM_CHANNELS; i++) {
        options.loglevels[i] = -1;
    }
    options.ssl = 0;
    options.nssdb = NULL;
    options.client_cert_name = NULL;
    options.server_name = NULL;
    options.server_addr = NULL;
    options.tapdev = NULL;
    options.num_scopes = 0;
    options.num_bind_addrs = 0;
    options.transport_mode = -1;
    options.encryption_mode = -1;
    options.hash_mode = -1;
    options.otp_mode = SPPROTO_OTP_MODE_NONE;
    options.fragmentation_latency = PEER_DEFAULT_FRAGMENTATION_LATENCY;
    options.peer_ssl = 0;
    options.send_buffer_size = PEER_DEFAULT_SEND_BUFFER_SIZE;
    options.send_buffer_relay_size = PEER_DEFAULT_SEND_BUFFER_RELAY_SIZE;
    
    int have_fragmentation_latency = 0;
    
    int i;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (!strcmp(arg, "--help")) {
            options.help = 1;
        }
        else if (!strcmp(arg, "--version")) {
            options.version = 1;
        }
        else if (!strcmp(arg, "--logger")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            char *arg2 = argv[i + 1];
            if (!strcmp(arg2, "stdout")) {
                options.logger = LOGGER_STDOUT;
            }
            #ifndef BADVPN_USE_WINAPI
            else if (!strcmp(arg2, "syslog")) {
                options.logger = LOGGER_SYSLOG;
            }
            #endif
            else {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        #ifndef BADVPN_USE_WINAPI
        else if (!strcmp(arg, "--syslog-facility")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.logger_syslog_facility = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--syslog-ident")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.logger_syslog_ident = argv[i + 1];
            i++;
        }
        #endif
        else if (!strcmp(arg, "--loglevel")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if ((options.loglevel = parse_loglevel(argv[i + 1])) < 0) {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        else if (!strcmp(arg, "--channel-loglevel")) {
            if (2 >= argc - i) {
                fprintf(stderr, "%s: requires two arguments\n", arg);
                return 0;
            }
            int channel = BLogGlobal_GetChannelByName(argv[i + 1]);
            if (channel < 0) {
                fprintf(stderr, "%s: wrong channel argument\n", arg);
                return 0;
            }
            int loglevel = parse_loglevel(argv[i + 2]);
            if (loglevel < 0) {
                fprintf(stderr, "%s: wrong loglevel argument\n", arg);
                return 0;
            }
            options.loglevels[channel] = loglevel;
            i += 2;
        }
        else if (!strcmp(arg, "--ssl")) {
            options.ssl = 1;
        }
        else if (!strcmp(arg, "--nssdb")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.nssdb = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--client-cert-name")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.client_cert_name = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--server-name")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.server_name = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--server-addr")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.server_addr = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--tapdev")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.tapdev = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--scope")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if (options.num_scopes == MAX_SCOPES) {
                fprintf(stderr, "%s: too many\n", arg);
                return 0;
            }
            options.scopes[options.num_scopes] = argv[i + 1];
            options.num_scopes++;
            i++;
        }
        else if (!strcmp(arg, "--bind-addr")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if (options.num_bind_addrs == MAX_BIND_ADDRS) {
                fprintf(stderr, "%s: too many\n", arg);
                return 0;
            }
            POINTER(addr, options.bind_addrs[options.num_bind_addrs])
            addr->addr = argv[i + 1];
            addr->num_ports = -1;
            addr->num_ext_addrs = 0;
            options.num_bind_addrs++;
            i++;
        }
        else if (!strcmp(arg, "--num-ports")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if (options.num_bind_addrs == 0) {
                fprintf(stderr, "%s: must folow --bind-addr\n", arg);
                return 0;
            }
            POINTER(addr, options.bind_addrs[options.num_bind_addrs - 1])
            if ((addr->num_ports = atoi(argv[i + 1])) < 0) {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        else if (!strcmp(arg, "--ext-addr")) {
            if (2 >= argc - i) {
                fprintf(stderr, "%s: requires two arguments\n", arg);
                return 0;
            }
            if (options.num_bind_addrs == 0) {
                fprintf(stderr, "%s: must folow --bind-addr\n", arg);
                return 0;
            }
            POINTER(addr, options.bind_addrs[options.num_bind_addrs - 1])
            if (addr->num_ext_addrs == MAX_EXT_ADDRS) {
                fprintf(stderr, "%s: too many\n", arg);
                return 0;
            }
            POINTER(eaddr, addr->ext_addrs[addr->num_ext_addrs])
            eaddr->addr = argv[i + 1];
            eaddr->scope = argv[i + 2];
            addr->num_ext_addrs++;
            i += 2;
        }
        else if (!strcmp(arg, "--transport-mode")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            char *arg2 = argv[i + 1];
            if (!strcmp(arg2, "udp")) {
                options.transport_mode = TRANSPORT_MODE_UDP;
            }
            else if (!strcmp(arg2, "tcp")) {
                options.transport_mode = TRANSPORT_MODE_TCP;
            }
            else {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        else if (!strcmp(arg, "--encryption-mode")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            char *arg2 = argv[i + 1];
            if (!strcmp(arg2, "none")) {
                options.encryption_mode = SPPROTO_ENCRYPTION_MODE_NONE;
            }
            else if (!strcmp(arg2, "blowfish")) {
                options.encryption_mode = BENCRYPTION_CIPHER_BLOWFISH;
            }
            else if (!strcmp(arg2, "aes")) {
                options.encryption_mode = BENCRYPTION_CIPHER_AES;
            }
            else {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        else if (!strcmp(arg, "--hash-mode")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            char *arg2 = argv[i + 1];
            if (!strcmp(arg2, "none")) {
                options.hash_mode = SPPROTO_HASH_MODE_NONE;
            }
            else if (!strcmp(arg2, "md5")) {
                options.hash_mode = BHASH_TYPE_MD5;
            }
            else if (!strcmp(arg2, "sha1")) {
                options.hash_mode = BHASH_TYPE_SHA1;
            }
            else {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        else if (!strcmp(arg, "--otp")) {
            if (3 >= argc - i) {
                fprintf(stderr, "%s: requires three arguments\n", arg);
                return 0;
            }
            char *otp_mode = argv[i + 1];
            char *otp_num = argv[i + 2];
            char *otp_num_warn = argv[i + 3];
            if (!strcmp(otp_mode, "blowfish")) {
                options.otp_mode = BENCRYPTION_CIPHER_BLOWFISH;
            }
            else if (!strcmp(otp_mode, "aes")) {
                options.otp_mode = BENCRYPTION_CIPHER_AES;
            }
            else {
                fprintf(stderr, "%s: wrong mode\n", arg);
                return 0;
            }
            if ((options.otp_num = atoi(otp_num)) <= 0) {
                fprintf(stderr, "%s: wrong num\n", arg);
                return 0;
            }
            options.otp_num_warn = atoi(otp_num_warn);
            if (options.otp_num_warn <= 0 || options.otp_num_warn > options.otp_num) {
                fprintf(stderr, "%s: wrong num warn\n", arg);
                return 0;
            }
            i += 3;
        }
        else if (!strcmp(arg, "--fragmentation-latency")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.fragmentation_latency = atoi(argv[i + 1]);
            have_fragmentation_latency = 1;
            i++;
        }
        else if (!strcmp(arg, "--peer-ssl")) {
            options.peer_ssl = 1;
        }
        else if (!strcmp(arg, "--send-buffer-size")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if ((options.send_buffer_size = atoi(argv[i + 1])) <= 0) {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        else if (!strcmp(arg, "--send-buffer-relay-size")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if ((options.send_buffer_relay_size = atoi(argv[i + 1])) <= 0) {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        else {
            fprintf(stderr, "unknown option: %s\n", arg);
            return 0;
        }
    }
    
    if (options.help || options.version) {
        return 1;
    }
    
    if (options.ssl != !!options.nssdb) {
        fprintf(stderr, "False: --ssl <=> --nssdb\n");
        return 0;
    }
    
    if (options.ssl != !!options.client_cert_name) {
        fprintf(stderr, "False: --ssl <=> --client-cert-name\n");
        return 0;
    }
    
    if (!options.server_addr) {
        fprintf(stderr, "False: --server-addr\n");
        return 0;
    }
    
    if (options.transport_mode < 0) {
        fprintf(stderr, "False: --transport-mode\n");
        return 0;
    }
    
    if ((options.transport_mode == TRANSPORT_MODE_UDP) != (options.encryption_mode >= 0)) {
        fprintf(stderr, "False: UDP <=> --encryption-mode\n");
        return 0;
    }
    
    if ((options.transport_mode == TRANSPORT_MODE_UDP) != (options.hash_mode >= 0)) {
        fprintf(stderr, "False: UDP <=> --hash-mode\n");
        return 0;
    }
    
    if (!(!(options.otp_mode != SPPROTO_OTP_MODE_NONE) || (options.transport_mode == TRANSPORT_MODE_UDP))) {
        fprintf(stderr, "False: --otp => UDP\n");
        return 0;
    }
    
    if (!(!have_fragmentation_latency || (options.transport_mode == TRANSPORT_MODE_UDP))) {
        fprintf(stderr, "False: --fragmentation-latency => UDP\n");
        return 0;
    }
    
    if (!(!options.peer_ssl || (options.ssl && options.transport_mode == TRANSPORT_MODE_TCP))) {
        fprintf(stderr, "False: --peer-ssl => (--ssl && TCP)\n");
        return 0;
    }
    
    return 1;
}

int process_arguments (void)
{
    // resolve server address
    ASSERT(options.server_addr)
    if (!BAddr_Parse(&server_addr, options.server_addr, server_name, sizeof(server_name))) {
        BLog(BLOG_ERROR, "server addr: BAddr_Parse failed");
        return 0;
    }
    if (!addr_supported(server_addr)) {
        BLog(BLOG_ERROR, "server addr: not supported");
        return 0;
    }
    
    // override server name if requested
    if (options.server_name) {
        snprintf(server_name, sizeof(server_name), "%s", options.server_name);
    }
    
    // resolve bind addresses and external addresses
    num_bind_addrs = 0;
    for (int i = 0; i < options.num_bind_addrs; i++) {
        POINTER(addr, options.bind_addrs[i])
        POINTER(out, bind_addrs[num_bind_addrs])
        
        // read addr
        if (!BAddr_Parse(&out->addr, addr->addr, NULL, 0)) {
            BLog(BLOG_ERROR, "bind addr: BAddr_Parse failed");
            return 0;
        }
        
        // read num ports
        if (options.transport_mode == TRANSPORT_MODE_UDP) {
            if (addr->num_ports < 0) {
                BLog(BLOG_ERROR, "bind addr: num ports missing");
                return 0;
            }
            out->num_ports = addr->num_ports;
        }
        else if (addr->num_ports >= 0) {
            BLog(BLOG_ERROR, "bind addr: num ports given, but not using UDP");
            return 0;
        }
        
        // read ext addrs
        out->num_ext_addrs = 0;
        for (int j = 0; j < addr->num_ext_addrs; j++) {
            POINTER(eaddr, addr->ext_addrs[j])
            POINTER(eout, out->ext_addrs[out->num_ext_addrs])
            
            // read addr
            char *colon = strstr(eaddr->addr, ":");
            if (!colon) {
                BLog(BLOG_ERROR, "ext addr: no colon");
                return 0;
            }
            char addrstr[colon - eaddr->addr + 1];
            memcpy(addrstr, eaddr->addr, colon - eaddr->addr);
            addrstr[colon - eaddr->addr] = '\0';
            if (!strcmp(addrstr, "{server_reported}")) {
                if ((eout->server_reported_port = atoi(colon + 1)) < 0) {
                    BLog(BLOG_ERROR, "ext addr: wrong port");
                    return 0;
                }
            } else {
                eout->server_reported_port = -1;
                if (!BAddr_Parse(&eout->addr, eaddr->addr, NULL, 0)) {
                    BLog(BLOG_ERROR, "ext addr: BAddr_Parse failed");
                    return 0;
                }
            }
            
            // read scope
            snprintf(eout->scope, sizeof(eout->scope), "%s", eaddr->scope);
            
            out->num_ext_addrs++;
        }
        
        num_bind_addrs++;
    }
    
    // initialize SPProto parameters
    if (options.transport_mode == TRANSPORT_MODE_UDP) {
        sp_params.encryption_mode = options.encryption_mode;
        sp_params.hash_mode = options.hash_mode;
        sp_params.otp_mode = options.otp_mode;
        if (options.otp_mode > 0) {
            sp_params.otp_num = options.otp_num;
        }
    }
    
    return 1;
}

void signal_handler (void *unused)
{
    BLog(BLOG_NOTICE, "termination requested");
    
    terminate();
    return;
}

int server_start_msg (void **data, peerid_t peer_id, int type, int len)
{
    ASSERT(server_ready)
    ASSERT(len >= 0)
    ASSERT(len <= MSG_MAX_PAYLOAD)
    ASSERT(!(len > 0) || data)
    
    uint8_t *packet;
    if (!ServerConnection_StartMessage(&server, (void **)&packet, peer_id, msg_SIZEtype + msg_SIZEpayload(len))) {
        BLog(BLOG_ERROR, "out of server buffer, exiting");
        terminate();
        return -1;
    }
    
    msgWriter writer;
    msgWriter_Init(&writer, packet);
    msgWriter_Addtype(&writer, type);
    uint8_t *payload_dst = msgWriter_Addpayload(&writer, len);
    msgWriter_Finish(&writer);
    
    if (data) {
        *data = payload_dst;
    }
    
    return 0;
}

void server_end_msg (void)
{
    ASSERT(server_ready)
    
    ServerConnection_EndMessage(&server);
}

int peer_add (peerid_t id, int flags, const uint8_t *cert, int cert_len)
{
    ASSERT(server_ready)
    ASSERT(num_peers < MAX_PEERS)
    ASSERT(!find_peer_by_id(id))
    ASSERT(id != my_id)
    ASSERT(cert_len >= 0)
    ASSERT(cert_len <= SCID_NEWCLIENT_MAX_CERT_LEN)
    
    // allocate structure
    struct peer_data *peer = malloc(sizeof(*peer));
    if (!peer) {
        BLog(BLOG_ERROR, "peer %d: failed to allocate memory", (int)id);
        goto fail0;
    }
    
    // remember id
    peer->id = id;
    
    // remember flags
    peer->flags = flags;
    
    // remember certificate if using SSL
    if (options.ssl) {
        memcpy(peer->cert, cert, cert_len);
        peer->cert_len = cert_len;
    }
    
    // init local flow
    if (!DataProtoLocalSource_Init(&peer->local_dpflow, device.mtu, my_id, peer->id, options.send_buffer_size, &ss)) {
        peer_log(peer, BLOG_ERROR, "DataProtoLocalSource_Init failed");
        goto fail1;
    }
    
    // init local receive flow
    PacketPassFairQueueFlow_Init(&peer->local_recv_qflow, &device.output_queue);
    peer->local_recv_if = PacketPassFairQueueFlow_GetInput(&peer->local_recv_qflow);
    PacketPassInterface_Sender_Init(peer->local_recv_if, (PacketPassInterface_handler_done)local_recv_qflow_output_handler_done, peer);
    
    // init relay source
    DataProtoRelaySource_Init(&peer->relay_source, peer->id);
    
    // have no link
    peer->have_link = 0;
    
    // allocate OTP seed buffers
    if (options.transport_mode == TRANSPORT_MODE_UDP && SPPROTO_HAVE_OTP(sp_params)) {
        if (!(peer->pio.udp.sendseed_sent_key = malloc(BEncryption_cipher_key_size(sp_params.otp_mode)))) {
            goto fail3;
        }
        if (!(peer->pio.udp.sendseed_sent_iv = malloc(BEncryption_cipher_block_size(sp_params.otp_mode)))) {
            goto fail4;
        }
    }
    
    // have no relaying
    peer->have_relaying = 0;
    
    // not waiting for relay
    peer->waiting_relay = 0;
    
    // init retry timer
    BTimer_Init(&peer->reset_timer, PEER_RETRY_TIME, (BTimer_handler)peer_reset_timer_handler, peer);
    
    // init MAC lists
    LinkedList2_Init(&peer->macs_used);
    LinkedList2_Init(&peer->macs_free);
    // init MAC entries and add them to the free list
    for (int i = 0; i < PEER_MAX_MACS; i++) {
        struct mac_table_entry *entry = &peer->macs_data[i];
        entry->peer = peer;
        LinkedList2_Append(&peer->macs_free, &entry->list_node);
    }
    
    // init groups lists
    LinkedList2_Init(&peer->groups_used);
    LinkedList2_Init(&peer->groups_free);
    // init group entries and add to unused list
    for (int i = 0; i < PEER_MAX_GROUPS; i++) {
        struct peer_group_entry *entry = &peer->groups_data[i];
        entry->peer = peer;
        LinkedList2_Append(&peer->groups_free, &entry->list_node);
        BTimer_Init(&entry->timer, 0, (BTimer_handler)peer_group_timer_handler, entry);
    }
    // init groups hash table
    if (!HashTable_Init(
        &peer->groups_hashtable,
        OFFSET_DIFF(struct peer_group_entry, group, table_node),
        (HashTable_comparator)peer_groups_table_key_comparator,
        (HashTable_hash_function)peer_groups_table_hash_function,
        PEER_MAX_GROUPS
    )) {
        peer_log(peer, BLOG_ERROR, "HashTable_Init failed");
        goto fail5;
    }
    
    // is not relay server
    peer->is_relay = 0;
    
    // init binding
    peer->binding = 0;
    
    // init jobs
    BPending_Init(&peer->job_send_seed_after_binding, BReactor_PendingGroup(&ss), (BPending_handler)peer_job_send_seed_after_binding, peer);
    
    // add to peers linked list
    LinkedList2_Append(&peers, &peer->list_node);
    
    // add to peers-by-ID hash table
    ASSERT_EXECUTE(HashTable_Insert(&peers_by_id, &peer->table_node))
    
    // increment number of peers
    num_peers++;
    
    peer_log(peer, BLOG_INFO, "initialized");
    
    // start setup process
    if (peer_am_master(peer)) {
        return peer_start_binding(peer);
    } else {
        return 0;
    }
    
fail5:
    if (options.transport_mode == TRANSPORT_MODE_UDP && SPPROTO_HAVE_OTP(sp_params)) {
        free(peer->pio.udp.sendseed_sent_iv);
fail4:
        free(peer->pio.udp.sendseed_sent_key);
    }
fail3:
    DataProtoRelaySource_Free(&peer->relay_source);
    PacketPassFairQueueFlow_Free(&peer->local_recv_qflow);
    DataProtoLocalSource_Free(&peer->local_dpflow);
fail1:
    free(peer);
fail0:
    return 0;
}

void peer_remove (struct peer_data *peer)
{
    peer_log(peer, BLOG_INFO, "removing");
    
    // uninstall relaying
    if (peer->have_relaying) {
        peer_uninstall_relay(peer);
    }
    
    // disable relay provider
    // this inserts former relay users into the need relay list
    if (peer->is_relay) {
        peer_dealloc_relay_provider(peer);
    }
    
    // release relay flows
    DataProtoRelaySource_Release(&peer->relay_source);
    
    // release local receive flow
    if (PacketPassFairQueueFlow_IsBusy(&peer->local_recv_qflow)) {
        PacketPassFairQueueFlow_Release(&peer->local_recv_qflow);
    }
    
    // deallocate peer
    peer_dealloc(peer);
    
    // assign relays because former relay users are disconnected above
    assign_relays();
}

void peer_dealloc (struct peer_data *peer)
{
    ASSERT(!peer->have_relaying)
    ASSERT(!peer->is_relay)
    ASSERT(DataProtoRelaySource_IsEmpty(&peer->relay_source))
    PacketPassFairQueueFlow_AssertFree(&peer->local_recv_qflow);
    
    LinkedList2Iterator it;
    LinkedList2Node *node;
    
    // remove from waiting relay list
    if (peer->waiting_relay) {
        peer_unregister_need_relay(peer);
    }
    
    // free link
    if (peer->have_link) {
        peer_free_link(peer);
    }
    
    // free group entries
    LinkedList2Iterator_InitForward(&it, &peer->groups_used);
    while (node = LinkedList2Iterator_Next(&it)) {
        struct peer_group_entry *group_entry = UPPER_OBJECT(node, struct peer_group_entry, list_node);
        ASSERT(group_entry->peer == peer)
        multicast_table_remove_entry(group_entry);
        BReactor_RemoveTimer(&ss, &group_entry->timer);
    }
    
    // free MAC addresses
    LinkedList2Iterator_InitForward(&it, &peer->macs_used);
    while (node = LinkedList2Iterator_Next(&it)) {
        struct mac_table_entry *mac_entry = UPPER_OBJECT(node, struct mac_table_entry, list_node);
        ASSERT(mac_entry->peer == peer)
        ASSERT_EXECUTE(HashTable_Remove(&mac_table, mac_entry->mac))
    }
    
    // decrement number of peers
    num_peers--;
    
    // remove from peers-by-ID hash table
    ASSERT_EXECUTE(HashTable_Remove(&peers_by_id, &peer->id))
    
    // remove from peers linked list
    LinkedList2_Remove(&peers, &peer->list_node);
    
    // free jobs
    BPending_Free(&peer->job_send_seed_after_binding);
    
    // free groups table
    HashTable_Free(&peer->groups_hashtable);
    
    // free retry timer
    BReactor_RemoveTimer(&ss, &peer->reset_timer);
    
    // free OTP seed buffers
    if (options.transport_mode == TRANSPORT_MODE_UDP && SPPROTO_HAVE_OTP(sp_params)) {
        free(peer->pio.udp.sendseed_sent_iv);
        free(peer->pio.udp.sendseed_sent_key);
    }
    
    // free relay source
    DataProtoRelaySource_Free(&peer->relay_source);
    
    // free local receive flow
    PacketPassFairQueueFlow_Free(&peer->local_recv_qflow);
    
    // free local flow
    DataProtoLocalSource_Free(&peer->local_dpflow);
    
    // free peer structure
    free(peer);
}

void peer_log (struct peer_data *peer, int level, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    BLog_Append("peer %d: ", (int)peer->id);
    BLog_LogToChannelVarArg(BLOG_CURRENT_CHANNEL, level, fmt, vl);
    va_end(vl);
}

int peer_am_master (struct peer_data *peer)
{
    return (my_id > peer->id);
}

int peer_init_link (struct peer_data *peer)
{
    ASSERT(!peer->have_link)
    ASSERT(!peer->have_relaying)
    ASSERT(!peer->waiting_relay)
    
    ASSERT(!peer->is_relay)
    
    // init link receive interface
    PacketPassInterface_Init(&peer->recv_ppi, data_mtu, (PacketPassInterface_handler_send)peer_recv_handler_send, peer,  BReactor_PendingGroup(&ss));
    
    // init transport-specific link objects
    PacketPassInterface *link_if;
    if (options.transport_mode == TRANSPORT_MODE_UDP) {
        // init DatagramPeerIO
        if (!DatagramPeerIO_Init(
            &peer->pio.udp.pio, &ss, data_mtu, CLIENT_UDP_MTU,
            sp_params, options.fragmentation_latency,
            &peer->recv_ppi
        )) {
            peer_log(peer, BLOG_ERROR, "DatagramPeerIO_Init failed");
            goto fail1;
        }
        
        // init OTP warning handler
        if (SPPROTO_HAVE_OTP(sp_params)) {
            DatagramPeerIO_SetOTPWarningHandler(&peer->pio.udp.pio, (DatagramPeerIO_handler_otp_warning)peer_udp_pio_handler_seed_warning, peer, options.otp_num_warn);
        }
        
        // init send seed state
        if (SPPROTO_HAVE_OTP(sp_params)) {
            peer->pio.udp.sendseed_nextid = 0;
            peer->pio.udp.sendseed_sent = 0;
        }
        
        link_if = DatagramPeerIO_GetSendInput(&peer->pio.udp.pio);
    } else {
        // init StreamPeerIO
        if (!StreamPeerIO_Init(
            &peer->pio.tcp.pio, &ss, options.peer_ssl,
            (options.peer_ssl ? peer->cert : NULL),
            (options.peer_ssl ? peer->cert_len : -1),
            data_mtu, &peer->recv_ppi,
            (StreamPeerIO_handler_error)peer_tcp_pio_handler_error, peer
        )) {
            peer_log(peer, BLOG_ERROR, "StreamPeerIO_Init failed");
            goto fail1;
        }
        
        link_if = StreamPeerIO_GetSendInput(&peer->pio.tcp.pio);
    }
    
    // init sending
    if (!DataProtoDest_Init(&peer->send_dp, &ss, peer->id, link_if, PEER_KEEPALIVE_INTERVAL, PEER_KEEPALIVE_RECEIVE_TIMER, (DataProtoDest_handler)peer_dataproto_handler, peer)) {
        peer_log(peer, BLOG_ERROR, "DataProto_Init failed");
        goto fail2;
    }
    
    // attach local flow to our DataProto
    DataProtoLocalSource_Attach(&peer->local_dpflow, &peer->send_dp);
    
    peer->have_link = 1;
    
    return 1;
    
fail2:
    if (options.transport_mode == TRANSPORT_MODE_UDP) {
        DatagramPeerIO_Free(&peer->pio.udp.pio);
    } else {
        StreamPeerIO_Free(&peer->pio.tcp.pio);
    }
fail1:
    PacketPassInterface_Free(&peer->recv_ppi);
    return 0;
}

void peer_free_link (struct peer_data *peer)
{
    ASSERT(peer->have_link)
    ASSERT(!peer->is_relay)
    
    ASSERT(!peer->have_relaying)
    ASSERT(!peer->waiting_relay)
    
    // allow detaching DataProto flows
    DataProtoDest_PrepareFree(&peer->send_dp);
    
    // detach local flow from our DataProto
    DataProtoLocalSource_Detach(&peer->local_dpflow);
    
    // free sending
    DataProtoDest_Free(&peer->send_dp);
    
    // free transport-specific link objects
    if (options.transport_mode == TRANSPORT_MODE_UDP) {
        // free DatagramPeerIO
        DatagramPeerIO_Free(&peer->pio.udp.pio);
    } else {
        // free StreamPeerIO
        StreamPeerIO_Free(&peer->pio.tcp.pio);
    }
    
    // free common link objects
    PacketPassInterface_Free(&peer->recv_ppi);
    
    peer->have_link = 0;
}

int peer_new_link (struct peer_data *peer)
{
    if (peer->have_link) {
        if (peer->is_relay) {
            peer_disable_relay_provider(peer);
        }
        
        peer_free_link(peer);
    }
    else if (peer->have_relaying) {
        peer_uninstall_relay(peer);
    }
    else if (peer->waiting_relay) {
        peer_unregister_need_relay(peer);
    }
    
    if (!peer_init_link(peer)) {
        return 0;
    }
    
    return 1;
}

void peer_enable_relay_provider (struct peer_data *peer)
{
    ASSERT(peer->have_link)
    ASSERT(!peer->is_relay)
    
    ASSERT(!peer->have_relaying)
    ASSERT(!peer->waiting_relay)
    
    // add to relays list
    LinkedList2_Append(&relays, &peer->relay_list_node);
    
    // init users list
    LinkedList2_Init(&peer->relay_users);
    
    peer->is_relay = 1;
    
    // assign relays
    assign_relays();
}

void peer_disable_relay_provider (struct peer_data *peer)
{
    ASSERT(peer->is_relay)
    
    ASSERT(peer->have_link)
    ASSERT(!peer->have_relaying)
    ASSERT(!peer->waiting_relay)
    
    // disconnect relay users
    LinkedList2Node *list_node;
    while (list_node = LinkedList2_GetFirst(&peer->relay_users)) {
        struct peer_data *relay_user = UPPER_OBJECT(list_node, struct peer_data, relaying_list_node);
        ASSERT(relay_user->have_relaying)
        ASSERT(relay_user->relaying_peer == peer)
        
        // disconnect relay user
        peer_uninstall_relay(relay_user);
        
        // add it to need relay list
        peer_register_need_relay(relay_user);
    }
    
    // remove from relays list
    LinkedList2_Remove(&relays, &peer->relay_list_node);
    
    peer->is_relay = 0;
    
    // assign relays
    assign_relays();
}

void peer_dealloc_relay_provider (struct peer_data *peer)
{
    ASSERT(peer->is_relay)
    
    ASSERT(peer->have_link)
    ASSERT(!peer->have_relaying)
    ASSERT(!peer->waiting_relay)
    
    // allow detaching DataProto flows from the relay peer
    DataProtoDest_PrepareFree(&peer->send_dp);
    
    // disconnect relay users
    LinkedList2Node *list_node;
    while (list_node = LinkedList2_GetFirst(&peer->relay_users)) {
        struct peer_data *relay_user = UPPER_OBJECT(list_node, struct peer_data, relaying_list_node);
        ASSERT(relay_user->have_relaying)
        ASSERT(relay_user->relaying_peer == peer)
        
        // disconnect relay user
        peer_dealloc_relay(relay_user);
        
        // add it to need relay list
        peer_register_need_relay(relay_user);
    }
    
    // remove from relays list
    LinkedList2_Remove(&relays, &peer->relay_list_node);
    
    peer->is_relay = 0;
}

void peer_install_relay (struct peer_data *peer, struct peer_data *relay)
{
    ASSERT(!peer->have_relaying)
    ASSERT(!peer->have_link)
    ASSERT(!peer->waiting_relay)
    ASSERT(relay->is_relay)
    
    ASSERT(!peer->is_relay)
    ASSERT(relay->have_link)
    
    peer_log(peer, BLOG_INFO, "installing relaying through %d", (int)relay->id);
    
    // remember relaying peer
    peer->relaying_peer = relay;
    
    // add to relay's users list
    LinkedList2_Append(&relay->relay_users, &peer->relaying_list_node);
    
    // attach local flow to relay
    DataProtoLocalSource_Attach(&peer->local_dpflow, &relay->send_dp);
    
    peer->have_relaying = 1;
}

void peer_uninstall_relay (struct peer_data *peer)
{
    ASSERT(peer->have_relaying)
    
    ASSERT(!peer->have_link)
    ASSERT(!peer->waiting_relay)
    
    struct peer_data *relay = peer->relaying_peer;
    ASSERT(relay->is_relay)
    ASSERT(relay->have_link)
    
    peer_log(peer, BLOG_INFO, "uninstalling relaying through %d", (int)relay->id);
    
    // release local flow before detaching it
    DataProtoLocalSource_Release(&peer->local_dpflow);
    
    // link out relay
    peer_dealloc_relay(peer);
}

void peer_dealloc_relay (struct peer_data *peer)
{
    ASSERT(peer->have_relaying)
    
    ASSERT(!peer->have_link)
    ASSERT(!peer->waiting_relay)
    
    struct peer_data *relay = peer->relaying_peer;
    ASSERT(relay->is_relay)
    ASSERT(relay->have_link)
    
    // detach local flow from relay
    DataProtoLocalSource_Detach(&peer->local_dpflow);
    
    // remove from relay's users list
    LinkedList2_Remove(&relay->relay_users, &peer->relaying_list_node);
    
    peer->have_relaying = 0;
}

void peer_need_relay (struct peer_data *peer)
{
    ASSERT(!peer->is_relay)
    
    if (peer->have_link) {
        peer_free_link(peer);
    }
    
    if (peer->have_relaying) {
        peer_uninstall_relay(peer);
    }
    
    if (peer->waiting_relay) {
        // already waiting for relay, do nothing
        return;
    }
    
    // register the peer as needing a relay
    peer_register_need_relay(peer);
    
    // assign relays
    assign_relays();
}

void peer_register_need_relay (struct peer_data *peer)
{
    ASSERT(!peer->waiting_relay)
    ASSERT(!peer->have_link)
    ASSERT(!peer->have_relaying)
    
    ASSERT(!peer->is_relay)
    
    // add to need relay list
    LinkedList2_Append(&waiting_relay_peers, &peer->waiting_relay_list_node);
    
    peer->waiting_relay = 1;
}

void peer_unregister_need_relay (struct peer_data *peer)
{
    ASSERT(peer->waiting_relay)
    
    ASSERT(!peer->have_link)
    ASSERT(!peer->have_relaying)
    ASSERT(!peer->is_relay)
    
    // remove from need relay list
    LinkedList2_Remove(&waiting_relay_peers, &peer->waiting_relay_list_node);
    
    peer->waiting_relay = 0;
}

int peer_reset (struct peer_data *peer)
{
    peer_log(peer, BLOG_NOTICE, "resetting");
    
    if (peer_am_master(peer)) {
        // if we're the master, schedule retry
        BReactor_SetTimer(&ss, &peer->reset_timer);
    } else {
        // if we're the slave, report to master
        if (peer_send_simple(peer, MSGID_YOURETRY) < 0) {
            return -1;
        }
    }
    
    return 0;
}

void peer_add_mac_address (struct peer_data *peer, uint8_t *mac)
{
    // check if the MAC address is already present in the global table
    HashTableNode *old_table_node;
    if (HashTable_Lookup(&mac_table, mac, &old_table_node)) {
        struct mac_table_entry *old_entry = UPPER_OBJECT(old_table_node, struct mac_table_entry, table_node);
        ASSERT(!memcmp(old_entry->mac, mac, 6))
        
        // if the MAC is already associated with this peer, only move it to the end of the list
        if (old_entry->peer == peer) {
            LinkedList2_Remove(&peer->macs_used, &old_entry->list_node);
            LinkedList2_Append(&peer->macs_used, &old_entry->list_node);
            return;
        }
        
        // remove entry from global hash table
        ASSERT_EXECUTE(HashTable_Remove(&mac_table, old_entry->mac))
        
        // move entry to peer's unused list
        LinkedList2_Remove(&old_entry->peer->macs_used, &old_entry->list_node);
        LinkedList2_Append(&old_entry->peer->macs_free, &old_entry->list_node);
    }
    
    peer_log(peer, BLOG_INFO, "adding MAC %02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8"", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // aquire MAC address entry, if there are no free ones reuse the oldest used one
    LinkedList2Node *node;
    struct mac_table_entry *entry;
    if (node = LinkedList2_GetFirst(&peer->macs_free)) {
        entry = UPPER_OBJECT(node, struct mac_table_entry, list_node);
        ASSERT(entry->peer == peer)
        
        // remove from unused list
        LinkedList2_Remove(&peer->macs_free, &entry->list_node);
    } else {
        node = LinkedList2_GetFirst(&peer->macs_used);
        ASSERT(node)
        entry = UPPER_OBJECT(node, struct mac_table_entry, list_node);
        ASSERT(entry->peer == peer)
        
        // remove from used list
        LinkedList2_Remove(&peer->macs_used, &entry->list_node);
        
        // remove from global hash table
        ASSERT_EXECUTE(HashTable_Remove(&mac_table, entry->mac))
    }
    
    // copy MAC to entry
    memcpy(entry->mac, mac, 6);
    
    // add entry to used list
    LinkedList2_Append(&peer->macs_used, &entry->list_node);
    
    // add entry to global hash table
    ASSERT_EXECUTE(HashTable_Insert(&mac_table, &entry->table_node))
}

void peer_join_group (struct peer_data *peer, uint32_t group)
{
    struct peer_group_entry *group_entry;
    
    HashTableNode *old_table_node;
    if (HashTable_Lookup(&peer->groups_hashtable, &group, &old_table_node)) {
        group_entry = UPPER_OBJECT(old_table_node, struct peer_group_entry, table_node);
        
        // move to end of used list
        LinkedList2_Remove(&peer->groups_used, &group_entry->list_node);
        LinkedList2_Append(&peer->groups_used, &group_entry->list_node);
    } else {
        peer_log(peer, BLOG_INFO, "joined group %"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8"",
            ((uint8_t *)&group)[0], ((uint8_t *)&group)[1], ((uint8_t *)&group)[2], ((uint8_t *)&group)[3]
        );
        
        // aquire group entry, if there are no free ones reuse the earliest used one
        LinkedList2Node *node;
        if (node = LinkedList2_GetFirst(&peer->groups_free)) {
            group_entry = UPPER_OBJECT(node, struct peer_group_entry, list_node);
            
            // remove from free list
            LinkedList2_Remove(&peer->groups_free, &group_entry->list_node);
        } else {
            node = LinkedList2_GetFirst(&peer->groups_used);
            ASSERT(node)
            group_entry = UPPER_OBJECT(node, struct peer_group_entry, list_node);
            
            // remove from used list
            LinkedList2_Remove(&peer->groups_used, &group_entry->list_node);
            
            // remove from groups hash table
            ASSERT_EXECUTE(HashTable_Remove(&peer->groups_hashtable, &group_entry->group))
            
            // remove from global multicast table
            multicast_table_remove_entry(group_entry);
        }
        
        // add entry to used list
        LinkedList2_Append(&peer->groups_used, &group_entry->list_node);
        
        // set group address in entry
        group_entry->group = group;
        
        // add entry to groups hash table
        ASSERT_EXECUTE(HashTable_Insert(&peer->groups_hashtable, &group_entry->table_node))
        
        // add entry to global multicast table
        multicast_table_add_entry(group_entry);
    }
    
    // start timer
    group_entry->timer_endtime = btime_gettime() + IGMP_DEFAULT_GROUP_MEMBERSHIP_INTERVAL;
    BReactor_SetTimerAbsolute(&ss, &group_entry->timer, group_entry->timer_endtime);
}

void peer_leave_group (struct peer_data *peer, uint32_t group)
{
    HashTableNode *table_node;
    if (!HashTable_Lookup(&peer->groups_hashtable, &group, &table_node)) {
        return;
    }
    struct peer_group_entry *group_entry = UPPER_OBJECT(table_node, struct peer_group_entry, table_node);
    
    peer_log(peer, BLOG_INFO, "left group %"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8"",
        ((uint8_t *)&group)[0], ((uint8_t *)&group)[1], ((uint8_t *)&group)[2], ((uint8_t *)&group)[3]
    );
    
    // move to free list
    LinkedList2_Remove(&peer->groups_used, &group_entry->list_node);
    LinkedList2_Append(&peer->groups_free, &group_entry->list_node);
    
    // stop timer
    BReactor_RemoveTimer(&ss, &group_entry->timer);
    
    // remove from groups hash table
    ASSERT_EXECUTE(HashTable_Remove(&peer->groups_hashtable, &group_entry->group))
    
    // remove from global multicast table
    multicast_table_remove_entry(group_entry);
}

void peer_msg (struct peer_data *peer, uint8_t *data, int data_len)
{
    ASSERT(server_ready)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= SC_MAX_MSGLEN)
    
    // parse message
    msgParser parser;
    if (!msgParser_Init(&parser, data, data_len)) {
        peer_log(peer, BLOG_NOTICE, "msg: failed to parse");
        return;
    }
    
    // read message
    uint16_t type;
    ASSERT_EXECUTE(msgParser_Gettype(&parser, &type))
    uint8_t *payload;
    int payload_len;
    ASSERT_EXECUTE(msgParser_Getpayload(&parser, &payload, &payload_len))
    
    // dispatch according to message type
    switch (type) {
        case MSGID_YOUCONNECT:
            peer_msg_youconnect(peer, payload, payload_len);
            return;
        case MSGID_CANNOTCONNECT:
            peer_msg_cannotconnect(peer, payload, payload_len);
            return;
        case MSGID_CANNOTBIND:
            peer_msg_cannotbind(peer, payload, payload_len);
            return;
        case MSGID_YOURETRY:
            peer_msg_youretry(peer, payload, payload_len);
            return;
        case MSGID_SEED:
            peer_msg_seed(peer, payload, payload_len);
            return;
        case MSGID_CONFIRMSEED:
            peer_msg_confirmseed(peer, payload, payload_len);
            return;
        default:
            BLog(BLOG_NOTICE, "msg: unknown type");
            return;
    }
}

void peer_msg_youconnect (struct peer_data *peer, uint8_t *data, int data_len)
{
    // init parser
    msg_youconnectParser parser;
    if (!msg_youconnectParser_Init(&parser, data, data_len)) {
        peer_log(peer, BLOG_WARNING, "msg_youconnect: failed to parse");
        return;
    }
    
    // try addresses
    BAddr addr;
    while (1) {
        // get address message
        uint8_t *addrmsg_data;
        int addrmsg_len;
        if (!msg_youconnectParser_Getaddr(&parser, &addrmsg_data, &addrmsg_len)) {
            peer_log(peer, BLOG_NOTICE, "msg_youconnect: no usable addresses");
            peer_send_simple(peer, MSGID_CANNOTCONNECT);
            return;
        }
        
        // parse address message
        msg_youconnect_addrParser aparser;
        if (!msg_youconnect_addrParser_Init(&aparser, addrmsg_data, addrmsg_len)) {
            peer_log(peer, BLOG_WARNING, "msg_youconnect: failed to parse address message");
            return;
        }
        
        // check if the address scope is known
        uint8_t *name_data;
        int name_len;
        ASSERT_EXECUTE(msg_youconnect_addrParser_Getname(&aparser, &name_data, &name_len))
        char *name;
        if (!(name = address_scope_known(name_data, name_len))) {
            continue;
        }
        
        // read address
        uint8_t *addr_data;
        int addr_len;
        ASSERT_EXECUTE(msg_youconnect_addrParser_Getaddr(&aparser, &addr_data, &addr_len))
        if (!addr_read(addr_data, addr_len, &addr)) {
            peer_log(peer, BLOG_WARNING, "msg_youconnect: failed to read address");
            return;
        }
        
        peer_log(peer, BLOG_NOTICE, "msg_youconnect: using address in scope '%s'", name);
        break;
    }
    
    // discard further addresses
    msg_youconnectParser_Forwardaddr(&parser);
    
    uint8_t *key = NULL;
    uint64_t password = 0;
    
    // read additonal parameters
    if (options.transport_mode == TRANSPORT_MODE_UDP) {
        if (SPPROTO_HAVE_ENCRYPTION(sp_params)) {
            int key_len;
            if (!msg_youconnectParser_Getkey(&parser, &key, &key_len)) {
                peer_log(peer, BLOG_WARNING, "msg_youconnect: no key");
                return;
            }
            if (key_len != BEncryption_cipher_key_size(sp_params.encryption_mode)) {
                peer_log(peer, BLOG_WARNING, "msg_youconnect: wrong key size");
                return;
            }
        }
    } else {
        if (!msg_youconnectParser_Getpassword(&parser, &password)) {
            peer_log(peer, BLOG_WARNING, "msg_youconnect: no password");
            return;
        }
    }
    
    if (!msg_youconnectParser_GotEverything(&parser)) {
        peer_log(peer, BLOG_WARNING, "msg_youconnect: stray data");
        return;
    }
    
    peer_log(peer, BLOG_INFO, "connecting");
    
    peer_connect(peer, addr, key, password);
    return;
}

void peer_msg_cannotconnect (struct peer_data *peer, uint8_t *data, int data_len)
{
    if (data_len != 0) {
        peer_log(peer, BLOG_WARNING, "msg_cannotconnect: invalid length");
        return;
    }
    
    if (!peer->binding) {
        peer_log(peer, BLOG_WARNING, "msg_cannotconnect: not binding");
        return;
    }
    
    peer_log(peer, BLOG_INFO, "peer could not connect");
    
    // continue trying bind addresses
    peer_bind(peer);
    return;
}

void peer_msg_cannotbind (struct peer_data *peer, uint8_t *data, int data_len)
{
    if (data_len != 0) {
        peer_log(peer, BLOG_WARNING, "msg_cannotbind: invalid length");
        return;
    }
    
    peer_log(peer, BLOG_INFO, "peer cannot bind");
    
    if (!peer_am_master(peer)) {
        peer_start_binding(peer);
        return;
    } else {
        if (!peer->is_relay) {
            peer_need_relay(peer);
        }
    }
}

void peer_msg_seed (struct peer_data *peer, uint8_t *data, int data_len)
{
    msg_seedParser parser;
    if (!msg_seedParser_Init(&parser, data, data_len)) {
        peer_log(peer, BLOG_WARNING, "msg_seed: failed to parse");
        return;
    }
    
    // read message
    uint16_t seed_id;
    ASSERT_EXECUTE(msg_seedParser_Getseed_id(&parser, &seed_id))
    uint8_t *key;
    int key_len;
    ASSERT_EXECUTE(msg_seedParser_Getkey(&parser, &key, &key_len))
    uint8_t *iv;
    int iv_len;
    ASSERT_EXECUTE(msg_seedParser_Getiv(&parser, &iv, &iv_len))
    
    if (options.transport_mode != TRANSPORT_MODE_UDP) {
        peer_log(peer, BLOG_WARNING, "msg_seed: not in UDP mode");
        return;
    }
    
    if (!SPPROTO_HAVE_OTP(sp_params)) {
        peer_log(peer, BLOG_WARNING, "msg_seed: OTPs disabled");
        return;
    }
    
    if (key_len != BEncryption_cipher_key_size(sp_params.otp_mode)) {
        peer_log(peer, BLOG_WARNING, "msg_seed: wrong key length");
        return;
    }
    
    if (iv_len != BEncryption_cipher_block_size(sp_params.otp_mode)) {
        peer_log(peer, BLOG_WARNING, "msg_seed: wrong IV length");
        return;
    }
    
    if (!peer->have_link) {
        peer_log(peer, BLOG_WARNING, "msg_seed: have no link");
        return;
    }
    
    peer_log(peer, BLOG_DEBUG, "received OTP receive seed");
    
    // add receive seed
    DatagramPeerIO_AddOTPRecvSeed(&peer->pio.udp.pio, seed_id, key, iv);
    
    // send confirmation
    peer_send_confirmseed(peer, seed_id);
    return;
}

void peer_msg_confirmseed (struct peer_data *peer, uint8_t *data, int data_len)
{
    msg_confirmseedParser parser;
    if (!msg_confirmseedParser_Init(&parser, data, data_len)) {
        peer_log(peer, BLOG_WARNING, "msg_confirmseed: failed to parse");
        return;
    }
    
    // read message
    uint16_t seed_id;
    ASSERT_EXECUTE(msg_confirmseedParser_Getseed_id(&parser, &seed_id))
    
    if (options.transport_mode != TRANSPORT_MODE_UDP) {
        peer_log(peer, BLOG_WARNING, "msg_confirmseed: not in UDP mode");
        return;
    }
    
    if (!SPPROTO_HAVE_OTP(sp_params)) {
        peer_log(peer, BLOG_WARNING, "msg_confirmseed: OTPs disabled");
        return;
    }
    
    if (!peer->have_link) {
        peer_log(peer, BLOG_WARNING, "msg_confirmseed: have no link");
        return;
    }
    
    if (!peer->pio.udp.sendseed_sent) {
        peer_log(peer, BLOG_WARNING, "msg_confirmseed: no seed has been sent");
        return;
    }
    
    if (seed_id != peer->pio.udp.sendseed_sent_id) {
        peer_log(peer, BLOG_WARNING, "msg_confirmseed: invalid seed: expecting %d, received %d", (int)peer->pio.udp.sendseed_sent_id, (int)seed_id);
        return;
    }
    
    peer_log(peer, BLOG_DEBUG, "OTP send seed confirmed");
    
    // no longer waiting for confirmation
    peer->pio.udp.sendseed_sent = 0;
    
    // start using the seed
    DatagramPeerIO_SetOTPSendSeed(&peer->pio.udp.pio, peer->pio.udp.sendseed_sent_id, peer->pio.udp.sendseed_sent_key, peer->pio.udp.sendseed_sent_iv);
}

void peer_msg_youretry (struct peer_data *peer, uint8_t *data, int data_len)
{
    if (data_len != 0) {
        peer_log(peer, BLOG_WARNING, "msg_youretry: invalid length");
        return;
    }
    
    if (!peer_am_master(peer)) {
        peer_log(peer, BLOG_WARNING, "msg_youretry: we are not master");
        return;
    }
    
    peer_log(peer, BLOG_NOTICE, "requests reset");
    
    peer_reset(peer);
    return;
}

void peer_udp_pio_handler_seed_warning (struct peer_data *peer)
{
    ASSERT(options.transport_mode == TRANSPORT_MODE_UDP)
    ASSERT(SPPROTO_HAVE_OTP(sp_params))
    ASSERT(peer->have_link)
    
    // generate and send a new seed
    if (!peer->pio.udp.sendseed_sent) {
        peer_udp_send_seed(peer);
        return;
    }
}

void peer_tcp_pio_handler_error (struct peer_data *peer)
{
    ASSERT(options.transport_mode == TRANSPORT_MODE_TCP)
    ASSERT(peer->have_link)
    
    peer_log(peer, BLOG_NOTICE, "TCP connection failed");
    
    peer_reset(peer);
    return;
}

void peer_reset_timer_handler (struct peer_data *peer)
{
    ASSERT(peer_am_master(peer))
    
    BLog(BLOG_NOTICE, "retry timer expired");
    
    // start setup process
    peer_start_binding(peer);
    return;
}

void peer_recv_handler_send (struct peer_data *peer, uint8_t *data, int data_len)
{
    ASSERT(peer->have_link)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= data_mtu)
    
    uint8_t *orig_data = data;
    int orig_data_len = data_len;
    
    int dp_good = 0;
    struct peer_data *relay = NULL;
    int local = 0;
    
    // check dataproto header
    if (data_len < sizeof(struct dataproto_header)) {
        peer_log(peer, BLOG_NOTICE, "receive: no dataproto header");
        goto out;
    }
    struct dataproto_header *header = (struct dataproto_header *)data;
    data += sizeof(struct dataproto_header);
    data_len -= sizeof(struct dataproto_header);
    uint8_t flags = header->flags;
    peerid_t from_id = ltoh16(header->from_id);
    int num_ids = ltoh16(header->num_peer_ids);
    
    // check destination IDs
    if (num_ids > 1) {
        peer_log(peer, BLOG_NOTICE, "receive: too many destination IDs");
        goto out;
    }
    if (data_len < num_ids * sizeof(struct dataproto_peer_id)) {
        peer_log(peer, BLOG_NOTICE, "receive: invalid length for destination IDs");
        goto out;
    }
    struct dataproto_peer_id *ids = (struct dataproto_peer_id *)data;
    data += num_ids * sizeof(struct dataproto_peer_id);
    data_len -= num_ids * sizeof(struct dataproto_peer_id);
    
    // check remaining data
    if (data_len > device.mtu) {
        peer_log(peer, BLOG_NOTICE, "receive: frame too large");
        goto out;
    }
    
    dp_good = 1;
    
    if (num_ids == 0) {
        goto out;
    }
    
    // find source peer
    struct peer_data *src_peer = find_peer_by_id(from_id);
    if (!src_peer) {
        peer_log(peer, BLOG_NOTICE, "receive: source peer %d not known", (int)from_id);
        goto out;
    }
    
    // find destination
    peerid_t id = ltoh16(ids[0].id);
    if (id == my_id) {
        // frame is for us
        
        // check ethernet header
        if (data_len < sizeof(struct ethernet_header)) {
            peer_log(peer, BLOG_INFO, "received frame without ethernet header");
            goto out;
        }
        struct ethernet_header *header = (struct ethernet_header *)data;
        
        // associate source address with peer
        peer_add_mac_address(peer, header->source);
        
        // invoke incoming hook
        peer_hook_incoming(peer, data, data_len);
        
        local = 1;
    } else {
        // frame is for someone else
        
        // make sure the client is allowed to relay though us
        if (!(peer->flags & SCID_NEWCLIENT_FLAG_RELAY_CLIENT)) {
            peer_log(peer, BLOG_NOTICE, "relaying not allowed");
            goto out;
        }
        
        // lookup destination peer
        struct peer_data *dest_peer = find_peer_by_id(id);
        if (!dest_peer) {
            peer_log(peer, BLOG_NOTICE, "relay destination peer not known");
            goto out;
        }
        
        // check if the destination peer has link
        if (!dest_peer->have_link) {
            peer_log(peer, BLOG_NOTICE, "relay destination peer has no link");
            goto out;
        }
        
        relay = dest_peer;
    }
    
out:
    // accept packet
    if (!local) {
        PacketPassInterface_Done(&peer->recv_ppi);
    }
    
    // relay frame
    if (relay) {
        peer_submit_relayed_frame(relay, src_peer, data, data_len);
    }
    
    // submit to device
    if (local) {
        PacketPassInterface_Sender_Send(peer->local_recv_if, data, data_len);
    }
    
    // inform DataProto of received packet
    if (dp_good) {
        DataProtoDest_Received(&peer->send_dp, !!(flags & DATAPROTO_FLAGS_RECEIVING_KEEPALIVES));
    }
}

void local_recv_qflow_output_handler_done (struct peer_data *peer)
{
    PacketPassInterface_Done(&peer->recv_ppi);
}

int peer_start_binding (struct peer_data *peer)
{
    peer->binding = 1;
    peer->binding_addrpos = 0;
    
    return peer_bind(peer);
}

int peer_bind (struct peer_data *peer)
{
    ASSERT(peer->binding)
    ASSERT(peer->binding_addrpos >= 0)
    ASSERT(peer->binding_addrpos <= num_bind_addrs)
    
    int res;
    
    while (peer->binding_addrpos < num_bind_addrs) {
        // if there are no external addresses, skip bind address
        if (bind_addrs[peer->binding_addrpos].num_ext_addrs == 0) {
            peer->binding_addrpos++;
            continue;
        }
        
        // try to bind
        int cont;
        if (peer_bind_one_address(peer, peer->binding_addrpos, &cont) < 0) {
            return -1;
        }
        
        // increment address counter
        peer->binding_addrpos++;
        
        if (!cont) {
            return 0;
        }
    }
    
    peer_log(peer, BLOG_NOTICE, "no more addresses to bind to");
    
    // no longer binding
    peer->binding = 0;
    
    // tell the peer we failed to bind
    if (peer_send_simple(peer, MSGID_CANNOTBIND) < 0) {
        return -1;
    }
    
    // if we are the slave, setup relaying
    if (!peer_am_master(peer)) {
        if (!peer->is_relay) {
            peer_need_relay(peer);
        }
    }
    
    return 0;
}

int peer_bind_one_address (struct peer_data *peer, int addr_index, int *cont)
{
    ASSERT(addr_index >= 0)
    ASSERT(addr_index < num_bind_addrs)
    ASSERT(bind_addrs[addr_index].num_ext_addrs > 0)
    
    // get a fresh link
    if (!peer_new_link(peer)) {
        peer_log(peer, BLOG_ERROR, "cannot get link");
        *cont = 0;
        return peer_reset(peer);
    }
    
    if (options.transport_mode == TRANSPORT_MODE_UDP) {
        // get addr
        POINTER(addr, bind_addrs[addr_index]);
        
        // try binding to all ports in the range
        int port_add;
        for (port_add = 0; port_add < addr->num_ports; port_add++) {
            BAddr tryaddr = addr->addr;
            BAddr_SetPort(&tryaddr, hton16(ntoh16(BAddr_GetPort(&tryaddr)) + port_add));
            if (DatagramPeerIO_Bind(&peer->pio.udp.pio, tryaddr)) {
                break;
            }
        }
        if (port_add == addr->num_ports) {
            BLog(BLOG_NOTICE, "failed to bind to any port");
            *cont = 1;
            return 0;
        }
        
        uint8_t key[SPPROTO_HAVE_ENCRYPTION(sp_params) ? BEncryption_cipher_key_size(sp_params.encryption_mode) : 0];
        
        // generate and set encryption key
        if (SPPROTO_HAVE_ENCRYPTION(sp_params)) {
            BRandom_randomize(key, sizeof(key));
            DatagramPeerIO_SetEncryptionKey(&peer->pio.udp.pio, key);
        }
        
        // schedule sending OTP seed
        if (SPPROTO_HAVE_OTP(sp_params)) {
            BPending_Set(&peer->job_send_seed_after_binding);
        }
        
        // send connectinfo
        if (peer_send_conectinfo(peer, addr_index, port_add, key, 0) < 0) {
            return -1;
        }
    } else {
        // order StreamPeerIO to listen
        uint64_t pass;
        StreamPeerIO_Listen(&peer->pio.tcp.pio, &listeners[addr_index], &pass);
        
        // send connectinfo
        if (peer_send_conectinfo(peer, addr_index, 0, NULL, pass) < 0) {
            return -1;
        }
    }
    
    peer_log(peer, BLOG_NOTICE, "bound to address number %d", addr_index);
    
    *cont = 0;
    return 0;
}

int peer_connect (struct peer_data *peer, BAddr addr, uint8_t *encryption_key, uint64_t password)
{
    ASSERT(!BAddr_IsInvalid(&addr))
    
    // get a fresh link
    if (!peer_new_link(peer)) {
        peer_log(peer, BLOG_ERROR, "cannot get link");
        return peer_reset(peer);
    }
    
    if (options.transport_mode == TRANSPORT_MODE_UDP) {
        // order DatagramPeerIO to connect
        if (!DatagramPeerIO_Connect(&peer->pio.udp.pio, addr)) {
            peer_log(peer, BLOG_NOTICE, "DatagramPeerIO_Connect failed");
            return peer_reset(peer);
        }
        
        // set encryption key
        if (SPPROTO_HAVE_ENCRYPTION(sp_params)) {
            DatagramPeerIO_SetEncryptionKey(&peer->pio.udp.pio, encryption_key);
        }
        
        // generate and send a send seed
        if (SPPROTO_HAVE_OTP(sp_params)) {
            if (peer_udp_send_seed(peer) < 0) {
                return -1;
            }
        }
    } else {
        // order StreamPeerIO to connect
        if (!StreamPeerIO_Connect(
            &peer->pio.tcp.pio, addr, password,
            (options.peer_ssl ? client_cert : NULL),
            (options.peer_ssl ? client_key : NULL)
        )) {
            peer_log(peer, BLOG_NOTICE, "StreamPeerIO_Connect failed");
            return peer_reset(peer);
        }
    }
    
    return 0;
}

int peer_send_simple (struct peer_data *peer, int msgid)
{
    if (server_start_msg(NULL, peer->id, msgid, 0) < 0) {
        return -1;
    }
    server_end_msg();
    
    return 0;
}

int peer_send_conectinfo (struct peer_data *peer, int addr_index, int port_adjust, uint8_t *enckey, uint64_t pass)
{
    ASSERT(addr_index >= 0)
    ASSERT(addr_index < num_bind_addrs)
    ASSERT(bind_addrs[addr_index].num_ext_addrs > 0)
    
    // get address
    POINTER(bind_addr, bind_addrs[addr_index]);
    
    // remember encryption key size
    int key_size;
    if (options.transport_mode == TRANSPORT_MODE_UDP && SPPROTO_HAVE_ENCRYPTION(sp_params)) {
        key_size = BEncryption_cipher_key_size(sp_params.encryption_mode);
    }
    
    // calculate message length ..
    int msg_len = 0;
    
    // addresses
    for (int i = 0; i < bind_addr->num_ext_addrs; i++) {
        int addrmsg_len =
            msg_youconnect_addr_SIZEname(strlen(bind_addr->ext_addrs[i].scope)) +
            msg_youconnect_addr_SIZEaddr(addr_size(bind_addr->ext_addrs[i].addr));
        msg_len += msg_youconnect_SIZEaddr(addrmsg_len);
    }
    
    // encryption key
    if (options.transport_mode == TRANSPORT_MODE_UDP && SPPROTO_HAVE_ENCRYPTION(sp_params)) {
        msg_len += msg_youconnect_SIZEkey(key_size);
    }
    
    // password
    if (options.transport_mode == TRANSPORT_MODE_TCP) {
        msg_len += msg_youconnect_SIZEpassword;
    }
    
    // check if it's too big (because of the addresses)
    if (msg_len > MSG_MAX_PAYLOAD) {
        BLog(BLOG_ERROR, "cannot send too big youconnect message");
        return 0;
    }
        
    // start message
    uint8_t *msg;
    if (server_start_msg((void **)&msg, peer->id, MSGID_YOUCONNECT, msg_len) < 0) {
        return -1;
    }
        
    // init writer
    msg_youconnectWriter writer;
    msg_youconnectWriter_Init(&writer, msg);
        
    // write addresses
    for (int i = 0; i < bind_addr->num_ext_addrs; i++) {
        int name_len = strlen(bind_addr->ext_addrs[i].scope);
        int addr_len = addr_size(bind_addr->ext_addrs[i].addr);
        
        // get a pointer for writing the address
        int addrmsg_len =
            msg_youconnect_addr_SIZEname(name_len) +
            msg_youconnect_addr_SIZEaddr(addr_len);
        uint8_t *addrmsg_dst = msg_youconnectWriter_Addaddr(&writer, addrmsg_len);
        
        // init address writer
        msg_youconnect_addrWriter awriter;
        msg_youconnect_addrWriter_Init(&awriter, addrmsg_dst);
        
        // write scope
        uint8_t *name_dst = msg_youconnect_addrWriter_Addname(&awriter, name_len);
        memcpy(name_dst, bind_addr->ext_addrs[i].scope, name_len);
        
        // write address with adjusted port
        BAddr addr = bind_addr->ext_addrs[i].addr;
        BAddr_SetPort(&addr, hton16(ntoh16(BAddr_GetPort(&addr)) + port_adjust));
        uint8_t *addr_dst = msg_youconnect_addrWriter_Addaddr(&awriter, addr_len);
        addr_write(addr_dst, addr);
        
        // finish address writer
        msg_youconnect_addrWriter_Finish(&awriter);
    }
    
    // write encryption key
    if (options.transport_mode == TRANSPORT_MODE_UDP && SPPROTO_HAVE_ENCRYPTION(sp_params)) {
        uint8_t *key_dst = msg_youconnectWriter_Addkey(&writer, key_size);
        memcpy(key_dst, enckey, key_size);
    }
    
    // write password
    if (options.transport_mode == TRANSPORT_MODE_TCP) {
        msg_youconnectWriter_Addpassword(&writer, pass);
    }
    
    // finish writer
    msg_youconnectWriter_Finish(&writer);
    
    // end message
    server_end_msg();
    
    return 0;
}

int peer_udp_send_seed (struct peer_data *peer)
{
    ASSERT(options.transport_mode == TRANSPORT_MODE_UDP)
    ASSERT(SPPROTO_HAVE_OTP(sp_params))
    ASSERT(peer->have_link)
    ASSERT(!peer->pio.udp.sendseed_sent)
    
    peer_log(peer, BLOG_DEBUG, "sending OTP send seed");
    
    int key_len = BEncryption_cipher_key_size(sp_params.otp_mode);
    int iv_len = BEncryption_cipher_block_size(sp_params.otp_mode);
    
    // generate seed
    peer->pio.udp.sendseed_sent_id = peer->pio.udp.sendseed_nextid;
    BRandom_randomize(peer->pio.udp.sendseed_sent_key, key_len);
    BRandom_randomize(peer->pio.udp.sendseed_sent_iv, iv_len);
    
    // set as sent, increment next seed ID
    peer->pio.udp.sendseed_sent = 1;
    peer->pio.udp.sendseed_nextid++;
    
    // send seed to the peer
    int msg_len = msg_seed_SIZEseed_id + msg_seed_SIZEkey(key_len) + msg_seed_SIZEiv(iv_len);
    uint8_t *msg;
    if (server_start_msg((void **)&msg, peer->id, MSGID_SEED, msg_len) < 0) {
        return -1;
    }
    msg_seedWriter writer;
    msg_seedWriter_Init(&writer, msg);
    msg_seedWriter_Addseed_id(&writer, peer->pio.udp.sendseed_sent_id);
    uint8_t *key_dst = msg_seedWriter_Addkey(&writer, key_len);
    memcpy(key_dst, peer->pio.udp.sendseed_sent_key, key_len);
    uint8_t *iv_dst = msg_seedWriter_Addiv(&writer, iv_len);
    memcpy(iv_dst, peer->pio.udp.sendseed_sent_iv, iv_len);
    msg_seedWriter_Finish(&writer);
    server_end_msg();
    
    return 0;
}

int peer_send_confirmseed (struct peer_data *peer, uint16_t seed_id)
{
    ASSERT(options.transport_mode == TRANSPORT_MODE_UDP)
    ASSERT(SPPROTO_HAVE_OTP(sp_params))
    
    // send confirmation
    int msg_len = msg_confirmseed_SIZEseed_id;
    uint8_t *msg;
    if (server_start_msg((void **)&msg, peer->id, MSGID_CONFIRMSEED, msg_len) < 0) {
        return -1;
    }
    msg_confirmseedWriter writer;
    msg_confirmseedWriter_Init(&writer, msg);
    msg_confirmseedWriter_Addseed_id(&writer, seed_id);
    msg_confirmseedWriter_Finish(&writer);
    server_end_msg();
    
    return 0;
}

void peer_submit_relayed_frame (struct peer_data *peer, struct peer_data *source_peer, uint8_t *frame, int frame_len)
{
    ASSERT(peer->have_link)
    ASSERT(frame_len >= 0)
    ASSERT(frame_len <= device.mtu)
    
    DataProtoDest_SubmitRelayFrame(&peer->send_dp, &source_peer->relay_source, frame, frame_len, options.send_buffer_relay_size);
}

void peer_group_timer_handler (struct peer_group_entry *entry)
{
    struct peer_data *peer = entry->peer;
    
    peer_leave_group(peer, entry->group);
}

void peer_dataproto_handler (struct peer_data *peer, int up)
{
    ASSERT(peer->have_link)
    
    // peer_recv_handler_send relies on this not bringing everything down
    
    if (up) {
        peer_log(peer, BLOG_INFO, "up");
        
        // if it can be a relay provided, enable it
        if ((peer->flags&SCID_NEWCLIENT_FLAG_RELAY_SERVER) && !peer->is_relay) {
            peer_enable_relay_provider(peer);
        }
    } else {
        peer_log(peer, BLOG_INFO, "down");
        
        // if it is a relay provider, disable it
        if (peer->is_relay) {
            peer_disable_relay_provider(peer);
        }
    }
}

struct peer_data * find_peer_by_id (peerid_t id)
{
    HashTableNode *node;
    if (!HashTable_Lookup(&peers_by_id, &id, &node)) {
        return NULL;
    }
    struct peer_data *peer = UPPER_OBJECT(node, struct peer_data, table_node);
    
    return peer;
}

void multicast_table_add_entry (struct peer_group_entry *group_entry)
{
    // key is 23 network byte order least-significant bits of group address
    uint32_t sig = hton32(ntoh32(group_entry->group)&0x7FFFFF);
    
    // lookup entry in multicast table
    struct multicast_table_entry *multicast_entry;
    HashTableNode *table_node;
    if (HashTable_Lookup(&multicast_table, &sig, &table_node)) {
        multicast_entry = UPPER_OBJECT(table_node, struct multicast_table_entry, table_node);
    } else {
        // grab entry from free multicast entries list
        LinkedList2Node *free_list_node = LinkedList2_GetFirst(&multicast_entries_free);
        ASSERT(free_list_node) // there are as many multicast entries as maximum number of groups
        multicast_entry = UPPER_OBJECT(free_list_node, struct multicast_table_entry, free_list_node);
        LinkedList2_Remove(&multicast_entries_free, &multicast_entry->free_list_node);
        
        // set key
        multicast_entry->sig = sig;
        
        // insert into hash table
        ASSERT_EXECUTE(HashTable_Insert(&multicast_table, &multicast_entry->table_node))
        
        // init list of group entries
        LinkedList2_Init(&multicast_entry->group_entries);
    }
    
    // add to list of group entries
    LinkedList2_Append(&multicast_entry->group_entries, &group_entry->multicast_list_node);
    
    // write multicast entry pointer to group entry for fast removal of groups
    group_entry->multicast_entry = multicast_entry;
}

void multicast_table_remove_entry (struct peer_group_entry *group_entry)
{
    struct multicast_table_entry *multicast_entry = group_entry->multicast_entry;
    
    // remove group entry from linked list in multicast entry
    LinkedList2_Remove(&multicast_entry->group_entries, &group_entry->multicast_list_node);
    
    // if the multicast entry has no more group entries, remove it from the hash table
    if (LinkedList2_IsEmpty(&multicast_entry->group_entries)) {
        // remove from multicast table
        ASSERT_EXECUTE(HashTable_Remove(&multicast_table, &multicast_entry->sig))
        
        // add to free list
        LinkedList2_Append(&multicast_entries_free, &multicast_entry->free_list_node);
    }
}

int peer_groups_table_key_comparator (uint32_t *group1, uint32_t *group2)
{
    return (*group1 == *group2);
}

int peer_groups_table_hash_function (uint32_t *group, int modulo)
{
    return (jenkins_lookup2_hash((uint8_t *)group, sizeof(*group), 0) % modulo);
}

int mac_table_key_comparator (uint8_t *mac1, uint8_t *mac2)
{
    return (memcmp(mac1, mac2, 6) == 0);
}

int mac_table_hash_function (uint8_t *mac, int modulo)
{
    return (jenkins_lookup2_hash(mac, 6, mac_table_initval) % modulo);
}

int multicast_table_key_comparator (uint32_t *sig1, uint32_t *sig2)
{
    return (*sig1 == *sig2);
}

int multicast_table_hash_function (uint32_t *sig, int modulo)
{
    return (jenkins_lookup2_hash((uint8_t *)sig, sizeof(*sig), multicast_table_initval) % modulo);
}

int peers_by_id_key_comparator (peerid_t *id1, peerid_t *id2)
{
    return (*id1 == *id2);
}

int peers_by_id_hash_function (peerid_t *id, int modulo)
{
    return (jenkins_lookup2_hash((uint8_t *)id, sizeof(*id), peers_by_id_initval) % modulo);
}

void device_error_handler (void *unused)
{
    BLog(BLOG_ERROR, "device error");
    
    terminate();
    return;
}

void device_input_handler_send (void *unused, uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= device.mtu)
    
    // accept packet
    PacketPassInterface_Done(&device.input_interface);
    
    const uint8_t broadcast_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    const uint8_t multicast_header[] = {0x01, 0x00, 0x5e};
    
    if (data_len < sizeof(struct ethernet_header)) {
        BLog(BLOG_INFO, "device: frame too small (%d)", data_len);
        return;
    }
    
    struct ethernet_header *header = (struct ethernet_header *)data;
    
    // invoke outgoing hook
    int hook_result = hook_outgoing(data, data_len);
    
    switch (hook_result) {
        case HOOK_OUT_DEFAULT:
            // is it multicast?
            if (!memcmp(header->dest, multicast_header, 3)) {
                // obtain multicast group bits from MAC address
                uint32_t sig;
                memcpy(&sig, &header->dest[2], 4);
                sig = hton32(ntoh32(sig)&0x7FFFFF);
                // lookup multicast entry
                HashTableNode *multicast_table_node;
                if (HashTable_Lookup(&multicast_table, &sig, &multicast_table_node)) {
                    struct multicast_table_entry *multicast_entry = UPPER_OBJECT(multicast_table_node, struct multicast_table_entry, table_node);
                    // send to all peers with groups matching the known bits of the group address
                    LinkedList2Iterator it;
                    LinkedList2Iterator_InitForward(&it, &multicast_entry->group_entries);
                    LinkedList2Node *group_entries_list_node;
                    while (group_entries_list_node = LinkedList2Iterator_Next(&it)) {
                        struct peer_group_entry *group_entry = UPPER_OBJECT(group_entries_list_node, struct peer_group_entry, multicast_list_node);
                        submit_frame_to_peer(group_entry->peer, data, data_len);
                    }
                }
            } else {
                // should we flood it?
                HashTableNode *mac_table_node;
                if (!memcmp(header->dest, broadcast_mac, 6) || !HashTable_Lookup(&mac_table, header->dest, &mac_table_node)) {
                    flood_frame(data, data_len);
                }
                // unicast it
                else {
                    struct mac_table_entry *mac_entry = UPPER_OBJECT(mac_table_node, struct mac_table_entry, table_node);
                    submit_frame_to_peer(mac_entry->peer, data, data_len);
                }
            }
            break;
        case HOOK_OUT_FLOOD:
            flood_frame(data, data_len);
            break;
        default:
            ASSERT(0);
    }
}

void submit_frame_to_peer (struct peer_data *peer, uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= device.mtu)
    
    DataProtoLocalSource_SubmitFrame(&peer->local_dpflow, data, data_len);
}

void flood_frame (uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= device.mtu)
    
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &peers);
    LinkedList2Node *peer_list_node;
    while (peer_list_node = LinkedList2Iterator_Next(&it)) {
        struct peer_data *peer = UPPER_OBJECT(peer_list_node, struct peer_data, list_node);
        submit_frame_to_peer(peer, data, data_len);
    }
}

int hook_outgoing (uint8_t *pos, int len)
{
    ASSERT(len >= sizeof(struct ethernet_header))
    
    struct ethernet_header *eth_header = (struct ethernet_header *)pos;
    pos += sizeof(struct ethernet_header);
    len -= sizeof(struct ethernet_header);
    
    switch (ntoh16(eth_header->type)) {
        case ETHERTYPE_IPV4: {
            struct ipv4_header *ipv4_header;
            if (!check_ipv4_packet(pos, len, &ipv4_header, &pos, &len)) {
                BLog(BLOG_INFO, "hook outgoing: wrong IP packet");
                goto out;
            }
            if (ipv4_header->protocol != IPV4_PROTOCOL_IGMP) {
                goto out;
            }
            if (len < sizeof(struct igmp_base)) {
                BLog(BLOG_INFO, "hook outgoing: IGMP: short packet");
                goto out;
            }
            struct igmp_base *igmp_base = (struct igmp_base *)pos;
            pos += sizeof(struct igmp_base);
            len -= sizeof(struct igmp_base);
            switch (igmp_base->type) {
                case IGMP_TYPE_MEMBERSHIP_QUERY: {
                    if (len == sizeof(struct igmp_v2_extra) && igmp_base->max_resp_code != 0) {
                        // V2 query
                        struct igmp_v2_extra *query = (struct igmp_v2_extra *)pos;
                        pos += sizeof(struct igmp_v2_extra);
                        len -= sizeof(struct igmp_v2_extra);
                        if (ntoh32(query->group) != 0) {
                            // got a Group Specific Query, lower group timers to LMQT
                            lower_group_timers_to_lmqt(query->group);
                        }
                    }
                    else if (len >= sizeof(struct igmp_v3_query_extra)) {
                        // V3 query
                        struct igmp_v3_query_extra *query = (struct igmp_v3_query_extra *)pos;
                        pos += sizeof(struct igmp_v3_query_extra);
                        len -= sizeof(struct igmp_v3_query_extra);
                        uint16_t num_sources = ntoh16(query->number_of_sources);
                        int i;
                        for (i = 0; i < num_sources; i++) {
                            if (len < sizeof(struct igmp_source)) {
                                BLog(BLOG_NOTICE, "hook outgoing: IGMP: short source");
                                goto out_igmp;
                            }
                            pos += sizeof(struct igmp_source);
                            len -= sizeof(struct igmp_source);
                        }
                        if (i < num_sources) {
                            BLog(BLOG_NOTICE, "hook outgoing: IGMP: not all sources present");
                            goto out_igmp;
                        }
                        if (ntoh32(query->group) != 0 && num_sources == 0) {
                            // got a Group Specific Query, lower group timers to LMQT
                            lower_group_timers_to_lmqt(query->group);
                        }
                    }
                } break;
            }
        out_igmp:
            // flood IGMP frames to allow all peers to learn group membership
            return HOOK_OUT_FLOOD;
        } break;
    }
    
out:
    return HOOK_OUT_DEFAULT;
}

void peer_hook_incoming (struct peer_data *peer, uint8_t *pos, int len)
{
    ASSERT(len >= sizeof(struct ethernet_header))
    
    struct ethernet_header *eth_header = (struct ethernet_header *)pos;
    pos += sizeof(struct ethernet_header);
    len -= sizeof(struct ethernet_header);
    
    switch (ntoh16(eth_header->type)) {
        case ETHERTYPE_IPV4: {
            struct ipv4_header *ipv4_header;
            if (!check_ipv4_packet(pos, len, &ipv4_header, &pos, &len)) {
                BLog(BLOG_INFO, "hook incoming: wrong IP packet");
                goto out;
            }
            if (ipv4_header->protocol != IPV4_PROTOCOL_IGMP) {
                goto out;
            }
            if (len < sizeof(struct igmp_base)) {
                BLog(BLOG_INFO, "hook incoming: IGMP: short");
                goto out;
            }
            struct igmp_base *igmp_base = (struct igmp_base *)pos;
            pos += sizeof(struct igmp_base);
            len -= sizeof(struct igmp_base);
            switch (igmp_base->type) {
                case IGMP_TYPE_V2_MEMBERSHIP_REPORT: {
                    if (len < sizeof(struct igmp_v2_extra)) {
                        BLog(BLOG_INFO, "hook incoming: IGMP: short v2 report");
                        goto out;
                    }
                    struct igmp_v2_extra *report = (struct igmp_v2_extra *)pos;
                    pos += sizeof(struct igmp_v2_extra);
                    len -= sizeof(struct igmp_v2_extra);
                    peer_join_group(peer, report->group);
                } break;
                case IGMP_TYPE_V3_MEMBERSHIP_REPORT: {
                    if (len < sizeof(struct igmp_v3_report_extra)) {
                        BLog(BLOG_INFO, "hook incoming: IGMP: short v3 report");
                        goto out;
                    }
                    struct igmp_v3_report_extra *report = (struct igmp_v3_report_extra *)pos;
                    pos += sizeof(struct igmp_v3_report_extra);
                    len -= sizeof(struct igmp_v3_report_extra);
                    uint16_t num_records = ntoh16(report->number_of_group_records);
                    int i;
                    for (i = 0; i < num_records; i++) {
                        if (len < sizeof(struct igmp_v3_report_record)) {
                            BLog(BLOG_INFO, "hook incoming: IGMP: short record header");
                            goto out;
                        }
                        struct igmp_v3_report_record *record = (struct igmp_v3_report_record *)pos;
                        pos += sizeof(struct igmp_v3_report_record);
                        len -= sizeof(struct igmp_v3_report_record);
                        uint16_t num_sources = ntoh16(record->number_of_sources);
                        int j;
                        for (j = 0; j < num_sources; j++) {
                            if (len < sizeof(struct igmp_source)) {
                                BLog(BLOG_INFO, "hook incoming: IGMP: short source");
                                goto out;
                            }
                            struct igmp_source *source = (struct igmp_source *)pos;
                            pos += sizeof(struct igmp_source);
                            len -= sizeof(struct igmp_source);
                        }
                        if (j < num_sources) {
                            goto out;
                        }
                        uint16_t aux_len = ntoh16(record->aux_data_len);
                        if (len < aux_len) {
                            BLog(BLOG_INFO, "hook incoming: IGMP: short record aux data");
                            goto out;
                        }
                        pos += aux_len;
                        len -= aux_len;
                        switch (record->type) {
                            case IGMP_RECORD_TYPE_MODE_IS_INCLUDE:
                            case IGMP_RECORD_TYPE_CHANGE_TO_INCLUDE_MODE:
                                if (num_sources != 0) {
                                    peer_join_group(peer, record->group);
                                }
                                break;
                            case IGMP_RECORD_TYPE_MODE_IS_EXCLUDE:
                            case IGMP_RECORD_TYPE_CHANGE_TO_EXCLUDE_MODE:
                                peer_join_group(peer, record->group);
                                break;
                        }
                    }
                    if (i < num_records) {
                        BLog(BLOG_INFO, "hook incoming: IGMP: not all records present");
                    }
                } break;
            }
        } break;
    }
    
out:;
}

void lower_group_timers_to_lmqt (uint32_t group)
{
    // lookup the group in every peer's group entries hash table
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &peers);
    LinkedList2Node *peer_list_node;
    while (peer_list_node = LinkedList2Iterator_Next(&it)) {
        struct peer_data *peer = UPPER_OBJECT(peer_list_node, struct peer_data, list_node);
        HashTableNode *groups_table_node;
        if (HashTable_Lookup(&peer->groups_hashtable, &group, &groups_table_node)) {
            struct peer_group_entry *group_entry = UPPER_OBJECT(groups_table_node, struct peer_group_entry, table_node);
            ASSERT(group_entry->peer == peer)
            btime_t now = btime_gettime();
            if (group_entry->timer_endtime > now + IGMP_LAST_MEMBER_QUERY_TIME) {
                group_entry->timer_endtime = now + IGMP_LAST_MEMBER_QUERY_TIME;
                BReactor_SetTimerAbsolute(&ss, &group_entry->timer, group_entry->timer_endtime);
            }
        }
    }
}

int check_ipv4_packet (uint8_t *data, int data_len, struct ipv4_header **out_header, uint8_t **out_payload, int *out_payload_len)
{
    // check base header
    if (data_len < sizeof(struct ipv4_header)) {
        BLog(BLOG_DEBUG, "check ipv4: packet too short (base header)");
        return 0;
    }
    struct ipv4_header *header = (struct ipv4_header *)data;
    
    // check version
    if (IPV4_GET_VERSION(*header) != 4) {
        BLog(BLOG_DEBUG, "check ipv4: version not 4");
        return 0;
    }
    
    // check options
    int header_len = IPV4_GET_IHL(*header) * 4;
    if (header_len < sizeof(struct ipv4_header)) {
        BLog(BLOG_DEBUG, "check ipv4: ihl too small");
        return 0;
    }
    if (header_len > data_len) {
        BLog(BLOG_DEBUG, "check ipv4: packet too short for ihl");
        return 0;
    }
    
    // check total length
    uint16_t total_length = ntoh16(header->total_length);
    if (total_length < header_len) {
        BLog(BLOG_DEBUG, "check ipv4: total length too small");
        return 0;
    }
    if (total_length > data_len) {
        BLog(BLOG_DEBUG, "check ipv4: total length too large");
        return 0;
    }
    
    *out_header = header;
    *out_payload = data + header_len;
    *out_payload_len = total_length - header_len;
    
    return 1;
}

void assign_relays (void)
{
    LinkedList2Node *list_node;
    while (list_node = LinkedList2_GetFirst(&waiting_relay_peers)) {
        struct peer_data *peer = UPPER_OBJECT(list_node, struct peer_data, waiting_relay_list_node);
        ASSERT(peer->waiting_relay)
        
        ASSERT(!peer->have_relaying)
        ASSERT(!peer->have_link)
        
        // get a relay
        LinkedList2Node *list_node2 = LinkedList2_GetFirst(&relays);
        if (!list_node2) {
            BLog(BLOG_NOTICE, "no relays");
            return;
        }
        struct peer_data *relay = UPPER_OBJECT(list_node2, struct peer_data, relay_list_node);
        ASSERT(relay->is_relay)
        
        // no longer waiting for relay
        peer_unregister_need_relay(peer);
        
        // install the relay
        peer_install_relay(peer, relay);
    }
}

char * address_scope_known (uint8_t *name, int name_len)
{
    ASSERT(name_len >= 0)
    
    for (int i = 0; i < options.num_scopes; i++) {
        if (name_len == strlen(options.scopes[i]) && !memcmp(name, options.scopes[i], name_len)) {
            return options.scopes[i];
        }
    }
    
    return NULL;
}

void server_handler_error (void *user)
{
    BLog(BLOG_ERROR, "server connection failed, exiting");
    
    terminate();
    return;
}

void server_handler_ready (void *user, peerid_t param_my_id, uint32_t ext_ip)
{
    ASSERT(!server_ready)
    
    // remember our ID
    my_id = param_my_id;
    
    // store server reported addresses
    for (int i = 0; i < num_bind_addrs; i++) {
        POINTER(addr, bind_addrs[i]);
        for (int j = 0; j < addr->num_ext_addrs; j++) {
            POINTER(eaddr, addr->ext_addrs[j]);
            if (eaddr->server_reported_port >= 0) {
                if (ext_ip == 0) {
                    BLog(BLOG_ERROR, "server did not provide our address");
                    terminate();
                    return;
                }
                BAddr_InitIPv4(&eaddr->addr, ext_ip, hton16(eaddr->server_reported_port));
                char str[BADDR_MAX_PRINT_LEN];
                BAddr_Print(&eaddr->addr, str);
                BLog(BLOG_INFO, "external address (%d,%d): server reported %s", i, j, str);
            }
        }
    }
    
    // set server ready
    server_ready = 1;
    
    BLog(BLOG_INFO, "server: ready, my ID is %d", (int)my_id);
}

void server_handler_newclient (void *user, peerid_t peer_id, int flags, const uint8_t *cert, int cert_len)
{
    ASSERT(server_ready)
    ASSERT(cert_len >= 0)
    ASSERT(cert_len <= SCID_NEWCLIENT_MAX_CERT_LEN)
    
    // check if the peer already exists
    if (find_peer_by_id(peer_id)) {
        BLog(BLOG_WARNING, "server: newclient: peer already known");
        return;
    }
    
    // make sure it's not the same ID as us
    if (peer_id == my_id) {
        BLog(BLOG_WARNING, "server: newclient: peer has our ID");
        return;
    }
    
    // check if there is spece for the peer
    if (num_peers >= MAX_PEERS) {
        BLog(BLOG_WARNING, "server: newclient: no space for new peer (maximum number reached)");
        return;
    }
    
    if (!options.ssl && cert_len > 0) {
        BLog(BLOG_WARNING, "server: newclient: certificate supplied, but not using TLS");
        return;
    }
    
    peer_add(peer_id, flags, cert, cert_len);
    return;
}

void server_handler_endclient (void *user, peerid_t peer_id)
{
    ASSERT(server_ready)
    
    // find peer
    struct peer_data *peer = find_peer_by_id(peer_id);
    if (!peer) {
        BLog(BLOG_WARNING, "server: endclient: peer %d not known", (int)peer_id);
        return;
    }
    
    // remove peer
    peer_remove(peer);
}

void server_handler_message (void *user, peerid_t peer_id, uint8_t *data, int data_len)
{
    ASSERT(server_ready)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= SC_MAX_MSGLEN)
    
    // find peer
    struct peer_data *peer = find_peer_by_id(peer_id);
    if (!peer) {
        BLog(BLOG_WARNING, "server: message: peer not known");
        return;
    }
    
    // process peer message
    peer_msg(peer, data, data_len);
    return;
}

void peer_job_send_seed_after_binding (struct peer_data *peer)
{
    ASSERT(options.transport_mode == TRANSPORT_MODE_UDP)
    ASSERT(SPPROTO_HAVE_OTP(sp_params))
    ASSERT(peer->have_link)
    ASSERT(!peer->pio.udp.sendseed_sent)
    
    peer_udp_send_seed(peer);
    return;
}
