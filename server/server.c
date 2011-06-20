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
#include <misc/offset.h>
#include <misc/nsskey.h>
#include <misc/byteorder.h>
#include <misc/loglevel.h>
#include <misc/loggers_string.h>
#include <predicate/BPredicate.h>
#include <base/DebugObject.h>
#include <base/BLog.h>
#include <system/BSignal.h>
#include <system/BTime.h>
#include <system/BNetwork.h>
#include <security/BRandom.h>
#include <nspr_support/DummyPRFileDesc.h>

#ifndef BADVPN_USE_WINAPI
#include <base/BLog_syslog.h>
#endif

#include <server/server.h>

#include <generated/blog_channel_server.h>

#define LOGGER_STDOUT 1
#define LOGGER_SYSLOG 2

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
    int client_sndbuf;
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
BListener listeners[MAX_LISTEN_ADDRS];
int num_listeners;

// number of connected clients
int clients_num;

// ID assigned to last connected client
peerid_t clients_nextid;

// clients list
LinkedList2 clients;

// clients tree (by ID)
BAVL clients_tree;

// prints help text to standard output
static void print_help (const char *name);

// prints program name and version to standard output
static void print_version (void);

// parses the command line
static int parse_arguments (int argc, char *argv[]);

// processes certain command line options
static int process_arguments (void);

// handler for program termination request
static void signal_handler (void *unused);

// listener handler, accepts new clients
static void listener_handler (BListener *listener);

// frees resources used by a client
static void client_dealloc (struct client_data *client);

// initializes the I/O porition of the client
static int client_init_io (struct client_data *client);

// deallocates the I/O portion of the client. Must have no outgoing flows.
static void client_dealloc_io (struct client_data *client);

// removes a client
static void client_remove (struct client_data *client);

// job to finish removal after clients are informed
static void client_dying_job (struct client_data *client);

// passes a message to the logger, prepending about the client
static void client_log (struct client_data *client, int level, const char *fmt, ...);

// client activity timer handler. Removes the client.
static void client_disconnect_timer_handler (struct client_data *client);

// BConnection handler
static void client_connection_handler (struct client_data *client, int event);

// BSSLConnection handler
static void client_sslcon_handler (struct client_data *client, int event);

// decoder handler
static void client_decoder_handler_error (struct client_data *client);

// provides a buffer for sending a control packet to the client
static int client_start_control_packet (struct client_data *client, void **data, int len);

// submits a packet written after client_start_control_packet
static void client_end_control_packet (struct client_data *client, uint8_t id);

// sends a newclient message to a client
static int client_send_newclient (struct client_data *client, struct client_data *nc, int relay_server, int relay_client);

// sends an endclient message to a client
static int client_send_endclient (struct client_data *client, peerid_t end_id);

// handler for packets received from the client
static void client_input_handler_send (struct client_data *client, uint8_t *data, int data_len);

// processes hello packets from clients
static void process_packet_hello (struct client_data *client, uint8_t *data, int data_len);

// processes outmsg packets from clients
static void process_packet_outmsg (struct client_data *client, uint8_t *data, int data_len);

// creates a peer flow
static struct peer_flow * peer_flow_create (struct client_data *src_client, struct client_data *dest_client);

// deallocates a peer flow
static void peer_flow_dealloc (struct peer_flow *flow);

// disconnects the source client from a peer flow
static void peer_flow_disconnect (struct peer_flow *flow);

// provides a buffer for sending a peer-to-peer packet
static int peer_flow_start_packet (struct peer_flow *flow, void **data, int len);

// submits a peer-to-peer packet written after peer_flow_start_packet
static void peer_flow_end_packet (struct peer_flow *flow, uint8_t type);

// handler called by the queue when a peer flow can be freed after its source has gone away
static void peer_flow_handler_canremove (struct peer_flow *flow);

// generates a client ID to be used for a newly connected client
static peerid_t new_client_id (void);

// finds a client by its ID
static struct client_data * find_client_by_id (peerid_t id);

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

static int create_know (struct client_data *from, struct client_data *to, int relay_server, int relay_client);
static void remove_know (struct peer_know *k);
static void know_inform_job_handler (struct peer_know *k);
static void uninform_know (struct peer_know *k);
static void know_uninform_job_handler (struct peer_know *k);

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
    if (!BSignal_Init(&ss, signal_handler, NULL)) {
        BLog(BLOG_ERROR, "BSignal_Init failed");
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
        if (!BSSLConnection_GlobalInit()) {
            BLog(BLOG_ERROR, "BSSLConnection_GlobalInit failed");
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
        
        // initialize server cache
        if (SSL_ConfigServerSessionIDCache(0, 0, 0, NULL) != SECSuccess) {
            BLog(BLOG_ERROR, "SSL_ConfigServerSessionIDCache failed (%d)", (int)PR_GetError());
            goto fail4;
        }
        
        // open server certificate and private key
        if (!open_nss_cert_and_key(options.server_cert_name, &server_cert, &server_key)) {
            BLog(BLOG_ERROR, "Cannot open certificate and key");
            goto fail4a;
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
    clients_nextid = 0;
    
    // initialize clients linked list
    LinkedList2_Init(&clients);
    
    // initialize clients tree
    BAVL_Init(&clients_tree, OFFSET_DIFF(struct client_data, id, tree_node), (BAVL_comparator)peerid_comparator, NULL);
    
    // initialize listeners
    num_listeners = 0;
    while (num_listeners < num_listen_addrs) {
        if (!BListener_Init(&listeners[num_listeners], listen_addrs[num_listeners], &ss, &listeners[num_listeners], (BListener_handler)listener_handler)) {
            BLog(BLOG_ERROR, "BListener_Init failed");
            goto fail7;
        }
        num_listeners++;
    }
    
    // enter event loop
    BLog(BLOG_NOTICE, "entering event loop");
    BReactor_Exec(&ss);
    
    // free clients
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&clients)) {
        struct client_data *client = UPPER_OBJECT(node, struct client_data, list_node);
        
        // remove outgoing knows
        LinkedList2Node *node2;
        while (node2 = LinkedList2_GetFirst(&client->know_out_list)) {
            struct peer_know *k = UPPER_OBJECT(node2, struct peer_know, from_node);
            remove_know(k);
        }
        
        // remove incoming knows
        LinkedList2Node *node3;
        while (node3 = LinkedList2_GetFirst(&client->know_in_list)) {
            struct peer_know *k = UPPER_OBJECT(node3, struct peer_know, to_node);
            remove_know(k);
        }
        
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
fail7:
    while (num_listeners > 0) {
        num_listeners--;
        BListener_Free(&listeners[num_listeners]);
    }
    
    if (options.ssl) {
fail6:
        ASSERT_FORCE(PR_Close(model_prfd) == PR_SUCCESS)
fail5:
        CERT_DestroyCertificate(server_cert);
        SECKEY_DestroyPrivateKey(server_key);
fail4a:
        ASSERT_FORCE(SSL_ShutdownServerSessionIDCache() == SECSuccess)
fail4:
        ASSERT_FORCE(NSS_Shutdown() == SECSuccess)
fail3:
        ASSERT_FORCE(PR_Cleanup() == PR_SUCCESS)
        PL_ArenaFinish();
    }
    
    BSignal_Finish();
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
    BLog(BLOG_NOTICE, "exiting");
    BLog_Free();
fail0:
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
        "        [--ssl --nssdb <string> --server-cert-name <string>]\n"
        "        [--comm-predicate <string>]\n"
        "        [--relay-predicate <string>]\n"
        "        [--client-sndbuf <bytes / 0>]\n"
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
    options.client_sndbuf = CLIENT_SOCKET_DEFAULT_SEND_BUFFER;
    
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
        else if (!strcmp(arg, "--client-sndbuf")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if ((options.client_sndbuf = atoi(argv[i + 1])) < 0) {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
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
    BReactor_Quit(&ss, 0);
}

void listener_handler (BListener *listener)
{
    if (clients_num == MAX_CLIENTS) {
        BLog(BLOG_WARNING, "too many clients for new client");
        goto fail0;
    }
    
    // allocate the client structure
    struct client_data *client = malloc(sizeof(*client));
    if (!client) {
        BLog(BLOG_ERROR, "failed to allocate client");
        goto fail0;
    }
    
    // accept connection
    if (!BConnection_Init(&client->con, BCONNECTION_SOURCE_LISTENER(listener, &client->addr), &ss, client, (BConnection_handler)client_connection_handler)) {
        BLog(BLOG_ERROR, "BConnection_Init failed");
        goto fail1;
    }
    
    // limit socket send buffer, else our scheduling is pointless
    if (!BConnection_SetSendBuffer(&client->con, options.client_sndbuf) < 0) {
        BLog(BLOG_WARNING, "BConnection_SetSendBuffer failed");
    }
    
    // assign ID
    client->id = new_client_id();
    
    // set no common name
    client->common_name = NULL;
    
    // now client_log() works
    
    // init connection interfaces
    BConnection_SendAsync_Init(&client->con);
    BConnection_RecvAsync_Init(&client->con);
    
    if (options.ssl) {
        // create bottom NSPR file descriptor
        if (!BSSLConnection_MakeBackend(&client->bottom_prfd, BConnection_SendAsync_GetIf(&client->con), BConnection_RecvAsync_GetIf(&client->con))) {
            client_log(client, BLOG_ERROR, "BSSLConnection_MakeBackend failed");
            goto fail2;
        }
        
        // create SSL file descriptor from the bottom NSPR file descriptor
        if (!(client->ssl_prfd = SSL_ImportFD(model_prfd, &client->bottom_prfd))) {
            client_log(client, BLOG_ERROR, "SSL_ImportFD failed");
            ASSERT_FORCE(PR_Close(&client->bottom_prfd) == PR_SUCCESS)
            goto fail2;
        }
        
        // set server mode
        if (SSL_ResetHandshake(client->ssl_prfd, PR_TRUE) != SECSuccess) {
            client_log(client, BLOG_ERROR, "SSL_ResetHandshake failed");
            goto fail3;
        }
        
        // set require client certificate
        if (SSL_OptionSet(client->ssl_prfd, SSL_REQUEST_CERTIFICATE, PR_TRUE) != SECSuccess) {
            client_log(client, BLOG_ERROR, "SSL_OptionSet(SSL_REQUEST_CERTIFICATE) failed");
            goto fail3;
        }
        if (SSL_OptionSet(client->ssl_prfd, SSL_REQUIRE_CERTIFICATE, PR_TRUE) != SECSuccess) {
            client_log(client, BLOG_ERROR, "SSL_OptionSet(SSL_REQUIRE_CERTIFICATE) failed");
            goto fail3;
        }
        
        // init SSL connection
        BSSLConnection_Init(&client->sslcon, client->ssl_prfd, 1, &ss, client, (BSSLConnection_handler)client_sslcon_handler);
    } else {
        // initialize I/O
        if (!client_init_io(client)) {
            goto fail2;
        }
    }
    
    // start disconnect timer
    BTimer_Init(&client->disconnect_timer, CLIENT_NO_DATA_TIME_LIMIT, (BTimer_handler)client_disconnect_timer_handler, client);
    BReactor_SetTimer(&ss, &client->disconnect_timer);
    
    // link in
    clients_num++;
    LinkedList2_Append(&clients, &client->list_node);
    ASSERT_EXECUTE(BAVL_Insert(&clients_tree, &client->tree_node, NULL))
    
    // init knowledge lists
    LinkedList2_Init(&client->know_out_list);
    LinkedList2_Init(&client->know_in_list);
    
    // initialize peer flows from us list and tree (flows for sending messages to other clients)
    LinkedList2_Init(&client->peer_out_flows_list);
    BAVL_Init(&client->peer_out_flows_tree, OFFSET_DIFF(struct peer_flow, dest_client_id, src_tree_node), (BAVL_comparator)peerid_comparator, NULL);
    
    // init dying
    client->dying = 0;
    BPending_Init(&client->dying_job, BReactor_PendingGroup(&ss), (BPending_handler)client_dying_job, client);
    
    // set state
    client->initstatus = (options.ssl ? INITSTATUS_HANDSHAKE : INITSTATUS_WAITHELLO);
    
    client_log(client, BLOG_INFO, "initialized");
    
    return;
    
    if (options.ssl) {
fail3:
        ASSERT_FORCE(PR_Close(client->ssl_prfd) == PR_SUCCESS)
    }
fail2:
    BConnection_RecvAsync_Free(&client->con);
    BConnection_SendAsync_Free(&client->con);
    BConnection_Free(&client->con);
fail1:
    free(client);
fail0:
    return;
}

void client_dealloc (struct client_data *client)
{
    ASSERT(LinkedList2_IsEmpty(&client->know_out_list))
    ASSERT(LinkedList2_IsEmpty(&client->know_in_list))
    ASSERT(LinkedList2_IsEmpty(&client->peer_out_flows_list))
    
    // free I/O
    if (client->initstatus >= INITSTATUS_WAITHELLO && !client->dying) {
        client_dealloc_io(client);
    }
    
    // free dying
    BPending_Free(&client->dying_job);
    
    // link out
    BAVL_Remove(&clients_tree, &client->tree_node);
    LinkedList2_Remove(&clients, &client->list_node);
    clients_num--;
    
    // stop disconnect timer
    BReactor_RemoveTimer(&ss, &client->disconnect_timer);
    
    // free SSL
    if (options.ssl) {
        BSSLConnection_Free(&client->sslcon);
        ASSERT_FORCE(PR_Close(client->ssl_prfd) == PR_SUCCESS)
    }
    
    // free common name
    if (client->common_name) {
        PORT_Free(client->common_name);
    }
    
    // free connection interfaces
    BConnection_RecvAsync_Free(&client->con);
    BConnection_SendAsync_Free(&client->con);
    
    // free connection
    BConnection_Free(&client->con);
    
    // free memory
    free(client);
}

int client_init_io (struct client_data *client)
{
    StreamPassInterface *send_if = (options.ssl ? BSSLConnection_GetSendIf(&client->sslcon) : BConnection_SendAsync_GetIf(&client->con));
    StreamRecvInterface *recv_if = (options.ssl ? BSSLConnection_GetRecvIf(&client->sslcon) : BConnection_RecvAsync_GetIf(&client->con));
    
    // init input
    
    // init interface
    PacketPassInterface_Init(&client->input_interface, SC_MAX_ENC, (PacketPassInterface_handler_send)client_input_handler_send, client, BReactor_PendingGroup(&ss));
    
    // init decoder
    if (!PacketProtoDecoder_Init(&client->input_decoder, recv_if, &client->input_interface, BReactor_PendingGroup(&ss), client,
        (PacketProtoDecoder_handler_error)client_decoder_handler_error
    )) {
        client_log(client, BLOG_ERROR, "PacketProtoDecoder_Init failed");
        goto fail1;
    }
    
    // init output common
    
    // init sender
    PacketStreamSender_Init(&client->output_sender, send_if, PACKETPROTO_ENCLEN(SC_MAX_ENC), BReactor_PendingGroup(&ss));
    
    // init queue
    PacketPassPriorityQueue_Init(&client->output_priorityqueue, PacketStreamSender_GetInput(&client->output_sender), BReactor_PendingGroup(&ss), 0);
    
    // init output control flow
    
    // init queue flow
    PacketPassPriorityQueueFlow_Init(&client->output_control_qflow, &client->output_priorityqueue, -1);
    
    // init PacketProtoFlow
    if (!PacketProtoFlow_Init(
        &client->output_control_oflow, SC_MAX_ENC, CLIENT_CONTROL_BUFFER_MIN_PACKETS,
        PacketPassPriorityQueueFlow_GetInput(&client->output_control_qflow), BReactor_PendingGroup(&ss)
    )) {
        client_log(client, BLOG_ERROR, "PacketProtoFlow_Init failed");
        goto fail2;
    }
    client->output_control_input = PacketProtoFlow_GetInput(&client->output_control_oflow);
    client->output_control_packet_len = -1;
    
    // init output peers flow
    
    // init queue flow
    // use lower priority than control flow (higher number)
    PacketPassPriorityQueueFlow_Init(&client->output_peers_qflow, &client->output_priorityqueue, 0);
    
    // init fair queue (for different peers)
    PacketPassFairQueue_Init(&client->output_peers_fairqueue, PacketPassPriorityQueueFlow_GetInput(&client->output_peers_qflow), BReactor_PendingGroup(&ss), 0, 1);
    
    // init list of flows
    LinkedList2_Init(&client->output_peers_flows);
    
    return 1;
    
fail2:
    PacketPassPriorityQueueFlow_Free(&client->output_control_qflow);
    // free output common
    PacketPassPriorityQueue_Free(&client->output_priorityqueue);
    PacketStreamSender_Free(&client->output_sender);
    // free input
    PacketProtoDecoder_Free(&client->input_decoder);
fail1:
    PacketPassInterface_Free(&client->input_interface);
    return 0;
}

void client_dealloc_io (struct client_data *client)
{
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
    
    // free input
    PacketProtoDecoder_Free(&client->input_decoder);
    PacketPassInterface_Free(&client->input_interface);
}

void client_remove (struct client_data *client)
{
    ASSERT(!client->dying)
    
    client_log(client, BLOG_INFO, "removing");
    
    // set dying to prevent sending this client anything
    client->dying = 1;
    
    // free I/O now, removing incoming flows
    if (client->initstatus >= INITSTATUS_WAITHELLO) {
        client_dealloc_io(client);
    }
    
    // remove outgoing knows
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&client->know_out_list)) {
        struct peer_know *k = UPPER_OBJECT(node, struct peer_know, from_node);
        remove_know(k);
    }
    
    // remove outgoing flows
    while (node = LinkedList2_GetFirst(&client->peer_out_flows_list)) {
        struct peer_flow *flow = UPPER_OBJECT(node, struct peer_flow, src_list_node);
        ASSERT(flow->src_client == client)
        ASSERT(flow->dest_client->initstatus == INITSTATUS_COMPLETE)
        ASSERT(!flow->dest_client->dying)
        
        if (PacketPassFairQueueFlow_IsBusy(&flow->qflow)) {
            client_log(client, BLOG_DEBUG, "removing flow to %d later", (int)flow->dest_client->id);
            peer_flow_disconnect(flow);
            PacketPassFairQueueFlow_SetBusyHandler(&flow->qflow, (PacketPassFairQueue_handler_busy)peer_flow_handler_canremove, flow);
        } else {
            client_log(client, BLOG_DEBUG, "removing flow to %d now", (int)flow->dest_client->id);
            peer_flow_dealloc(flow);
        }
    }
    
    // schedule job to finish removal after clients are informed
    BPending_Set(&client->dying_job);
    
    // inform other clients that 'client' is no more
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &client->know_in_list);
    while (node = LinkedList2Iterator_Next(&it)) {
        struct peer_know *k = UPPER_OBJECT(node, struct peer_know, to_node);
        uninform_know(k);
    }
}

void client_dying_job (struct client_data *client)
{
    ASSERT(client->dying)
    ASSERT(LinkedList2_IsEmpty(&client->know_in_list))
    
    client_dealloc(client);
    return;
}

void client_log (struct client_data *client, int level, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    char addr[BADDR_MAX_PRINT_LEN];
    BAddr_Print(&client->addr, addr);
    BLog_Append("client %d (%s)", (int)client->id, addr);
    if (client->common_name) {
        BLog_Append(" (%s)", client->common_name);
    }
    BLog_Append(": ");
    BLog_LogToChannelVarArg(BLOG_CURRENT_CHANNEL, level, fmt, vl);
    va_end(vl);
}

void client_disconnect_timer_handler (struct client_data *client)
{
    ASSERT(!client->dying)
    
    client_log(client, BLOG_INFO, "timed out");
    
    client_remove(client);
    return;
}

void client_connection_handler (struct client_data *client, int event)
{
    ASSERT(!client->dying)
    
    if (event == BCONNECTION_EVENT_RECVCLOSED) {
        client_log(client, BLOG_INFO, "connection closed");
    } else {
        client_log(client, BLOG_INFO, "connection error");
    }
    
    client_remove(client);
    return;
}

void client_sslcon_handler (struct client_data *client, int event)
{
    ASSERT(options.ssl)
    ASSERT(!client->dying)
    ASSERT(event == BSSLCONNECTION_EVENT_UP || event == BSSLCONNECTION_EVENT_ERROR)
    ASSERT(!(event == BSSLCONNECTION_EVENT_UP) || client->initstatus == INITSTATUS_HANDSHAKE)
    
    if (event == BSSLCONNECTION_EVENT_ERROR) {
        client_log(client, BLOG_ERROR, "SSL error");
        client_remove(client);
        return;
    }
    
    client_log(client, BLOG_INFO, "handshake complete");
    
    // get client certificate
    CERTCertificate *cert = SSL_PeerCertificate(client->ssl_prfd);
    if (!cert) {
        client_log(client, BLOG_ERROR, "SSL_PeerCertificate failed");
        goto fail0;
    }
    
    // remember common name
    if (!(client->common_name = CERT_GetCommonName(&cert->subject))) {
        client_log(client, BLOG_NOTICE, "CERT_GetCommonName failed");
        goto fail1;
    }
    
    // store certificate
    SECItem der = cert->derCert;
    if (der.len > sizeof(client->cert)) {
        client_log(client, BLOG_NOTICE, "client certificate too big");
        goto fail1;
    }
    memcpy(client->cert, der.data, der.len);
    client->cert_len = der.len;
    
    PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        client_log(client, BLOG_ERROR, "PORT_NewArena failed");
        goto fail1;
    }
    
    // encode certificate
    memset(&der, 0, sizeof(der));
    if (!SEC_ASN1EncodeItem(arena, &der, cert, SEC_ASN1_GET(CERT_CertificateTemplate))) {
        client_log(client, BLOG_ERROR, "SEC_ASN1EncodeItem failed");
        goto fail2;
    }
    
    // store re-encoded certificate (for compatibility with old clients)
    if (der.len > sizeof(client->cert_old)) {
        client_log(client, BLOG_NOTICE, "client certificate too big");
        goto fail2;
    }
    memcpy(client->cert_old, der.data, der.len);
    client->cert_old_len = der.len;
    
    // init I/O chains
    if (!client_init_io(client)) {
        goto fail2;
    }
    
    PORT_FreeArena(arena, PR_FALSE);
    CERT_DestroyCertificate(cert);
    
    // set client state
    client->initstatus = INITSTATUS_WAITHELLO;
    
    client_log(client, BLOG_INFO, "handshake complete");
    
    return;
    
    // handle errors
fail2:
    PORT_FreeArena(arena, PR_FALSE);
fail1:
    CERT_DestroyCertificate(cert);
fail0:
    client_remove(client);
}

void client_decoder_handler_error (struct client_data *client)
{
    ASSERT(INITSTATUS_HASLINK(client->initstatus))
    ASSERT(!client->dying)
    
    client_log(client, BLOG_ERROR, "decoder error");
    
    client_remove(client);
    return;
}

int client_start_control_packet (struct client_data *client, void **data, int len)
{
    ASSERT(len >= 0)
    ASSERT(len <= SC_MAX_PAYLOAD)
    ASSERT(!(len > 0) || data)
    ASSERT(INITSTATUS_HASLINK(client->initstatus))
    ASSERT(!client->dying)
    ASSERT(client->output_control_packet_len == -1)
    
    // obtain location for writing the packet
    if (!BufferWriter_StartPacket(client->output_control_input, &client->output_control_packet)) {
        // out of buffer, kill client
        client_log(client, BLOG_INFO, "out of control buffer, removing");
        client_remove(client);
        return -1;
    }
    
    client->output_control_packet_len = len;
    
    if (data) {
        *data = client->output_control_packet + sizeof(struct sc_header);
    }
    
    return 0;
}

void client_end_control_packet (struct client_data *client, uint8_t type)
{
    ASSERT(INITSTATUS_HASLINK(client->initstatus))
    ASSERT(!client->dying)
    ASSERT(client->output_control_packet_len >= 0)
    ASSERT(client->output_control_packet_len <= SC_MAX_PAYLOAD)
    
    // write header
    struct sc_header *header = (struct sc_header *)client->output_control_packet;
    header->type = htol8(type);
    
    // finish writing packet
    BufferWriter_EndPacket(client->output_control_input, sizeof(struct sc_header) + client->output_control_packet_len);
    
    client->output_control_packet_len = -1;
}

int client_send_newclient (struct client_data *client, struct client_data *nc, int relay_server, int relay_client)
{
    ASSERT(client->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!client->dying)
    ASSERT(nc->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!nc->dying)
    
    int flags = 0;
    if (relay_server) {
        flags |= SCID_NEWCLIENT_FLAG_RELAY_SERVER;
    }
    if (relay_client) {
        flags |= SCID_NEWCLIENT_FLAG_RELAY_CLIENT;
    }
    
    uint8_t *cert_data = NULL;
    int cert_len = 0;
    if (options.ssl) {
        cert_data = (client->version == SC_OLDVERSION ?  nc->cert_old : nc->cert);
        cert_len = (client->version == SC_OLDVERSION ?  nc->cert_old_len : nc->cert_len);
    }
    
    struct sc_server_newclient *pack;
    if (client_start_control_packet(client, (void **)&pack, sizeof(struct sc_server_newclient) + cert_len) < 0) {
        return -1;
    }
    pack->id = htol16(nc->id);
    pack->flags = htol16(flags);
    memcpy(pack + 1, cert_data, cert_len);
    client_end_control_packet(client, SCID_NEWCLIENT);
    
    return 0;
}

int client_send_endclient (struct client_data *client, peerid_t end_id)
{
    ASSERT(client->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!client->dying)
    
    struct sc_server_endclient *pack;
    if (client_start_control_packet(client, (void **)&pack, sizeof(struct sc_server_endclient)) < 0) {
        return -1;
    }
    pack->id = htol16(end_id);
    client_end_control_packet(client, SCID_ENDCLIENT);
    
    return 0;
}

void client_input_handler_send (struct client_data *client, uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= SC_MAX_ENC)
    ASSERT(INITSTATUS_HASLINK(client->initstatus))
    ASSERT(!client->dying)
    
    // accept packet
    PacketPassInterface_Done(&client->input_interface);
    
    // restart disconnect timer
    BReactor_SetTimer(&ss, &client->disconnect_timer);
    
    // parse header
    if (data_len < sizeof(struct sc_header)) {
        client_log(client, BLOG_NOTICE, "packet too short");
        client_remove(client);
        return;
    }
    struct sc_header *header = (struct sc_header *)data;
    data += sizeof(*header);
    data_len -= sizeof(*header);
    uint8_t type = ltoh8(header->type);
    
    ASSERT(data_len >= 0)
    ASSERT(data_len <= SC_MAX_PAYLOAD)
    
    // perform action based on packet type
    switch (type) {
        case SCID_KEEPALIVE:
            client_log(client, BLOG_DEBUG, "received keep-alive");
            return;
        case SCID_CLIENTHELLO:
            process_packet_hello(client, data, data_len);
            return;
        case SCID_OUTMSG:
            process_packet_outmsg(client, data, data_len);
            return;
        default:
            client_log(client, BLOG_NOTICE, "unknown packet type %d, removing", (int)type);
            client_remove(client);
            return;
    }
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
    client->version = ltoh16(msg->version);
    
    if (client->version != SC_VERSION && client->version != SC_OLDVERSION) {
        client_log(client, BLOG_NOTICE, "hello: unknown version");
        client_remove(client);
        return;
    }
    
    client_log(client, BLOG_INFO, "received hello");
    
    // set client state to complete
    client->initstatus = INITSTATUS_COMPLETE;
    
    // publish client
    for (LinkedList2Node *list_node = LinkedList2_GetFirst(&clients); list_node; list_node = LinkedList2Node_Next(list_node)) {
        struct client_data *client2 = UPPER_OBJECT(list_node, struct client_data, list_node);
        if (client2 == client || client2->initstatus != INITSTATUS_COMPLETE || client2->dying || !clients_allowed(client, client2)) {
            continue;
        }
        
        // determine relay relations
        int relay_to = relay_allowed(client, client2);
        int relay_from = relay_allowed(client2, client);
        
        if (!create_know(client, client2, relay_to, relay_from)) {
            client_log(client, BLOG_ERROR, "failed to allocate know to %d", (int)client2->id);
            goto fail;
        }
        
        if (!create_know(client2, client, relay_from, relay_to)) {
            client_log(client, BLOG_ERROR, "failed to allocate know from %d", (int)client2->id);
            goto fail;
        }
        
        // create flow from client to client2
        if (!peer_flow_create(client, client2)) {
            client_log(client, BLOG_ERROR, "failed to allocate flow to %d", (int)client2->id);
            goto fail;
        }
        
        // create flow from client2 to client
        if (!peer_flow_create(client2, client)) {
            client_log(client, BLOG_ERROR, "failed to allocate flow from %d", (int)client2->id);
            goto fail;
        }
    }
    
    // send hello
    struct sc_server_hello *pack;
    if (client_start_control_packet(client, (void **)&pack, sizeof(struct sc_server_hello)) < 0) {
        return;
    }
    pack->flags = htol16(0);
    pack->id = htol16(client->id);
    pack->clientAddr = (client->addr.type == BADDR_TYPE_IPV4 ? client->addr.ipv4.ip : hton32(0));
    client_end_control_packet(client, SCID_SERVERHELLO);
    
    return;
    
fail:
    client_remove(client);
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
        client_log(client, BLOG_INFO, "no flow for message to %d", (int)id);
        return;
    }
    struct peer_flow *flow = UPPER_OBJECT(node, struct peer_flow, src_tree_node);
    
    // send packet
    struct sc_server_inmsg *pack;
    if (!peer_flow_start_packet(flow, (void **)&pack, sizeof(struct sc_server_inmsg) + payload_size)) {
        return;
    }
    pack->clientid = htol16(client->id);
    memcpy((uint8_t *)(pack + 1), payload, payload_size);
    peer_flow_end_packet(flow, SCID_INMSG);
}

struct peer_flow * peer_flow_create (struct client_data *src_client, struct client_data *dest_client)
{
    ASSERT(src_client->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!src_client->dying)
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
    
    // add to source list and tree
    LinkedList2_Append(&flow->src_client->peer_out_flows_list, &flow->src_list_node);
    ASSERT_EXECUTE(BAVL_Insert(&flow->src_client->peer_out_flows_tree, &flow->src_tree_node, NULL))
    
    // add to destination client list
    LinkedList2_Append(&flow->dest_client->output_peers_flows, &flow->dest_list_node);
    
    // initialize I/O
    PacketPassFairQueueFlow_Init(&flow->qflow, &flow->dest_client->output_peers_fairqueue);
    if (!PacketProtoFlow_Init(
        &flow->oflow, SC_MAX_ENC, CLIENT_PEER_FLOW_BUFFER_MIN_PACKETS,
        PacketPassFairQueueFlow_GetInput(&flow->qflow), BReactor_PendingGroup(&ss)
    )) {
        BLog(BLOG_ERROR, "PacketProtoFlow_Init failed");
        goto fail1;
    }
    flow->input = PacketProtoFlow_GetInput(&flow->oflow);
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
    PacketPassFairQueueFlow_AssertFree(&flow->qflow);
    
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
    ASSERT(len >= 0)
    ASSERT(len <= SC_MAX_PAYLOAD)
    ASSERT(!(len > 0) || data)
    ASSERT(flow->dest_client->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!flow->dest_client->dying)
    ASSERT(flow->src_client->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!flow->src_client->dying)
    ASSERT(flow->packet_len == -1)
    
    // obtain location for writing the packet
    if (!BufferWriter_StartPacket(flow->input, &flow->packet)) {
        client_log(flow->src_client, BLOG_INFO, "out of flow buffer for message to %d", (int)flow->dest_client->id);
        return 0;
    }
    
    flow->packet_len = len;
    
    if (data) {
        *data = flow->packet + sizeof(struct sc_header);
    }
    
    return 1;
}

void peer_flow_end_packet (struct peer_flow *flow, uint8_t type)
{
    ASSERT(flow->packet_len >= 0)
    ASSERT(flow->packet_len <= SC_MAX_PAYLOAD)
    
    // write header
    struct sc_header *header = (struct sc_header *)flow->packet;
    header->type = type;
    
    // finish writing packet
    BufferWriter_EndPacket(flow->input, sizeof(struct sc_header) + flow->packet_len);
    
    flow->packet_len = -1;
}

void peer_flow_handler_canremove (struct peer_flow *flow)
{
    ASSERT(!flow->src_client)
    ASSERT(flow->dest_client->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!flow->dest_client->dying)
    
    client_log(flow->dest_client, BLOG_DEBUG, "removing old flow");
    
    peer_flow_dealloc(flow);
    return;
}

peerid_t new_client_id (void)
{
    ASSERT(clients_num < MAX_CLIENTS)
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        peerid_t id = clients_nextid++;
        if (!find_client_by_id(id)) {
            return id;
        }
    }
    
    ASSERT(0)
    return 42;
}

struct client_data * find_client_by_id (peerid_t id)
{
    BAVLNode *node;
    if (!(node = BAVL_LookupExact(&clients_tree, &id))) {
        return NULL;
    }
    
    return UPPER_OBJECT(node, struct client_data, tree_node);
}

int clients_allowed (struct client_data *client1, struct client_data *client2)
{
    ASSERT(client1->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!client1->dying)
    ASSERT(client2->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!client2->dying)
    
    if (!options.comm_predicate) {
        return 1;
    }
    
    // set values to compare against
    comm_predicate_p1name = (client1->common_name ? client1->common_name : "");
    comm_predicate_p2name = (client2->common_name ? client2->common_name : "");
    BAddr_GetIPAddr(&client1->addr, &comm_predicate_p1addr);
    BAddr_GetIPAddr(&client2->addr, &comm_predicate_p2addr);
    
    // evaluate predicate
    int res = BPredicate_Eval(&comm_predicate);
    if (res < 0) {
        return 0;
    }
    
    return res;
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
        return -1;
    }
    
    return BIPAddr_Compare(&addr, &comm_predicate_p1addr);
}

int comm_predicate_func_p2addr_cb (void *user, void **args)
{
    char *arg = args[0];
    
    BIPAddr addr;
    if (!BIPAddr_Resolve(&addr, arg, 1)) {
        BLog(BLOG_WARNING, "failed to parse address");
        return -1;
    }
    
    return BIPAddr_Compare(&addr, &comm_predicate_p2addr);
}

int relay_allowed (struct client_data *client, struct client_data *relay)
{
    if (!options.relay_predicate) {
        return 0;
    }
    
    // set values to compare against
    relay_predicate_pname = (client->common_name ? client->common_name : "");
    relay_predicate_rname = (relay->common_name ? relay->common_name : "");
    BAddr_GetIPAddr(&client->addr, &relay_predicate_paddr);
    BAddr_GetIPAddr(&relay->addr, &relay_predicate_raddr);
    
    // evaluate predicate
    int res = BPredicate_Eval(&relay_predicate);
    if (res < 0) {
        return 0;
    }
    
    return res;
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
        return -1;
    }
    
    return BIPAddr_Compare(&addr, &relay_predicate_paddr);
}

int relay_predicate_func_raddr_cb (void *user, void **args)
{
    char *arg = args[0];
    
    BIPAddr addr;
    if (!BIPAddr_Resolve(&addr, arg, 1)) {
        BLog(BLOG_ERROR, "raddr: failed to parse address");
        return -1;
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

int create_know (struct client_data *from, struct client_data *to, int relay_server, int relay_client)
{
    ASSERT(from->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!from->dying)
    ASSERT(to->initstatus == INITSTATUS_COMPLETE)
    ASSERT(!to->dying)
    
    // allocate structure
    struct peer_know *k = malloc(sizeof(*k));
    if (!k) {
        return 0;
    }
    
    // init arguments
    k->from = from;
    k->to = to;
    k->relay_server = relay_server;
    k->relay_client = relay_client;
    
    // append to lists
    LinkedList2_Append(&from->know_out_list, &k->from_node);
    LinkedList2_Append(&to->know_in_list, &k->to_node);
    
    // init and set inform job to inform client 'from' about client 'to'
    BPending_Init(&k->inform_job, BReactor_PendingGroup(&ss), (BPending_handler)know_inform_job_handler, k);
    BPending_Set(&k->inform_job);
    
    // init uninform job
    BPending_Init(&k->uninform_job, BReactor_PendingGroup(&ss), (BPending_handler)know_uninform_job_handler, k);
    
    return 1;
}

void remove_know (struct peer_know *k)
{
    // free uninform job
    BPending_Free(&k->uninform_job);
    
    // free inform job
    BPending_Free(&k->inform_job);
    
    // remove from lists
    LinkedList2_Remove(&k->to->know_in_list, &k->to_node);
    LinkedList2_Remove(&k->from->know_out_list, &k->from_node);
    
    // free structure
    free(k);
}

void know_inform_job_handler (struct peer_know *k)
{
    ASSERT(!k->from->dying)
    ASSERT(!k->to->dying)
    
    client_send_newclient(k->from, k->to, k->relay_server, k->relay_client);
    return;
}

void uninform_know (struct peer_know *k)
{
    ASSERT(!k->from->dying)
    ASSERT(k->to->dying)
    ASSERT(!BPending_IsSet(&k->uninform_job))
    
    // if 'from' has not been informed about 'to' yet, remove know, otherwise
    // schedule informing 'from' that 'to' is no more
    if (BPending_IsSet(&k->inform_job)) {
        remove_know(k);
    } else {
        BPending_Set(&k->uninform_job);
    }
}

void know_uninform_job_handler (struct peer_know *k)
{
    ASSERT(!k->from->dying)
    ASSERT(k->to->dying)
    ASSERT(!BPending_IsSet(&k->inform_job))
    
    struct client_data *from = k->from;
    struct client_data *to = k->to;
    
    // remove know
    remove_know(k);
    
    // uninform
    client_send_endclient(from, to->id);
}
