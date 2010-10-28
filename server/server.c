/**
 * @file server.c
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
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>

// NSPR and NSS
#include <prinit.h>
#include <prio.h>
#include <prerror.h>
#include <prtypes.h>
#include <nss.h>
#include <ssl.h>
#include <cert.h>
#include <keyhi.h>
#include <secasn1.h>

// BadVPN
#include <misc/version.h>
#include <misc/debug.h>
#include <misc/jenkins_hash.h>
#include <misc/offset.h>
#include <misc/nsskey.h>
#include <misc/dead.h>
#include <misc/byteorder.h>
#include <misc/brandom.h>
#include <misc/loglevel.h>
#include <misc/loggers_string.h>
#include <nspr_support/DummyPRFileDesc.h>
#include <predicate/BPredicate.h>
#include <system/BLog.h>
#include <system/BSignal.h>
#include <system/BTime.h>
#include <system/DebugObject.h>
#include <system/BAddr.h>
#include <listener/Listener.h>

#ifndef BADVPN_USE_WINAPI
#include <system/BLog_syslog.h>
#endif

#include <server/server.h>

#include <generated/blog_channel_server.h>

#define COMPONENT_SOURCE 1
#define COMPONENT_SINK 2
#define COMPONENT_DECODER 3

#define LOGGER_STDOUT 1
#define LOGGER_SYSLOG 2

// program dead variable
dead_t dead;

// parsed command-line options
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
    char *server_cert_name;
    char *listen_addrs[MAX_LISTEN_ADDRS];
    int num_listen_addrs;
    char *comm_predicate;
    char *relay_predicate;
} options;

// listen addresses
BAddr listen_addrs[MAX_LISTEN_ADDRS];
int num_listen_addrs;

// communication predicate
BPredicate comm_predicate;

// communication predicate functions
BPredicateFunction comm_predicate_func_p1name;
BPredicateFunction comm_predicate_func_p2name;
BPredicateFunction comm_predicate_func_p1addr;
BPredicateFunction comm_predicate_func_p2addr;

// variables when evaluating the predicate, adjusted before every evaluation
const char *comm_predicate_p1name;
const char *comm_predicate_p2name;
BIPAddr comm_predicate_p1addr;
BIPAddr comm_predicate_p2addr;

// relay predicate
BPredicate relay_predicate;

// gateway predicate functions
BPredicateFunction relay_predicate_func_pname;
BPredicateFunction relay_predicate_func_rname;
BPredicateFunction relay_predicate_func_paddr;
BPredicateFunction relay_predicate_func_raddr;

// variables when evaluating the comm_predicate, adjusted before every evaluation
const char *relay_predicate_pname;
const char *relay_predicate_rname;
BIPAddr relay_predicate_paddr;
BIPAddr relay_predicate_raddr;

// i/o system
BReactor ss;

// server certificate if using SSL
CERTCertificate *server_cert;

// server private key if using SSL
SECKEYPrivateKey *server_key;

// model NSPR file descriptor to speed up client initialization
PRFileDesc model_dprfd;
PRFileDesc *model_prfd;

// listeners
Listener listeners[MAX_LISTEN_ADDRS];
int num_listeners;

// number of connected clients
int clients_num;

// ID assigned to last connected client
peerid_t clients_lastid;

// clients list
LinkedList2 clients;

// clients hash table by client ID
HashTable clients_by_id;
uint32_t clients_by_id_initval;

// cleans everything up that can be cleaned in order to return
// from the event loop and exit
static void terminate (void);

// prints help text to standard output
static void print_help (const char *name);

// prints program name and version to standard output
static void print_version (void);

// parses the command line
static int parse_arguments (int argc, char *argv[]);

// processes certain command line options
static int resolve_arguments (void);

// handler for program termination request
static void signal_handler (void *unused);

// listener socket handler, accepts new clients
static void listener_handler (Listener *listener);

// adds a client. The client structure must have the sock and addr members
// already initialized.
static void client_add (struct client_data *client);

// removes a client
static void client_remove (struct client_data *client);

// frees resources used by a client. Must have no outgoing flows.
static void client_dealloc (struct client_data *client);

// passes a message to the logger, prepending about the client
static void client_log (struct client_data *client, int level, const char *fmt, ...);

// client activity timer handler. Removes the client.
static void client_disconnect_timer_handler (struct client_data *client);

// drives cline SSL handshake
static void client_try_handshake (struct client_data *client);

// event handler for driving client SSL handshake
static void client_handshake_read_handler (struct client_data *client, PRInt16 event);

// initializes the I/O porition of the client
static int client_init_io (struct client_data *client);

// deallocates the I/O portion of the client. Must have no outgoing flows.
static void client_dealloc_io (struct client_data *client);

// handler for client I/O errors. Removes the client.
static void client_error_handler (struct client_data *client, int component, const void *data);

// provides a buffer for sending a control packet to the client
static int client_start_control_packet (struct client_data *client, void **data, int len);

// submits a packet written after client_start_control_packet
static int client_end_control_packet (struct client_data *client, uint8_t id);

// handler for packets received from the client
static int client_input_handler_send (struct client_data *client, uint8_t *data, int data_len);

// creates a peer flow
static struct peer_flow * peer_flow_create (struct client_data *src_client, struct client_data *dest_client);

// deallocates a peer flow
static void peer_flow_dealloc (struct peer_flow *flow);

// disconnects the source client from a peer flow
static void peer_flow_disconnect (struct peer_flow *flow);

// provides a buffer for sending a peer-to-peer packet
static int peer_flow_start_packet (struct peer_flow *flow, void **data, int len);

// submits a peer-to-peer packet written after peer_flow_start_packet
static int peer_flow_end_packet (struct peer_flow *flow, uint8_t type);

// handler called by the queue when a peer flow can be freed after its source has gone away
static void peer_flow_handler_canremove (struct peer_flow *flow);

// processes hello packets from clients
static void process_packet_hello (struct client_data *client, uint8_t *data, int data_len);

// processes outmsg packets from clients
static void process_packet_outmsg (struct client_data *client, uint8_t *data, int data_len);

// sends a newclient message to a client
static int client_send_newclient (struct client_data *client, struct client_data *nc, int relay_server, int relay_client);

// sends an endclient message to a client
static int client_send_endclient (struct client_data *client, peerid_t end_id);

// informs two clients of each other after one of them has just come. Does nothing
// if they are not permitted by the communication predicate.
void connect_clients (struct client_data *clientA, struct client_data *clientB);

// calls connect_clients for this client and all other finished peers
static int publish_client (struct client_data *client);

// generates a client ID to be used for a newly connected client
static peerid_t new_client_id (void);

// finds a client by its ID
static struct client_data * find_client_by_id (peerid_t id);

// clients by ID hash table key comparator
static int clients_by_id_key_comparator (peerid_t *id1, peerid_t *id2);

// clients by ID hash table hash function
static int clients_by_id_hash_function (peerid_t *id, int modulo);

// checks if two clients are allowed to communicate. May depend on the order
// of the clients.
static int clients_allowed (struct client_data *client1, struct client_data *client2);

// communication predicate function p1name
static int comm_predicate_func_p1name_cb (void *user, void **args);

// communication predicate function p2name
static int comm_predicate_func_p2name_cb (void *user, void **args);

// communication predicate function p1addr
static int comm_predicate_func_p1addr_cb (void *user, void **args);

// communication predicate function p2addr
static int comm_predicate_func_p2addr_cb (void *user, void **args);

// checks if relay is allowed for a client through another client
static int relay_allowed (struct client_data *client, struct client_data *relay);

// relay predicate function pname
static int relay_predicate_func_pname_cb (void *user, void **args);

// relay predicate function rname
static int relay_predicate_func_rname_cb (void *user, void **args);

// relay predicate function paddr
static int relay_predicate_func_paddr_cb (void *user, void **args);

// relay predicate function raddr
static int relay_predicate_func_raddr_cb (void *user, void **args);

// comparator for peerid_t used in AVL tree
static int peerid_comparator (void *unused, peerid_t *p1, peerid_t *p2);

int main (int argc, char *argv[])
{
    if (argc <= 0) {
        return 1;
    }
    
    // init dead variable
    DEAD_INIT(dead);
    
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
    for (int i = 0; i < BLOG_NUM_CHANNELS; i++) {
        if (options.loglevels[i] >= 0) {
            BLog_SetChannelLoglevel(i, options.loglevels[i]);
        }
        else if (options.loglevel >= 0) {
            BLog_SetChannelLoglevel(i, options.loglevel);
        }
    }
    
    BLog(BLOG_NOTICE, "initializing "GLOBAL_PRODUCT_NAME" server "GLOBAL_VERSION);
    
    // initialize sockets
    if (BSocket_GlobalInit() < 0) {
        BLog(BLOG_ERROR, "BSocket_GlobalInit failed");
        goto fail1;
    }
    
    // resolve addresses
    if (!resolve_arguments()) {
        BLog(BLOG_ERROR, "Failed to resolve arguments");
        goto fail1;
    }
    
    // init communication predicate
    if (options.comm_predicate) {
        // init predicate
        if (!BPredicate_Init(&comm_predicate, options.comm_predicate)) {
            BLog(BLOG_ERROR, "BPredicate_Init failed");
            goto fail1;
        }
        
        // init functions
        BPredicateFunction_Init(&comm_predicate_func_p1name, &comm_predicate, "p1name", (int []){PREDICATE_TYPE_STRING}, 1, comm_predicate_func_p1name_cb, NULL);
        BPredicateFunction_Init(&comm_predicate_func_p2name, &comm_predicate, "p2name", (int []){PREDICATE_TYPE_STRING}, 1, comm_predicate_func_p2name_cb, NULL);
        BPredicateFunction_Init(&comm_predicate_func_p1addr, &comm_predicate, "p1addr", (int []){PREDICATE_TYPE_STRING}, 1, comm_predicate_func_p1addr_cb, NULL);
        BPredicateFunction_Init(&comm_predicate_func_p2addr, &comm_predicate, "p2addr", (int []){PREDICATE_TYPE_STRING}, 1, comm_predicate_func_p2addr_cb, NULL);
    }
    
    // init relay predicate
    if (options.relay_predicate) {
        // init predicate
        if (!BPredicate_Init(&relay_predicate, options.relay_predicate)) {
            BLog(BLOG_ERROR, "BPredicate_Init failed");
            goto fail1_1;
        }
        
        // init functions
        BPredicateFunction_Init(&relay_predicate_func_pname, &relay_predicate, "pname", (int []){PREDICATE_TYPE_STRING}, 1, relay_predicate_func_pname_cb, NULL);
        BPredicateFunction_Init(&relay_predicate_func_rname, &relay_predicate, "rname", (int []){PREDICATE_TYPE_STRING}, 1, relay_predicate_func_rname_cb, NULL);
        BPredicateFunction_Init(&relay_predicate_func_paddr, &relay_predicate, "paddr", (int []){PREDICATE_TYPE_STRING}, 1, relay_predicate_func_paddr_cb, NULL);
        BPredicateFunction_Init(&relay_predicate_func_raddr, &relay_predicate, "raddr", (int []){PREDICATE_TYPE_STRING}, 1, relay_predicate_func_raddr_cb, NULL);
    }
    
    // init time
    BTime_Init();
    
    // initialize reactor
    if (!BReactor_Init(&ss)) {
        BLog(BLOG_ERROR, "BReactor_Init failed");
        goto fail2;
    }
    
    // setup signal handler
    if (!BSignal_Init()) {
        BLog(BLOG_ERROR, "BSignal_Init failed");
        goto fail2a;
    }
    BSignal_Capture();
    if (!BSignal_SetHandler(&ss, signal_handler, NULL)) {
        BLog(BLOG_ERROR, "BSignal_SetHandler failed");
        goto fail2a;
    }
    
    if (options.ssl) {
        // initialize NSPR
        PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
        
        // initialize i/o layer types
        if (!DummyPRFileDesc_GlobalInit()) {
            BLog(BLOG_ERROR, "DummyPRFileDesc_GlobalInit failed");
            goto fail3;
        }
        if (!BSocketPRFileDesc_GlobalInit()) {
            BLog(BLOG_ERROR, "BSocketPRFileDesc_GlobalInit failed");
            goto fail3;
        }
        
        // initialize NSS
        if (NSS_Init(options.nssdb) != SECSuccess) {
            BLog(BLOG_ERROR, "NSS_Init failed (%d)", (int)PR_GetError());
            goto fail3;
        }
        if (NSS_SetDomesticPolicy() != SECSuccess) {
            BLog(BLOG_ERROR, "NSS_SetDomesticPolicy failed (%d)", (int)PR_GetError());
            goto fail4;
        }
        if (SSL_ConfigServerSessionIDCache(0, 0, 0, NULL) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_ConfigServerSessionIDCache failed (%d)", (int)PR_GetError());
            goto fail4;
        }
        
        // open server certificate and private key
        if (!open_nss_cert_and_key(options.server_cert_name, &server_cert, &server_key)) {
            BLog(BLOG_ERROR, "Cannot open certificate and key");
            goto fail4;
        }
        
        // initialize model SSL fd
        DummyPRFileDesc_Create(&model_dprfd);
        if (!(model_prfd = SSL_ImportFD(NULL, &model_dprfd))) {
            BLog(BLOG_ERROR, "SSL_ImportFD failed");
            ASSERT_FORCE(PR_Close(&model_dprfd) == PR_SUCCESS)
            goto fail5;
        }
        
        // set server certificate
        if (SSL_ConfigSecureServer(model_prfd, server_cert, server_key, NSS_FindCertKEAType(server_cert)) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_ConfigSecureServer failed");
            goto fail6;
        }
    }
    
    // initialize number of clients
    clients_num = 0;
    
    // first client ID will be zero
    clients_lastid = 65535;
    
    // initialize clients linked list
    LinkedList2_Init(&clients);
    
    // initialize clients-by-id hash table
    brandom_randomize((uint8_t *)&clients_by_id_initval, sizeof(clients_by_id_initval));
    if (!HashTable_Init(
        &clients_by_id,
        OFFSET_DIFF(struct client_data, id, table_node_id),
        (HashTable_comparator)clients_by_id_key_comparator,
        (HashTable_hash_function)clients_by_id_hash_function,
        MAX_CLIENTS
    )) {
        BLog(BLOG_ERROR, "HashTable_Init failed");
        goto fail6;
    }
    
    // initialize listeners
    num_listeners = 0;
    for (int i = 0; i < num_listen_addrs; i++) {
        if (!Listener_Init(&listeners[num_listeners], &ss, listen_addrs[i], (Listener_handler)listener_handler, &listeners[num_listeners])) {
            BLog(BLOG_ERROR, "Listener_Init failed");
            goto fail7;
        }
        num_listeners++;
    }
    
    goto run_reactor;
    
    // cleanup on error
fail7:
    while (num_listeners > 0) {
        Listener_Free(&listeners[num_listeners - 1]);
        num_listeners--;
    }
    HashTable_Free(&clients_by_id);
fail6:
    if (options.ssl) {
        ASSERT_FORCE(PR_Close(model_prfd) == PR_SUCCESS)
fail5:
        CERT_DestroyCertificate(server_cert);
        SECKEY_DestroyPrivateKey(server_key);
fail4:
        ASSERT_FORCE(NSS_Shutdown() == SECSuccess)
fail3:
        ASSERT_FORCE(PR_Cleanup() == PR_SUCCESS)
        PL_ArenaFinish();
    }
    BSignal_RemoveHandler();
fail2a:
    BReactor_Free(&ss);
fail2:
    if (options.relay_predicate) {
        BPredicateFunction_Free(&relay_predicate_func_raddr);
        BPredicateFunction_Free(&relay_predicate_func_paddr);
        BPredicateFunction_Free(&relay_predicate_func_rname);
        BPredicateFunction_Free(&relay_predicate_func_pname);
        BPredicate_Free(&relay_predicate);
    }
fail1_1:
    if (options.comm_predicate) {
        BPredicateFunction_Free(&comm_predicate_func_p2addr);
        BPredicateFunction_Free(&comm_predicate_func_p1addr);
        BPredicateFunction_Free(&comm_predicate_func_p2name);
        BPredicateFunction_Free(&comm_predicate_func_p1name);
        BPredicate_Free(&comm_predicate);
    }
fail1:
    BLog(BLOG_ERROR, "initialization failed");
    BLog_Free();
fail0:
    DebugObjectGlobal_Finish();
    return 1;
    
run_reactor:
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
    
    // free clients
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&clients)) {
        struct client_data *client = UPPER_OBJECT(node, struct client_data, list_node);
        
        // remove outgoing flows
        LinkedList2Node *flow_node;
        while (flow_node = LinkedList2_GetFirst(&client->peer_out_flows_list)) {
            struct peer_flow *flow = UPPER_OBJECT(flow_node, struct peer_flow, src_list_node);
            ASSERT(flow->src_client == client)
            
            // allow freeing queue flows at dest
            PacketPassFairQueue_PrepareFree(&flow->dest_client->output_peers_fairqueue);
            
            // deallocate flow
            peer_flow_dealloc(flow);
        }
        
        // deallocate client
        client_dealloc(client);
    }
    
    // free listeners
    while (num_listeners > 0) {
        Listener_Free(&listeners[num_listeners - 1]);
        num_listeners--;
    }
    
    // free clients hash table
    HashTable_Free(&clients_by_id);
    
    if (options.ssl) {
        // free model
        ASSERT_FORCE(PR_Close(model_prfd) == PR_SUCCESS)
        
        // free certificate and private key
        CERT_DestroyCertificate(server_cert);
        SECKEY_DestroyPrivateKey(server_key);
        
        // free server cache
        SSL_ShutdownServerSessionIDCache();
        
        // free NSS
        ASSERT_FORCE(NSS_Shutdown() == SECSuccess)
        
        // free NSPR
        ASSERT_FORCE(PR_Cleanup() == PR_SUCCESS)
        PL_ArenaFinish();
    }
    
    // remove signal handler
    BSignal_RemoveHandler();
    
    // free relay predicate
    if (options.relay_predicate) {
        BPredicateFunction_Free(&relay_predicate_func_raddr);
        BPredicateFunction_Free(&relay_predicate_func_paddr);
        BPredicateFunction_Free(&relay_predicate_func_rname);
        BPredicateFunction_Free(&relay_predicate_func_pname);
        BPredicate_Free(&relay_predicate);
    }
    
    // free communication predicate
    if (options.comm_predicate) {
        BPredicateFunction_Free(&comm_predicate_func_p2addr);
        BPredicateFunction_Free(&comm_predicate_func_p1addr);
        BPredicateFunction_Free(&comm_predicate_func_p2name);
        BPredicateFunction_Free(&comm_predicate_func_p1name);
        BPredicate_Free(&comm_predicate);
    }
    
    // kill program dead variable
    DEAD_KILL(dead);
    
    // exit event loop
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
        "        [--listen-addr <addr>] ...\n"
        "        [--ssl --nssdb <string> --server-cert-name <string>]\n"
        "        [--comm-predicate <string>]\n"
        "        [--relay-predicate <string>]\n"
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
    options.server_cert_name = NULL;
    options.num_listen_addrs = 0;
    options.comm_predicate = NULL;
    options.relay_predicate = NULL;
    
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (!strcmp(arg, "--help")) {
            options.help = 1;
        }
        else if (!strcmp(arg, "--version")) {
            options.version = 1;
        }
        else if (!strcmp(arg, "--logger")) {
            if (i + 1 >= argc) {
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
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.logger_syslog_facility = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--syslog-ident")) {
            if (i + 1 >= argc) {
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
        else if (!strcmp(arg, "--server-cert-name")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.server_cert_name = argv[i + 1];
            i++;
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
        else if (!strcmp(arg, "--comm-predicate")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.comm_predicate = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--relay-predicate")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.relay_predicate = argv[i + 1];
            i++;
        }
        else {
            fprintf(stderr, "%s: unknown option\n", arg);
            return 0;
        }
    }
    
    if (options.help || options.version) {
        return 1;
    }
    
    if (!!options.nssdb != options.ssl) {
        fprintf(stderr, "--ssl and --nssdb must be used together\n");
        return 0;
    }
    
    if (!!options.server_cert_name != options.ssl) {
        fprintf(stderr, "--ssl and --server-cert-name must be used together\n");
        return 0;
    }
    
    return 1;
}

int resolve_arguments (void)
{
    // resolve listen addresses
    num_listen_addrs = 0;
    for (int i = 0; i < options.num_listen_addrs; i++) {
        if (!BAddr_Parse(&listen_addrs[num_listen_addrs], options.listen_addrs[i], NULL, 0)) {
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
    
    terminate();
}

void listener_handler (Listener *listener)
{
    if (clients_num >= MAX_CLIENTS) {
        BLog(BLOG_WARNING, "too many clients for new client");
        return;
    }
    
    // allocate the client structure
    struct client_data *client = malloc(sizeof(struct client_data));
    if (!client) {
        BLog(BLOG_ERROR, "failed to allocate client");
        return;
    }
    
    // accept it
    if (!Listener_Accept(listener, &client->sock, &client->addr)) {
        BLog(BLOG_NOTICE, "Listener_Accept failed");
        free(client);
        return;
    }
    
    client_add(client);
    return;
}

void client_add (struct client_data *client)
{
    ASSERT(clients_num < MAX_CLIENTS)
    
    // initialize dead variable
    DEAD_INIT(client->dead);
    
    if (options.ssl) {
        // create BSocket NSPR file descriptor
        BSocketPRFileDesc_Create(&client->bottom_prfd, &client->sock);
        
        // create SSL file descriptor from the socket's BSocketPRFileDesc
        if (!(client->ssl_prfd = SSL_ImportFD(model_prfd, &client->bottom_prfd))) {
            ASSERT_FORCE(PR_Close(&client->bottom_prfd) == PR_SUCCESS)
            goto fail0;
        }
        
        // set server mode
        if (SSL_ResetHandshake(client->ssl_prfd, PR_TRUE) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_ResetHandshake failed");
            goto fail1;
        }
        
        // set require client certificate
        if (SSL_OptionSet(client->ssl_prfd, SSL_REQUEST_CERTIFICATE, PR_TRUE) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_OptionSet(SSL_REQUEST_CERTIFICATE) failed");
            goto fail1;
        }
        if (SSL_OptionSet(client->ssl_prfd, SSL_REQUIRE_CERTIFICATE, PR_TRUE) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_OptionSet(SSL_REQUIRE_CERTIFICATE) failed");
            goto fail1;
        }
        
        // initialize BPRFileDesc on SSL file descriptor
        BPRFileDesc_Init(&client->ssl_bprfd, client->ssl_prfd);
        
        // set client state
        client->initstatus = INITSTATUS_HANDSHAKE;
    } else {
        // initialize i/o chains
        if (!client_init_io(client)) {
            goto fail0;
        }
        
        // set client state
        client->initstatus = INITSTATUS_WAITHELLO;
    }
    
    // start disconnect timer
    BTimer_Init(&client->disconnect_timer, CLIENT_NO_DATA_TIME_LIMIT, (BTimer_handler)client_disconnect_timer_handler, client);
    BReactor_SetTimer(&ss, &client->disconnect_timer);
    
    // assign ID
    // must be done before linking
    client->id = new_client_id();
    
    // link in
    clients_num++;
    LinkedList2_Append(&clients, &client->list_node);
    ASSERT_EXECUTE(HashTable_Insert(&clients_by_id, &client->table_node_id))
    
    // initialize peer flows from us list and tree (flows for sending messages to other clients)
    LinkedList2_Init(&client->peer_out_flows_list);
    BAVL_Init(&client->peer_out_flows_tree, OFFSET_DIFF(struct peer_flow, dest_client_id, src_tree_node), (BAVL_comparator)peerid_comparator, NULL);
    
    // set not dying
    client->dying = 0;
    
    client_log(client, BLOG_INFO, "initialized");
    
    // start I/O
    if (options.ssl) {
        // set read handler for driving handshake
        BPRFileDesc_AddEventHandler(&client->ssl_bprfd, PR_POLL_READ, (BPRFileDesc_handler)client_handshake_read_handler, client);
        
        // start handshake
        client_try_handshake(client);
        return;
    } else {
        return;
    }
    
    // cleanup on errors
fail1:
    if (options.ssl) {
        ASSERT_FORCE(PR_Close(client->ssl_prfd) == PR_SUCCESS)
    }
fail0:
    BSocket_Free(&client->sock);
    free(client);
}

void client_remove (struct client_data *client)
{
    ASSERT(!client->dying)
    
    client_log(client, BLOG_NOTICE, "removing");
    
    // set dying to prevent sending this client anything
    client->dying = 1;
    
    // remove outgoing flows and tell those who know about it that it's gone
    // (outgoing flow also means that the target knows the source)
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&client->peer_out_flows_list)) {
        struct peer_flow *flow = UPPER_OBJECT(node, struct peer_flow, src_list_node);
        ASSERT(flow->src_client == client)
        struct client_data *clientB = flow->dest_client;
        ASSERT(clientB->initstatus == INITSTATUS_COMPLETE)
        
        // remove the flow
        if (PacketPassFairQueueFlow_IsBusy(&flow->qflow)) {
            client_log(client, BLOG_DEBUG, "removing flow later");
            peer_flow_disconnect(flow);
            PacketPassFairQueueFlow_SetBusyHandler(&flow->qflow, (PacketPassFairQueue_handler_busy)peer_flow_handler_canremove, flow);
        } else {
            client_log(client, BLOG_DEBUG, "removing flow now");
            peer_flow_dealloc(flow);
        }
        
        // inform the other peer this client is gone
        if (!clientB->dying) {
            DEAD_ENTER(dead)
            client_send_endclient(clientB, client->id);
            if (DEAD_LEAVE(dead)) {
                return;
            }
        }
    }
    
    // deallocate client
    client_dealloc(client);
}

void client_dealloc (struct client_data *client)
{
    ASSERT(LinkedList2_IsEmpty(&client->peer_out_flows_list))
    
    // link out
    ASSERT_EXECUTE(HashTable_Remove(&clients_by_id, &client->id))
    LinkedList2_Remove(&clients, &client->list_node);
    clients_num--;
    
    // stop disconnect timer
    BReactor_RemoveTimer(&ss, &client->disconnect_timer);
    
    if (client->initstatus >= INITSTATUS_WAITHELLO) {
        // free I/O
        client_dealloc_io(client);
        
        // free common name
        if (options.ssl) {
            PORT_Free(client->common_name);
        }
    }
    
    // free SSL
    if (options.ssl) {
        // free BPRFileDesc
        BPRFileDesc_Free(&client->ssl_bprfd);
        
        // free SSL PRFD
        ASSERT_FORCE(PR_Close(client->ssl_prfd) == PR_SUCCESS)
    }
    
    // free socket
    BSocket_Free(&client->sock);
    
    // free dead variable
    DEAD_KILL(client->dead);
    
    // free memory
    free(client);
}

void client_log (struct client_data *client, int level, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    char addr[BADDR_MAX_PRINT_LEN];
    BAddr_Print(&client->addr, addr);
    BLog_Append("client %d (%s): ", (int)client->id, addr);
    BLog_LogToChannelVarArg(BLOG_CURRENT_CHANNEL, level, fmt, vl);
    va_end(vl);
}

void client_disconnect_timer_handler (struct client_data *client)
{
    client_log(client, BLOG_NOTICE, "timed out");
    
    client_remove(client);
    return;
}

void client_try_handshake (struct client_data *client)
{
    ASSERT(client->initstatus == INITSTATUS_HANDSHAKE)
    
    // attempt handshake
    if (SSL_ForceHandshake(client->ssl_prfd) != SECSuccess) {
        PRErrorCode error = PR_GetError();
        if (error == PR_WOULD_BLOCK_ERROR) {
            // try again on read event
            BPRFileDesc_EnableEvent(&client->ssl_bprfd, PR_POLL_READ);
            return;
        }
        client_log(client, BLOG_NOTICE, "SSL_ForceHandshake failed (%d)", (int)error);
        goto fail0;
    }
    
    client_log(client, BLOG_INFO, "handshake complete");
    
    // remove read handler
    BPRFileDesc_RemoveEventHandler(&client->ssl_bprfd, PR_POLL_READ);
    
    // get client certificate
    CERTCertificate *cert = SSL_PeerCertificate(client->ssl_prfd);
    if (!cert) {
        client_log(client, BLOG_ERROR, "SSL_PeerCertificate failed");
        goto fail0;
    }
    
    PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        client_log(client, BLOG_ERROR, "PORT_NewArena failed");
        goto fail1;
    }
    
    // encode certificate
    SECItem der;
    der.len = 0;
    der.data = NULL;
    if (!SEC_ASN1EncodeItem(arena, &der, cert, SEC_ASN1_GET(CERT_CertificateTemplate))) {
        client_log(client, BLOG_ERROR, "SEC_ASN1EncodeItem failed");
        goto fail2;
    }
    
    // store certificate
    if (der.len > (unsigned int)sizeof(client->cert)) {
        client_log(client, BLOG_NOTICE, "client certificate too big");
        goto fail2;
    }
    memcpy(client->cert, der.data, der.len);
    client->cert_len = der.len;
    
    // remember common name
    if (!(client->common_name = CERT_GetCommonName(&cert->subject))) {
        client_log(client, BLOG_NOTICE, "CERT_GetCommonName failed");
        goto fail2;
    }
    
    // init I/O chains
    if (!client_init_io(client)) {
        goto fail3;
    }
    
    PORT_FreeArena(arena, PR_FALSE);
    CERT_DestroyCertificate(cert);
    
    // set client state
    client->initstatus = INITSTATUS_WAITHELLO;
    
    return;
    
    // handle errors
fail3:
    PORT_Free(client->common_name);
fail2:
    PORT_FreeArena(arena, PR_FALSE);
fail1:
    CERT_DestroyCertificate(cert);
fail0:
    client_remove(client);
}

void client_handshake_read_handler (struct client_data *client, PRInt16 event)
{
    ASSERT(event == PR_POLL_READ)
    
    // restart no data timer
    BReactor_SetTimer(&ss, &client->disconnect_timer);
    
    // continue handshake
    client_try_handshake(client);
    return;
}

int client_init_io (struct client_data *client)
{
    // initialize error domain
    FlowErrorDomain_Init(&client->domain, (FlowErrorDomain_handler)client_error_handler, client);
    
    // init output common
    
    // init sink
    StreamPassInterface *sink_input;
    if (options.ssl) {
        PRStreamSink_Init(&client->output_sink.ssl, FlowErrorReporter_Create(&client->domain, COMPONENT_SINK), &client->ssl_bprfd);
        sink_input = PRStreamSink_GetInput(&client->output_sink.ssl);
    } else {
        StreamSocketSink_Init(&client->output_sink.plain, FlowErrorReporter_Create(&client->domain, COMPONENT_SINK), &client->sock);
        sink_input = StreamSocketSink_GetInput(&client->output_sink.plain);
    }
    
    // init sender
    PacketStreamSender_Init(&client->output_sender, sink_input, PACKETPROTO_ENCLEN(SC_MAX_ENC));
    
    // init queue
    PacketPassPriorityQueue_Init(&client->output_priorityqueue, PacketStreamSender_GetInput(&client->output_sender), BReactor_PendingGroup(&ss));
    
    // init output control flow
    
    // init queue flow
    PacketPassPriorityQueueFlow_Init(&client->output_control_qflow, &client->output_priorityqueue, -1);
    
    // init PacketProtoFlow
    if (!PacketProtoFlow_Init(
        &client->output_control_oflow,
        SC_MAX_ENC,
        CLIENT_CONTROL_BUFFER_MIN_PACKETS,
        PacketPassPriorityQueueFlow_GetInput(&client->output_control_qflow),
        BReactor_PendingGroup(&ss)
    )) {
        client_log(client, BLOG_ERROR, "PacketProtoFlow_Init failed");
        goto fail0;
    }
    client->output_control_input = PacketBufferAsyncInput_GetInput(&client->output_control_oflow.ainput);
    client->output_control_packet_len = -1;
    
    // init output peers flow
    
    // init queue flow
    // use lower priority than control flow (higher number)
    PacketPassPriorityQueueFlow_Init(&client->output_peers_qflow, &client->output_priorityqueue, 0);
    
    // init fair queue (for different peers)
    PacketPassFairQueue_Init(&client->output_peers_fairqueue, PacketPassPriorityQueueFlow_GetInput(&client->output_peers_qflow), BReactor_PendingGroup(&ss));
    
    // init list of flows
    LinkedList2_Init(&client->output_peers_flows);
    
    // init input
    // NOTE: input must be initialized after output is, otherwise we might receive a packet from the client
    // before output was started via the jobs system, and fail to send a response (because pending jobs are executed in
    // the order they are registered).
    
    // init source
    StreamRecvInterface *source_interface;
    if (options.ssl) {
        PRStreamSource_Init(&client->input_source.ssl, FlowErrorReporter_Create(&client->domain, COMPONENT_SOURCE), &client->ssl_bprfd);
        source_interface = PRStreamSource_GetOutput(&client->input_source.ssl);
    } else {
        StreamSocketSource_Init(&client->input_source.plain, FlowErrorReporter_Create(&client->domain, COMPONENT_SOURCE), &client->sock);
        source_interface = StreamSocketSource_GetOutput(&client->input_source.plain);
    }
    
    // init interface
    PacketPassInterface_Init(&client->input_interface, SC_MAX_ENC, (PacketPassInterface_handler_send)client_input_handler_send, client);
    
    // init decoder
    if (!PacketProtoDecoder_Init(
        &client->input_decoder,
        FlowErrorReporter_Create(&client->domain, COMPONENT_DECODER),
        source_interface,
        &client->input_interface,
        BReactor_PendingGroup(&ss)
    )) {
        client_log(client, BLOG_ERROR, "PacketProtoDecoder_Init failed");
        goto fail1;
    }
    
    return 1;
    
    // free input
fail1:
    PacketPassInterface_Free(&client->input_interface);
    if (options.ssl) {
        PRStreamSource_Free(&client->input_source.ssl);
    } else {
        StreamSocketSource_Free(&client->input_source.plain);
    }
    // free output peers flow
    PacketPassFairQueue_Free(&client->output_peers_fairqueue);
    PacketPassPriorityQueueFlow_Free(&client->output_peers_qflow);
    // free output control flow
    PacketProtoFlow_Free(&client->output_control_oflow);
fail0:
    PacketPassPriorityQueueFlow_Free(&client->output_control_qflow);
    // free output common
    PacketPassPriorityQueue_Free(&client->output_priorityqueue);
    PacketStreamSender_Free(&client->output_sender);
    if (options.ssl) {
        PRStreamSink_Free(&client->output_sink.ssl);
    } else {
        StreamSocketSink_Free(&client->output_sink.plain);
    }
    return 0;
}

void client_dealloc_io (struct client_data *client)
{
    // free input
    PacketProtoDecoder_Free(&client->input_decoder);
    PacketPassInterface_Free(&client->input_interface);
    if (options.ssl) {
        PRStreamSource_Free(&client->input_source.ssl);
    } else {
        StreamSocketSource_Free(&client->input_source.plain);
    }
    
    // allow freeing fair queue flows
    PacketPassFairQueue_PrepareFree(&client->output_peers_fairqueue);
    
    // remove flows to us
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&client->output_peers_flows)) {
        struct peer_flow *flow = UPPER_OBJECT(node, struct peer_flow, dest_list_node);
        ASSERT(flow->dest_client == client)
        peer_flow_dealloc(flow);
    }
    
    // allow freeing priority queue flows
    PacketPassPriorityQueue_PrepareFree(&client->output_priorityqueue);
    
    // free output peers flow
    PacketPassFairQueue_Free(&client->output_peers_fairqueue);
    PacketPassPriorityQueueFlow_Free(&client->output_peers_qflow);
    
    // free output control flow
    PacketProtoFlow_Free(&client->output_control_oflow);
    PacketPassPriorityQueueFlow_Free(&client->output_control_qflow);
    
    // free output common
    PacketPassPriorityQueue_Free(&client->output_priorityqueue);
    PacketStreamSender_Free(&client->output_sender);
    if (options.ssl) {
        PRStreamSink_Free(&client->output_sink.ssl);
    } else {
        StreamSocketSink_Free(&client->output_sink.plain);
    }
}

void client_error_handler (struct client_data *client, int component, const void *data)
{
    ASSERT(INITSTATUS_HASLINK(client->initstatus))
    
    switch (component) {
        case COMPONENT_SOURCE:
        case COMPONENT_SINK:
            client_log(client, BLOG_NOTICE, "BSocket error %d", BSocket_GetError(&client->sock));
            if (options.ssl) {
                client_log(client, BLOG_NOTICE, "NSPR error %d", (int)PR_GetError());
            }
            break;
        case COMPONENT_DECODER:
            client_log(client, BLOG_NOTICE, "decoder error %d", *((int *)data));
            break;
        default:
            ASSERT(0);
    }
    
    client_remove(client);
    return;
}

int client_start_control_packet (struct client_data *client, void **data, int len)
{
    ASSERT(INITSTATUS_HASLINK(client->initstatus))
    ASSERT(!client->dying)
    ASSERT(client->output_control_packet_len == -1)
    ASSERT(len >= 0)
    ASSERT(len <= SC_MAX_PAYLOAD);
    ASSERT(data || len == 0)
    
    // obtain location for writing the packet
    DEAD_ENTER(client->dead)
    int res = BestEffortPacketWriteInterface_Sender_StartPacket(client->output_control_input, &client->output_control_packet);
    if (DEAD_LEAVE(client->dead)) {
        return -1;
    }
    
    ASSERT(res == 0 || res == 1)
    
    if (!res) {
        // out of buffer, kill client
        client_log(client, BLOG_NOTICE, "out of control buffer, removing");
        client_remove(client);
        return -1;
    }
    
    client->output_control_packet_len = len;
    
    if (data) {
        *data = client->output_control_packet + sizeof(struct sc_header);
    }
    
    return 0;
}

int client_end_control_packet (struct client_data *client, uint8_t type)
{
    ASSERT(INITSTATUS_HASLINK(client->initstatus))
    ASSERT(!client->dying)
    ASSERT(client->output_control_packet_len >= 0)
    ASSERT(client->output_control_packet_len <= SC_MAX_PAYLOAD)
    
    // write header
    struct sc_header *header = (struct sc_header *)client->output_control_packet;
    header->type = type;
    
    // finish writing packet
    DEAD_ENTER(client->dead)
    BestEffortPacketWriteInterface_Sender_EndPacket(client->output_control_input, sizeof(struct sc_header) + client->output_control_packet_len);
    if (DEAD_LEAVE(client->dead)) {
        return -1;
    }
    
    client->output_control_packet_len = -1;
    
    return 0;
}

int client_input_handler_send (struct client_data *client, uint8_t *data, int data_len)
{
    ASSERT(INITSTATUS_HASLINK(client->initstatus))
    
    // restart no data timer
    BReactor_SetTimer(&ss, &client->disconnect_timer);
    
    if (data_len < sizeof(struct sc_header)) {
        client_log(client, BLOG_NOTICE, "packet too short");
        client_remove(client);
        return -1;
    }
    
    struct sc_header *header = (struct sc_header *)data;
    
    uint8_t *sc_data = data + sizeof(struct sc_header);
    int sc_data_len = data_len - sizeof(struct sc_header);
    
    #ifndef NDEBUG
    DEAD_ENTER(client->dead)
    #endif
    
    // perform action based on packet type
    switch (header->type) {
        case SCID_KEEPALIVE:
            client_log(client, BLOG_DEBUG, "received keep-alive");
            break;
        case SCID_CLIENTHELLO:
            process_packet_hello(client, sc_data, sc_data_len);
            break;
        case SCID_OUTMSG:
            process_packet_outmsg(client, sc_data, sc_data_len);
            break;
        default:
            client_log(client, BLOG_NOTICE, "unknown packet type %d, removing", (int)header->type);
            client_remove(client);
    }
    
    #ifndef NDEBUG
    if (DEAD_LEAVE(client->dead)) {
        return -1;
    }
    #endif
    
    return 1;
}

struct peer_flow * peer_flow_create (struct client_data *src_client, struct client_data *dest_client)
{
    ASSERT(dest_client->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!dest_client->dying)
    ASSERT(!BAVL_LookupExact(&src_client->peer_out_flows_tree, &dest_client->id))
    
    // allocate flow structure
    struct peer_flow *flow = malloc(sizeof(*flow));
    if (!flow) {
        goto fail0;
    }
    
    // set source and destination
    flow->src_client = src_client;
    flow->dest_client = dest_client;
    flow->dest_client_id = dest_client->id;
    
    // init dead variable
    DEAD_INIT(flow->dead);
    
    // add to source list and hash table
    LinkedList2_Append(&flow->src_client->peer_out_flows_list, &flow->src_list_node);
    ASSERT_EXECUTE(BAVL_Insert(&flow->src_client->peer_out_flows_tree, &flow->src_tree_node, NULL))
    
    // add to destination client list
    LinkedList2_Append(&flow->dest_client->output_peers_flows, &flow->dest_list_node);
    
    // initialize I/O
    PacketPassFairQueueFlow_Init(&flow->qflow, &flow->dest_client->output_peers_fairqueue);
    if (!PacketProtoFlow_Init(
        &flow->oflow,
        SC_MAX_ENC,
        CLIENT_PEER_FLOW_BUFFER_MIN_PACKETS,
        PacketPassFairQueueFlow_GetInput(&flow->qflow),
        BReactor_PendingGroup(&ss)
    )) {
        BLog(BLOG_ERROR, "PacketProtoFlow_Init failed");
        goto fail1;
    }
    flow->bepwi = PacketBufferAsyncInput_GetInput(&flow->oflow.ainput);
    flow->packet_len = -1;
    
    return flow;
    
fail1:
    PacketPassFairQueueFlow_Free(&flow->qflow);
    LinkedList2_Remove(&flow->dest_client->output_peers_flows, &flow->dest_list_node);
    BAVL_Remove(&flow->src_client->peer_out_flows_tree, &flow->src_tree_node);
    LinkedList2_Remove(&flow->src_client->peer_out_flows_list, &flow->src_list_node);
    free(flow);
fail0:
    return NULL;
}

void peer_flow_dealloc (struct peer_flow *flow)
{
    // free I/O
    PacketProtoFlow_Free(&flow->oflow);
    PacketPassFairQueueFlow_Free(&flow->qflow);
    
    // remove from destination client list
    LinkedList2_Remove(&flow->dest_client->output_peers_flows, &flow->dest_list_node);
    
    // remove from source list and hash table
    if (flow->src_client) {
        BAVL_Remove(&flow->src_client->peer_out_flows_tree, &flow->src_tree_node);
        LinkedList2_Remove(&flow->src_client->peer_out_flows_list, &flow->src_list_node);
    }
    
    // free dead variable
    DEAD_KILL(flow->dead);
    
    // free memory
    free(flow);
}

void peer_flow_disconnect (struct peer_flow *flow)
{
    ASSERT(flow->src_client)
    
    // remove from source list and hash table
    BAVL_Remove(&flow->src_client->peer_out_flows_tree, &flow->src_tree_node);
    LinkedList2_Remove(&flow->src_client->peer_out_flows_list, &flow->src_list_node);
    
    // set no source
    flow->src_client = NULL;
}

int peer_flow_start_packet (struct peer_flow *flow, void **data, int len)
{
    ASSERT(flow->dest_client->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!flow->dest_client->dying)
    ASSERT(flow->packet_len == -1)
    ASSERT(len >= 0)
    ASSERT(len <= SC_MAX_PAYLOAD)
    ASSERT(data || len == 0)
    
    // obtain location for writing the packet
    DEAD_ENTER(flow->dead)
    int res = BestEffortPacketWriteInterface_Sender_StartPacket(flow->bepwi, &flow->packet);
    if (DEAD_LEAVE(flow->dead)) {
        return -1;
    }
    
    ASSERT(res == 0 || res == 1)
    
    if (!res) {
        BLog(BLOG_INFO, "out of flow buffer");
        return 0;
    }
    
    flow->packet_len = len;
    
    if (data) {
        *data = flow->packet + sizeof(struct sc_header);
    }
    
    return 1;
}

int peer_flow_end_packet (struct peer_flow *flow, uint8_t type)
{
    ASSERT(flow->dest_client->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!flow->dest_client->dying)
    ASSERT(flow->packet_len >= 0)
    ASSERT(flow->packet_len <= SC_MAX_PAYLOAD)
    
    // write header
    struct sc_header *header = (struct sc_header *)flow->packet;
    header->type = type;
    
    // finish writing packet
    DEAD_ENTER(flow->dead)
    BestEffortPacketWriteInterface_Sender_EndPacket(flow->bepwi, sizeof(struct sc_header) + flow->packet_len);
    if (DEAD_LEAVE(flow->dead)) {
        return -1;
    }
    
    flow->packet_len = -1;
    
    return 0;
}

void peer_flow_handler_canremove (struct peer_flow *flow)
{
    ASSERT(!flow->src_client)
    
    client_log(flow->dest_client, BLOG_DEBUG, "removing old flow");
    
    peer_flow_dealloc(flow);
    return;
}

void process_packet_hello (struct client_data *client, uint8_t *data, int data_len)
{
    if (client->initstatus != INITSTATUS_WAITHELLO) {
        client_log(client, BLOG_NOTICE, "hello: not expected");
        client_remove(client);
        return;
    }
    
    if (data_len != sizeof(struct sc_client_hello)) {
        client_log(client, BLOG_NOTICE, "hello: invalid length");
        client_remove(client);
        return;
    }
    
    struct sc_client_hello *msg = (struct sc_client_hello *)data;
    uint16_t version = ltoh16(msg->version);
    
    if (version != SC_VERSION) {
        client_log(client, BLOG_NOTICE, "hello: unknown version");
        client_remove(client);
        return;
    }
    
    client_log(client, BLOG_INFO, "received hello");
    
    // set client state to complete
    client->initstatus = INITSTATUS_COMPLETE;
    
    // send hello
    struct sc_server_hello *pack;
    if (client_start_control_packet(client, (void **)&pack, sizeof(struct sc_server_hello)) < 0) {
        return;
    }
    pack->flags = htol16(0);
    pack->id = htol16(client->id);
    pack->clientAddr = (client->addr.type == BADDR_TYPE_IPV4 ? client->addr.ipv4.ip : 0);
    if (client_end_control_packet(client, SCID_SERVERHELLO) < 0) {
        return;
    }
    
    // send it the peer list and inform others
    publish_client(client);
    return;
}

void process_packet_outmsg (struct client_data *client, uint8_t *data, int data_len)
{
    if (client->initstatus != INITSTATUS_COMPLETE) {
        client_log(client, BLOG_NOTICE, "outmsg: not expected");
        client_remove(client);
        return;
    }
    
    if (data_len < sizeof(struct sc_client_outmsg)) {
        client_log(client, BLOG_NOTICE, "outmsg: wrong size");
        client_remove(client);
        return;
    }
    
    struct sc_client_outmsg *msg = (struct sc_client_outmsg *)data;
    peerid_t id = ltoh16(msg->clientid);
    int payload_size = data_len - sizeof(struct sc_client_outmsg);
    
    if (payload_size > SC_MAX_MSGLEN) {
        client_log(client, BLOG_NOTICE, "outmsg: too large payload");
        client_remove(client);
        return;
    }
    
    uint8_t *payload = data + sizeof(struct sc_client_outmsg);
    
    // lookup flow to destination client
    BAVLNode *node = BAVL_LookupExact(&client->peer_out_flows_tree, &id);
    if (!node) {
        client_log(client, BLOG_NOTICE, "no flow for message to %d", (int)id);
        return;
    }
    struct peer_flow *flow = UPPER_OBJECT(node, struct peer_flow, src_tree_node);
    
    // send packet
    struct sc_server_inmsg *pack;
    if (peer_flow_start_packet(flow, (void **)&pack, sizeof(struct sc_server_inmsg) + payload_size) <= 0) {
        return;
    }
    pack->clientid = htol16(client->id);
    memcpy((uint8_t *)pack + sizeof(struct sc_server_inmsg), payload, payload_size);
    if (peer_flow_end_packet(flow, SCID_INMSG) < 0) {
        return;
    }
    return;
}

int client_send_newclient (struct client_data *client, struct client_data *nc, int relay_server, int relay_client)
{
    int flags = 0;
    if (relay_server) {
        flags |= SCID_NEWCLIENT_FLAG_RELAY_SERVER;
    }
    if (relay_client) {
        flags |= SCID_NEWCLIENT_FLAG_RELAY_CLIENT;
    }
    
    struct sc_server_newclient *pack;
    if (client_start_control_packet(client, (void **)&pack, sizeof(struct sc_server_newclient) + (options.ssl ? nc->cert_len : 0)) < 0) {
        return -1;
    }
    pack->id = htol16(nc->id);
    pack->flags = htol16(flags);
    if (options.ssl) {
        memcpy(pack + 1, nc->cert, nc->cert_len);
    }
    if (client_end_control_packet(client, SCID_NEWCLIENT) < 0) {
        return -1;
    }
    
    return 0;
}

int client_send_endclient (struct client_data *client, peerid_t end_id)
{
    struct sc_server_endclient *pack;
    if (client_start_control_packet(client, (void **)&pack, sizeof(struct sc_server_endclient)) < 0) {
        return -1;
    }
    pack->id = htol16(end_id);
    if (client_end_control_packet(client, SCID_ENDCLIENT) < 0) {
        return -1;
    }
    
    return 0;
}

void connect_clients (struct client_data *clientA, struct client_data *clientB)
{
    ASSERT(clientA->initstatus == INITSTATUS_COMPLETE)
    ASSERT(clientB->initstatus == INITSTATUS_COMPLETE)
    ASSERT(clientA != clientB)
    
    if (!clients_allowed(clientA, clientB)) {
        return;
    }
    
    client_log(clientA, BLOG_DEBUG, "connecting %d", (int)clientB->id);
    
    // determine relay relations
    int relayAB = relay_allowed(clientA, clientB);
    int relayBA = relay_allowed(clientB, clientA);
    
    // tell clientB about clientA
    if (client_send_newclient(clientB, clientA, relayBA, relayAB) < 0) {
        return;
    }
    
    // create flow clientA -> clientB
    struct peer_flow *flowAB = peer_flow_create(clientA, clientB);
    if (!flowAB) {
        client_send_endclient(clientB, clientA->id);
        return;
    }
    
    // tell clientA about clientB
    if (client_send_newclient(clientA, clientB, relayAB, relayBA) < 0) {
        return;
    }
    
    // create flow clientB -> clientA
    struct peer_flow *flowBA = peer_flow_create(clientB, clientA);
    if (!flowBA) {
        if (client_send_endclient(clientA, clientB->id) < 0) {
            return;
        }
        peer_flow_dealloc(flowAB);
        client_send_endclient(clientB, clientA->id);
        return;
    }
}

int publish_client (struct client_data *client)
{
    ASSERT(client->initstatus == INITSTATUS_COMPLETE)
    
    // connect clients allowed to communicate
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &clients);
    LinkedList2Node *node;
    while (node = LinkedList2Iterator_Next(&it)) {
        struct client_data *client2 = UPPER_OBJECT(node, struct client_data, list_node);
        if (client2->initstatus != INITSTATUS_COMPLETE || client2 == client) {
            continue;
        }
        
        DEAD_ENTER_N(server, dead)
        DEAD_ENTER_N(client, client->dead)
        connect_clients(client, client2);
        if (DEAD_LEAVE_N(server, dead)) {
            DEAD_LEAVE_N(client, client->dead);
            return -1;
        }
        if (DEAD_LEAVE_N(client, client->dead)) {
            LinkedList2Iterator_Free(&it);
            return -1;
        }
    }
    
    return 0;
}

peerid_t new_client_id (void)
{
    ASSERT(clients_num < MAX_CLIENTS)
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients_lastid++;
        if (!find_client_by_id(clients_lastid)) {
            return clients_lastid;
        }
    }
    
    ASSERT(0)
    return 42;
}

struct client_data * find_client_by_id (peerid_t id)
{
    HashTableNode *node;
    if (!HashTable_Lookup(&clients_by_id, &id, &node)) {
        return NULL;
    }
    struct client_data *client = UPPER_OBJECT(node, struct client_data, table_node_id);
    ASSERT(client->id == id)
    
    return client;
}

int clients_by_id_key_comparator (peerid_t *id1, peerid_t *id2)
{
    return (*id1 == *id2);
}

int clients_by_id_hash_function (peerid_t *id, int modulo)
{
    return (jenkins_lookup2_hash((uint8_t *)id, sizeof(peerid_t), clients_by_id_initval) % modulo);
}

int clients_allowed (struct client_data *client1, struct client_data *client2)
{
    if (!options.comm_predicate) {
        return 1;
    }
    
    // set values to compare against
    if (!options.ssl) {
        comm_predicate_p1name = "";
        comm_predicate_p2name = "";
    } else {
        comm_predicate_p1name = client1->common_name;
        comm_predicate_p2name = client2->common_name;
    }
    BAddr_GetIPAddr(&client1->addr, &comm_predicate_p1addr);
    BAddr_GetIPAddr(&client2->addr, &comm_predicate_p2addr);
    
    return BPredicate_Eval(&comm_predicate);
}

int comm_predicate_func_p1name_cb (void *user, void **args)
{
    char *arg = args[0];
    
    return (!strcmp(arg, comm_predicate_p1name));
}

int comm_predicate_func_p2name_cb (void *user, void **args)
{
    char *arg = args[0];
    
    return (!strcmp(arg, comm_predicate_p2name));
}

int comm_predicate_func_p1addr_cb (void *user, void **args)
{
    char *arg = args[0];
    
    BIPAddr addr;
    if (!BIPAddr_Resolve(&addr, arg, 1)) {
        BLog(BLOG_WARNING, "failed to parse address");
        return 0;
    }
    
    return BIPAddr_Compare(&addr, &comm_predicate_p1addr);
}

int comm_predicate_func_p2addr_cb (void *user, void **args)
{
    char *arg = args[0];
    
    BIPAddr addr;
    if (!BIPAddr_Resolve(&addr, arg, 1)) {
        BLog(BLOG_WARNING, "failed to parse address");
        return 0;
    }
    
    return BIPAddr_Compare(&addr, &comm_predicate_p2addr);
}

int relay_allowed (struct client_data *client, struct client_data *relay)
{
    if (!options.relay_predicate) {
        return 0;
    }
    
    // set values to compare against
    if (!options.ssl) {
        relay_predicate_pname = "";
        relay_predicate_rname = "";
    } else {
        relay_predicate_pname = client->common_name;
        relay_predicate_rname = relay->common_name;
    }
    BAddr_GetIPAddr(&client->addr, &relay_predicate_paddr);
    BAddr_GetIPAddr(&relay->addr, &relay_predicate_raddr);
    
    return BPredicate_Eval(&relay_predicate);
}

int relay_predicate_func_pname_cb (void *user, void **args)
{
    char *arg = args[0];
    
    return (!strcmp(arg, relay_predicate_pname));
}

int relay_predicate_func_rname_cb (void *user, void **args)
{
    char *arg = args[0];
    
    return (!strcmp(arg, relay_predicate_rname));
}

int relay_predicate_func_paddr_cb (void *user, void **args)
{
    char *arg = args[0];
    
    BIPAddr addr;
    if (!BIPAddr_Resolve(&addr, arg, 1)) {
        BLog(BLOG_ERROR, "paddr: failed to parse address");
        return 0;
    }
    
    return BIPAddr_Compare(&addr, &relay_predicate_paddr);
}

int relay_predicate_func_raddr_cb (void *user, void **args)
{
    char *arg = args[0];
    
    BIPAddr addr;
    if (!BIPAddr_Resolve(&addr, arg, 1)) {
        BLog(BLOG_ERROR, "raddr: failed to parse address");
        return 0;
    }
    
    return BIPAddr_Compare(&addr, &relay_predicate_raddr);
}

int peerid_comparator (void *unused, peerid_t *p1, peerid_t *p2)
{
    if (*p1 < *p2) {
        return -1;
    }
    if (*p1 > *p2) {
        return 1;
    }
    return 0;
}
