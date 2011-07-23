/**
 * @file udpgw.c
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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <protocol/udpgw_proto.h>
#include <misc/debug.h>
#include <misc/version.h>
#include <misc/loggers_string.h>
#include <misc/loglevel.h>
#include <misc/offset.h>
#include <misc/byteorder.h>
#include <misc/bsize.h>
#include <structure/LinkedList1.h>
#include <structure/BAVL.h>
#include <base/BLog.h>
#include <system/BReactor.h>
#include <system/BNetwork.h>
#include <system/BConnection.h>
#include <system/BDatagram.h>
#include <system/BSignal.h>
#include <flow/PacketProtoDecoder.h>
#include <flow/PacketPassFairQueue.h>
#include <flow/PacketStreamSender.h>
#include <flow/PacketProtoFlow.h>
#include <flow/SinglePacketBuffer.h>

#ifndef BADVPN_USE_WINAPI
#include <base/BLog_syslog.h>
#endif

#include <udpgw/udpgw.h>

#include <generated/blog_channel_udpgw.h>

#define LOGGER_STDOUT 1
#define LOGGER_SYSLOG 2

struct client {
    BConnection con;
    BAddr addr;
    BTimer disconnect_timer;
    PacketProtoDecoder recv_decoder;
    PacketPassInterface recv_if;
    PacketPassFairQueue send_queue;
    PacketStreamSender send_sender;
    BAVL connections_tree;
    LinkedList1 connections_list;
    int num_connections;
    LinkedList1 closing_connections_list;
    LinkedList1Node clients_list_node;
};

struct connection {
    struct client *client;
    uint16_t conid;
    BAddr addr;
    const uint8_t *first_data;
    int first_data_len;
    int closing;
    BPending first_job;
    BufferWriter *send_if;
    PacketProtoFlow send_ppflow;
    PacketPassFairQueueFlow send_qflow;
    union {
        struct {
            BDatagram udp_dgram;
            BufferWriter udp_send_writer;
            PacketBuffer udp_send_buffer;
            SinglePacketBuffer udp_recv_buffer;
            PacketPassInterface udp_recv_if;
            BAVLNode connections_tree_node;
            LinkedList1Node connections_list_node;
        };
        struct {
            LinkedList1Node closing_connections_list_node;
        };
    };
};

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
    char *listen_addrs[MAX_LISTEN_ADDRS];
    int num_listen_addrs;
    int udp_mtu;
    int max_clients;
    int max_connections_for_client;
    int client_socket_sndbuf;
} options;

// MTUs
int udpgw_mtu;
int pp_mtu;

// listen addresses
BAddr listen_addrs[MAX_LISTEN_ADDRS];
int num_listen_addrs;

// reactor
BReactor ss;

// listeners
BListener listeners[MAX_LISTEN_ADDRS];
int num_listeners;

// clients
LinkedList1 clients_list;
int num_clients;

static void print_help (const char *name);
static void print_version (void);
static int parse_arguments (int argc, char *argv[]);
static int process_arguments (void);
static void signal_handler (void *unused);
static void listener_handler (BListener *listener);
static void client_free (struct client *client);
static void client_logfunc (struct client *client);
static void client_log (struct client *client, int level, const char *fmt, ...);
static void client_disconnect_timer_handler (struct client *client);
static void client_connection_handler (struct client *client, int event);
static void client_decoder_handler_error (struct client *client);
static void client_recv_if_handler_send (struct client *client, uint8_t *data, int data_len);
static void connection_init (struct client *client, uint16_t conid, BAddr addr, const uint8_t *data, int data_len);
static void connection_free (struct connection *con);
static void connection_logfunc (struct connection *con);
static void connection_log (struct connection *con, int level, const char *fmt, ...);
static void connection_free_udp (struct connection *con);
static void connection_first_job_handler (struct connection *con);
static int connection_send_to_client (struct connection *con, uint8_t flags, const uint8_t *data, int data_len);
static int connection_send_to_udp (struct connection *con, const uint8_t *data, int data_len);
static void connection_close (struct connection *con);
static void connection_send_qflow_busy_handler (struct connection *con);
static void connection_dgram_handler_event (struct connection *con, int event);
static void connection_udp_recv_if_handler_send (struct connection *con, uint8_t *data, int data_len);
static struct connection * find_connection (struct client *client, uint16_t conid);
static int uint16_comparator (void *unused, uint16_t *v1, uint16_t *v2);

int main (int argc, char **argv)
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
    
    BLog(BLOG_NOTICE, "initializing "GLOBAL_PRODUCT_NAME" "PROGRAM_NAME" "GLOBAL_VERSION);
    
    // initialize network
    if (!BNetwork_GlobalInit()) {
        BLog(BLOG_ERROR, "BNetwork_GlobalInit failed");
        goto fail1;
    }
    
    // process arguments
    if (!process_arguments()) {
        BLog(BLOG_ERROR, "Failed to process arguments");
        goto fail1;
    }
    
    // compute MTUs
    if ((udpgw_mtu = udpgw_compute_mtu(options.udp_mtu)) < 0 ||
        udpgw_mtu > PACKETPROTO_MAXPAYLOAD
    ) {
        BLog(BLOG_ERROR, "MTU is too big");
        goto fail1;
    }
    pp_mtu = udpgw_mtu + sizeof(struct packetproto_header);
    
    // init time
    BTime_Init();
    
    // init reactor
    if (!BReactor_Init(&ss)) {
        BLog(BLOG_ERROR, "BReactor_Init failed");
        goto fail1;
    }
    
    // setup signal handler
    if (!BSignal_Init(&ss, signal_handler, NULL)) {
        BLog(BLOG_ERROR, "BSignal_Init failed");
        goto fail2;
    }
    
    // initialize listeners
    num_listeners = 0;
    while (num_listeners < num_listen_addrs) {
        if (!BListener_Init(&listeners[num_listeners], listen_addrs[num_listeners], &ss, &listeners[num_listeners], (BListener_handler)listener_handler)) {
            BLog(BLOG_ERROR, "Listener_Init failed");
            goto fail3;
        }
        num_listeners++;
    }
    
    // init clients list
    LinkedList1_Init(&clients_list);
    num_clients = 0;
    
    // enter event loop
    BLog(BLOG_NOTICE, "entering event loop");
    BReactor_Exec(&ss);
    
    // free clients
    while (!LinkedList1_IsEmpty(&clients_list)) {
        struct client *client = UPPER_OBJECT(LinkedList1_GetFirst(&clients_list), struct client, clients_list_node);
        client_free(client);
    }
fail3:
    // free listeners
    while (num_listeners > 0) {
        num_listeners--;
        BListener_Free(&listeners[num_listeners]);
    }
    // finish signal handling
    BSignal_Finish();
fail2:
    // free reactor
    BReactor_Free(&ss);
fail1:
    // free logger
    BLog(BLOG_NOTICE, "exiting");
    BLog_Free();
fail0:
    // finish debug objects
    DebugObjectGlobal_Finish();
    
    return 1;
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
        "        [--listen-addr <addr>] ...\n"
        "        [--udp-mtu <bytes>]\n"
        "        [--max-clients <number>]\n"
        "        [--max-connections-for-client <number>]\n"
        "        [--client-socket-sndbuf <bytes / 0>]\n"
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
    options.num_listen_addrs = 0;
    options.udp_mtu = DEFAULT_UDP_MTU;
    options.max_clients = DEFAULT_MAX_CLIENTS;
    options.max_connections_for_client = DEFAULT_MAX_CONNECTIONS_FOR_CLIENT;
    options.client_socket_sndbuf = CLIENT_DEFAULT_SOCKET_SEND_BUFFER;
    
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
        else if (!strcmp(arg, "--listen-addr")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if (options.num_listen_addrs == MAX_LISTEN_ADDRS) {
                fprintf(stderr, "%s: too many\n", arg);
                return 0;
            }
            options.listen_addrs[options.num_listen_addrs] = argv[i + 1];
            options.num_listen_addrs++;
            i++;
        }
        else if (!strcmp(arg, "--udp-mtu")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if ((options.udp_mtu = atoi(argv[i + 1])) < 0) {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        else if (!strcmp(arg, "--max-clients")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if ((options.max_clients = atoi(argv[i + 1])) <= 0) {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        else if (!strcmp(arg, "--max-connections-for-client")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if ((options.max_connections_for_client = atoi(argv[i + 1])) <= 0) {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        else if (!strcmp(arg, "--client-socket-sndbuf")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if ((options.client_socket_sndbuf = atoi(argv[i + 1])) < 0) {
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
    
    return 1;
}

int process_arguments (void)
{
    // resolve listen addresses
    num_listen_addrs = 0;
    while (num_listen_addrs < options.num_listen_addrs) {
        if (!BAddr_Parse(&listen_addrs[num_listen_addrs], options.listen_addrs[num_listen_addrs], NULL, 0)) {
            BLog(BLOG_ERROR, "listen addr: BAddr_Parse failed");
            return 0;
        }
        num_listen_addrs++;
    }
    
    return 1;
}

void signal_handler (void *unused)
{
    BLog(BLOG_NOTICE, "termination requested");
    
    // exit event loop
    BReactor_Quit(&ss, 1);
}

void listener_handler (BListener *listener)
{
    if (num_clients == options.max_clients) {
        BLog(BLOG_ERROR, "maximum number of clients reached");
        goto fail0;
    }
    
    // allocate structure
    struct client *client = malloc(sizeof(*client));
    if (!client) {
        BLog(BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    
    // accept client
    if (!BConnection_Init(&client->con, BCONNECTION_SOURCE_LISTENER(listener, &client->addr), &ss, client, (BConnection_handler)client_connection_handler)) {
        BLog(BLOG_ERROR, "BConnection_Init failed");
        goto fail1;
    }
    
    // limit socket send buffer, else our scheduling is pointless
    if (options.client_socket_sndbuf > 0) {
        if (!BConnection_SetSendBuffer(&client->con, options.client_socket_sndbuf)) {
            BLog(BLOG_WARNING, "BConnection_SetSendBuffer failed");
        }
    }
    
    // init connection interfaces
    BConnection_SendAsync_Init(&client->con);
    BConnection_RecvAsync_Init(&client->con);
    
    // init disconnect timer
    BTimer_Init(&client->disconnect_timer, CLIENT_DISCONNECT_TIMEOUT, (BTimer_handler)client_disconnect_timer_handler, client);
    BReactor_SetTimer(&ss, &client->disconnect_timer);
    
    // init recv interface
    PacketPassInterface_Init(&client->recv_if, udpgw_mtu, (PacketPassInterface_handler_send)client_recv_if_handler_send, client, BReactor_PendingGroup(&ss));
    
    // init recv decoder
    if (!PacketProtoDecoder_Init(&client->recv_decoder, BConnection_RecvAsync_GetIf(&client->con), &client->recv_if, BReactor_PendingGroup(&ss), client,
        (PacketProtoDecoder_handler_error)client_decoder_handler_error
    )) {
        BLog(BLOG_ERROR, "PacketProtoDecoder_Init failed");
        goto fail2;
    }
    
    // init send sender
    PacketStreamSender_Init(&client->send_sender, BConnection_SendAsync_GetIf(&client->con), pp_mtu, BReactor_PendingGroup(&ss));
    
    // init send queue
    if (!PacketPassFairQueue_Init(&client->send_queue, PacketStreamSender_GetInput(&client->send_sender), BReactor_PendingGroup(&ss), 0, 1)) {
        BLog(BLOG_ERROR, "PacketPassFairQueue_Init failed");
        goto fail3;
    }
    
    // init connections tree
    BAVL_Init(&client->connections_tree, OFFSET_DIFF(struct connection, conid, connections_tree_node), (BAVL_comparator)uint16_comparator, NULL);
    
    // init connections list
    LinkedList1_Init(&client->connections_list);
    
    // set zero connections
    client->num_connections = 0;
    
    // init closing connections list
    LinkedList1_Init(&client->closing_connections_list);
    
    // insert to clients list
    LinkedList1_Append(&clients_list, &client->clients_list_node);
    num_clients++;
    
    client_log(client, BLOG_INFO, "connected");
    
    return;
    
fail3:
    PacketStreamSender_Free(&client->send_sender);
    PacketProtoDecoder_Free(&client->recv_decoder);
fail2:
    PacketPassInterface_Free(&client->recv_if);
    BReactor_RemoveTimer(&ss, &client->disconnect_timer);
    BConnection_RecvAsync_Free(&client->con);
    BConnection_SendAsync_Free(&client->con);
    BConnection_Free(&client->con);
fail1:
    free(client);
fail0:
    return;
}

void client_free (struct client *client)
{
    // allow freeing send queue flows
    PacketPassFairQueue_PrepareFree(&client->send_queue);
    
    // free connections
    while (!LinkedList1_IsEmpty(&client->connections_list)) {
        struct connection *con = UPPER_OBJECT(LinkedList1_GetFirst(&client->connections_list), struct connection, connections_list_node);
        connection_free(con);
    }
    
    // free closing connections
    while (!LinkedList1_IsEmpty(&client->closing_connections_list)) {
        struct connection *con = UPPER_OBJECT(LinkedList1_GetFirst(&client->closing_connections_list), struct connection, closing_connections_list_node);
        connection_free(con);
    }
    
    // remove from clients list
    LinkedList1_Remove(&clients_list, &client->clients_list_node);
    num_clients--;
    
    // free send queue
    PacketPassFairQueue_Free(&client->send_queue);
    
    // free send sender
    PacketStreamSender_Free(&client->send_sender);
    
    // free recv decoder
    PacketProtoDecoder_Free(&client->recv_decoder);
    
    // free recv interface
    PacketPassInterface_Free(&client->recv_if);
    
    // free disconnect timer
    BReactor_RemoveTimer(&ss, &client->disconnect_timer);
    
    // free connection interfaces
    BConnection_RecvAsync_Free(&client->con);
    BConnection_SendAsync_Free(&client->con);
    
    // free connection
    BConnection_Free(&client->con);
    
    // free structure
    free(client);
}

void client_logfunc (struct client *client)
{
    char addr[BADDR_MAX_PRINT_LEN];
    BAddr_Print(&client->addr, addr);
    
    BLog_Append("client (%s): ", addr);
}

void client_log (struct client *client, int level, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    BLog_LogViaFuncVarArg((BLog_logfunc)client_logfunc, client, BLOG_CURRENT_CHANNEL, level, fmt, vl);
    va_end(vl);
}

void client_disconnect_timer_handler (struct client *client)
{
    client_log(client, BLOG_INFO, "timed out, disconnecting");
    
    // free client
    client_free(client);
}

void client_connection_handler (struct client *client, int event)
{
    if (event == BCONNECTION_EVENT_RECVCLOSED) {
        client_log(client, BLOG_INFO, "client closed");
    } else {
        client_log(client, BLOG_INFO, "client error");
    }
    
    // free client
    client_free(client);
}

void client_decoder_handler_error (struct client *client)
{
    client_log(client, BLOG_ERROR, "decoder error");
    
    // free client
    client_free(client);
}

void client_recv_if_handler_send (struct client *client, uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= udpgw_mtu)
    
    // accept packet
    PacketPassInterface_Done(&client->recv_if);
    
    // parse header
    if (data_len < sizeof(struct udpgw_header)) {
        client_log(client, BLOG_ERROR, "missing header");
        return;
    }
    struct udpgw_header *header = (struct udpgw_header *)data;
    data += sizeof(*header);
    data_len -= sizeof(*header);
    uint8_t flags = ltoh8(header->flags);
    uint16_t conid = ltoh16(header->conid);
    
    // reset disconnect timer
    BReactor_SetTimer(&ss, &client->disconnect_timer);
    
    // if this is keepalive, ignore any payload
    if ((flags & UDPGW_CLIENT_FLAG_KEEPALIVE)) {
        client_log(client, BLOG_DEBUG, "received keepalive");
        return;
    }
    
    // check payload length
    if (data_len > options.udp_mtu) {
        client_log(client, BLOG_ERROR, "too much data");
        return;
    }
    
    // find connection
    struct connection *con = find_connection(client, conid);
    ASSERT(!con || !con->closing)
    
    // if connection exists, close it if needed
    if (con && ((flags & UDPGW_CLIENT_FLAG_REBIND) || con->addr.ipv4.ip != header->addr_ip || con->addr.ipv4.port != header->addr_port)) {
        connection_log(con, BLOG_DEBUG, "close old");
        
        connection_close(con);
        con = NULL;
    }
    
    // if connection doesn't exists, create it
    if (!con) {
        // check number of connections
        if (client->num_connections == options.max_connections_for_client) {
            // close least recently used connection
            con = UPPER_OBJECT(LinkedList1_GetFirst(&client->connections_list), struct connection, connections_list_node);
            connection_close(con);
        }
        
        // read address
        BAddr addr;
        BAddr_InitIPv4(&addr, header->addr_ip, header->addr_port);
        
        // create new connection
        connection_init(client, conid, addr, data, data_len);
    } else {
        // submit packet to existing connection
        connection_send_to_udp(con, data, data_len);
    }
}

void connection_init (struct client *client, uint16_t conid, BAddr addr, const uint8_t *data, int data_len)
{
    ASSERT(client->num_connections < options.max_connections_for_client)
    ASSERT(!find_connection(client, conid))
    BAddr_Assert(&addr);
    ASSERT(addr.type == BADDR_TYPE_IPV4)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= options.udp_mtu)
    
    // allocate structure
    struct connection *con = malloc(sizeof(*con));
    if (!con) {
        client_log(client, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    
    // init arguments
    con->client = client;
    con->conid = conid;
    con->addr = addr;
    con->first_data = data;
    con->first_data_len = data_len;
    
    // set not closing
    con->closing = 0;
    
    // init first job
    BPending_Init(&con->first_job, BReactor_PendingGroup(&ss), (BPending_handler)connection_first_job_handler, con);
    BPending_Set(&con->first_job);
    
    // init send queue flow
    PacketPassFairQueueFlow_Init(&con->send_qflow, &client->send_queue);
    
    // init send PacketProtoFlow
    if (!PacketProtoFlow_Init(&con->send_ppflow, udpgw_mtu, CONNECTION_CLIENT_BUFFER_SIZE, PacketPassFairQueueFlow_GetInput(&con->send_qflow), BReactor_PendingGroup(&ss))) {
        client_log(client, BLOG_ERROR, "PacketProtoFlow_Init failed");
        goto fail1;
    }
    con->send_if = PacketProtoFlow_GetInput(&con->send_ppflow);
    
    // init UDP dgram
    if (!BDatagram_Init(&con->udp_dgram, addr.type, &ss, con, (BDatagram_handler)connection_dgram_handler_event)) {
        client_log(client, BLOG_ERROR, "BDatagram_Init failed");
        goto fail2;
    }
    
    // set UDP dgram send address
    BIPAddr ipaddr;
    BIPAddr_InitInvalid(&ipaddr);
    BDatagram_SetSendAddrs(&con->udp_dgram, addr, ipaddr);
    
    // init UDP dgram interfaces
    BDatagram_SendAsync_Init(&con->udp_dgram, options.udp_mtu);
    BDatagram_RecvAsync_Init(&con->udp_dgram, options.udp_mtu);
    
    // init UDP writer
    BufferWriter_Init(&con->udp_send_writer, options.udp_mtu, BReactor_PendingGroup(&ss));
    
    // init UDP buffer
    if (!PacketBuffer_Init(&con->udp_send_buffer, BufferWriter_GetOutput(&con->udp_send_writer), BDatagram_SendAsync_GetIf(&con->udp_dgram), CONNECTION_UDP_BUFFER_SIZE, BReactor_PendingGroup(&ss))) {
        client_log(client, BLOG_ERROR, "PacketBuffer_Init failed");
        goto fail4;
    }
    
    // init UDP recv interface
    PacketPassInterface_Init(&con->udp_recv_if, options.udp_mtu, (PacketPassInterface_handler_send)connection_udp_recv_if_handler_send, con, BReactor_PendingGroup(&ss));
    
    // init UDP recv buffer
    if (!SinglePacketBuffer_Init(&con->udp_recv_buffer, BDatagram_RecvAsync_GetIf(&con->udp_dgram), &con->udp_recv_if, BReactor_PendingGroup(&ss))) {
        client_log(client, BLOG_ERROR, "SinglePacketBuffer_Init failed");
        goto fail5;
    }
    
    // insert to client's connections tree
    ASSERT_EXECUTE(BAVL_Insert(&client->connections_tree, &con->connections_tree_node, NULL))
    
    // insert to client's connections list
    LinkedList1_Append(&client->connections_list, &con->connections_list_node);
    
    // increment number of connections
    client->num_connections++;
    
    connection_log(con, BLOG_DEBUG, "initialized");
    
    return;
    
fail5:
    PacketPassInterface_Free(&con->udp_recv_if);
    PacketBuffer_Free(&con->udp_send_buffer);
fail4:
    BufferWriter_Free(&con->udp_send_writer);
    BDatagram_RecvAsync_Free(&con->udp_dgram);
    BDatagram_SendAsync_Free(&con->udp_dgram);
    BDatagram_Free(&con->udp_dgram);
fail2:
    PacketProtoFlow_Free(&con->send_ppflow);
fail1:
    PacketPassFairQueueFlow_Free(&con->send_qflow);
    BPending_Free(&con->first_job);
    free(con);
fail0:
    return;
}

void connection_free (struct connection *con)
{
    struct client *client = con->client;
    PacketPassFairQueueFlow_AssertFree(&con->send_qflow);
    
    if (con->closing) {
        // remove from client's closing connections list
        LinkedList1_Remove(&client->closing_connections_list, &con->closing_connections_list_node);
    } else {
        // decrement number of connections
        client->num_connections--;
        
        // remove from client's connections list
        LinkedList1_Remove(&client->connections_list, &con->connections_list_node);
        
        // remove from client's connections tree
        BAVL_Remove(&client->connections_tree, &con->connections_tree_node);
        
        // free UDP
        connection_free_udp(con);
    }
    
    // free send PacketProtoFlow
    PacketProtoFlow_Free(&con->send_ppflow);
    
    // free send queue flow
    PacketPassFairQueueFlow_Free(&con->send_qflow);
    
    // free first job
    BPending_Free(&con->first_job);
    
    // free structure
    free(con);
}

void connection_logfunc (struct connection *con)
{
    client_logfunc(con->client);
    
    if (con->closing) {
        BLog_Append("old connection %"PRIu16": ", con->conid);
    } else {
        BLog_Append("connection %"PRIu16": ", con->conid);
    }
}

void connection_log (struct connection *con, int level, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    BLog_LogViaFuncVarArg((BLog_logfunc)connection_logfunc, con, BLOG_CURRENT_CHANNEL, level, fmt, vl);
    va_end(vl);
}

void connection_free_udp (struct connection *con)
{
    // free UDP receive buffer
    SinglePacketBuffer_Free(&con->udp_recv_buffer);
    
    // free UDP receive interface
    PacketPassInterface_Free(&con->udp_recv_if);
    
    // free UDP buffer
    PacketBuffer_Free(&con->udp_send_buffer);
    
    // free UDP writer
    BufferWriter_Free(&con->udp_send_writer);
    
    // free UDP dgram interfaces
    BDatagram_RecvAsync_Free(&con->udp_dgram);
    BDatagram_SendAsync_Free(&con->udp_dgram);
    
    // free UDP dgram
    BDatagram_Free(&con->udp_dgram);
}

void connection_first_job_handler (struct connection *con)
{
    ASSERT(!con->closing)
    
    connection_send_to_udp(con, con->first_data, con->first_data_len);
}

int connection_send_to_client (struct connection *con, uint8_t flags, const uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= options.udp_mtu)
    
    // get buffer location
    uint8_t *out;
    if (!BufferWriter_StartPacket(con->send_if, &out)) {
        connection_log(con, BLOG_ERROR, "out of client buffer");
        return 0;
    }
    
    // write header
    struct udpgw_header *header = (struct udpgw_header *)out;
    header->flags = htol8(flags);
    header->conid = htol16(con->conid);
    header->addr_ip = con->addr.ipv4.ip;
    header->addr_port = con->addr.ipv4.port;
    
    // write message
    memcpy(out + sizeof(*header), data, data_len);
    
    // submit written message
    BufferWriter_EndPacket(con->send_if, sizeof(*header) + data_len);
    
    return 1;
}

int connection_send_to_udp (struct connection *con, const uint8_t *data, int data_len)
{
    struct client *client = con->client;
    ASSERT(!con->closing)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= options.udp_mtu)
    
    connection_log(con, BLOG_DEBUG, "from client %d bytes", data_len);
    
    // move connection to front
    LinkedList1_Remove(&client->connections_list, &con->connections_list_node);
    LinkedList1_Append(&client->connections_list, &con->connections_list_node);
    
    // get buffer location
    uint8_t *out;
    if (!BufferWriter_StartPacket(&con->udp_send_writer, &out)) {
        connection_log(con, BLOG_ERROR, "out of UDP buffer");
        return 0;
    }
    
    // write message
    memcpy(out, data, data_len);
    
    // submit written message
    BufferWriter_EndPacket(&con->udp_send_writer, data_len);
    
    return 1;
}

void connection_close (struct connection *con)
{
    struct client *client = con->client;
    ASSERT(!con->closing)
    
    // if possible, free connection immediately
    if (!PacketPassFairQueueFlow_IsBusy(&con->send_qflow)) {
        connection_free(con);
        return;
    }
    
    connection_log(con, BLOG_DEBUG, "closing later");
    
    // decrement number of connections
    client->num_connections--;
    
    // remove from client's connections list
    LinkedList1_Remove(&client->connections_list, &con->connections_list_node);
    
    // remove from client's connections tree
    BAVL_Remove(&client->connections_tree, &con->connections_tree_node);
    
    // free UDP
    connection_free_udp(con);
    
    // insert to client's closing connections list
    LinkedList1_Append(&client->closing_connections_list, &con->closing_connections_list_node);
    
    // set busy handler
    PacketPassFairQueueFlow_SetBusyHandler(&con->send_qflow, (PacketPassFairQueue_handler_busy)connection_send_qflow_busy_handler, con);
    
    // unset first job
    BPending_Unset(&con->first_job);
    
    // set closing
    con->closing = 1;
}

void connection_send_qflow_busy_handler (struct connection *con)
{
    ASSERT(con->closing)
    PacketPassFairQueueFlow_AssertFree(&con->send_qflow);
    
    connection_log(con, BLOG_DEBUG, "closing finally");
    
    // free connection
    connection_free(con);
}

void connection_dgram_handler_event (struct connection *con, int event)
{
    ASSERT(!con->closing)
    
    connection_log(con, BLOG_INFO, "UDP error");
    
    // close connection
    connection_close(con);
}

void connection_udp_recv_if_handler_send (struct connection *con, uint8_t *data, int data_len)
{
    struct client *client = con->client;
    ASSERT(!con->closing)
    ASSERT(data_len >= 0)
    ASSERT(data_len <= options.udp_mtu)
    
    connection_log(con, BLOG_DEBUG, "from UDP %d bytes", data_len);
    
    // move connection to front
    LinkedList1_Remove(&client->connections_list, &con->connections_list_node);
    LinkedList1_Append(&client->connections_list, &con->connections_list_node);
    
    // accept packet
    PacketPassInterface_Done(&con->udp_recv_if);
    
    // send packet to client
    connection_send_to_client(con, 0, data, data_len);
}

struct connection * find_connection (struct client *client, uint16_t conid)
{
    BAVLNode *tree_node = BAVL_LookupExact(&client->connections_tree, &conid);
    if (!tree_node) {
        return NULL;
    }
    struct connection *con = UPPER_OBJECT(tree_node, struct connection, connections_tree_node);
    ASSERT(con->conid == conid)
    ASSERT(!con->closing)
    
    return con;
}

int uint16_comparator (void *unused, uint16_t *v1, uint16_t *v2)
{
    if (*v1 < *v2) {
        return -1;
    }
    if (*v1 > *v2) {
        return 1;
    }
    return 0;
}
