/**
 * @file tun2socks.c
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
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include <misc/version.h>
#include <misc/loggers_string.h>
#include <misc/loglevel.h>
#include <misc/minmax.h>
#include <misc/offset.h>
#include <misc/dead.h>
#include <structure/LinkedList2.h>
#include <system/BLog.h>
#include <system/BReactor.h>
#include <system/BSocket.h>
#include <system/BSignal.h>
#include <system/BAddr.h>
#include <flow/PacketBuffer.h>
#include <flow/BufferWriter.h>
#include <flow/SinglePacketBuffer.h>
#include <socksclient/BSocksClient.h>
#include <tuntap/BTap.h>
#include <lwip/init.h>
#include <lwip/tcp_impl.h>
#include <lwip/netif.h>
#include <lwip/tcp.h>

#ifndef BADVPN_USE_WINAPI
#include <system/BLog_syslog.h>
#endif

#include <tun2socks/tun2socks.h>

#include <generated/blog_channel_tun2socks.h>

#define LOGGER_STDOUT 1
#define LOGGER_SYSLOG 2

#define SYNC_DECL \
    BPending sync_mark; \

#define SYNC_FROMHERE \
    BPending_Init(&sync_mark, BReactor_PendingGroup(&ss), NULL, NULL); \
    BPending_Set(&sync_mark);

#define SYNC_BREAK \
    BPending_Free(&sync_mark);

#define SYNC_COMMIT \
    BReactor_Synchronize(&ss, &sync_mark); \
    BPending_Free(&sync_mark);

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
    char *tapdev;
    char *netif_ipaddr;
    char *netif_netmask;
    char *socks_server_addr;
} options;

// TCP client
struct tcp_client {
    dead_t dead;
    dead_t dead_client;
    LinkedList2Node list_node;
    BAddr local_addr;
    BAddr remote_addr;
    struct tcp_pcb *pcb;
    int client_closed;
    uint8_t buf[TCP_WND];
    int buf_used;
    BSocksClient socks_client;
    int socks_up;
    int socks_closed;
    StreamPassInterface *socks_send_if;
    int socks_send_prev_buf_used;
    BPending socks_send_finished_job;
    StreamRecvInterface *socks_recv_if;
    uint8_t socks_recv_buf[CLIENT_SOCKS_RECV_BUF_SIZE];
    int socks_recv_buf_used;
    int socks_recv_buf_sent;
    int socks_recv_waiting;
    int socks_recv_tcp_pending;
};

// IP address of netif
BIPAddr netif_ipaddr;

// netmask of netif
BIPAddr netif_netmask;

// SOCKS server address
BAddr socks_server_addr;

// reactor
BReactor ss;

// set to 1 by terminate
int quitting;

// TUN device
BTap device;

// device writing
BufferWriter device_write_writer;
PacketBuffer device_write_buffer;

// device reading
SinglePacketBuffer device_read_buffer;
PacketPassInterface device_read_interface;

// TCP timer
BTimer tcp_timer;

// job for initializing lwip
BPending lwip_init_job;

// lwip netif
int have_netif;
struct netif netif;

// lwip TCP listener
struct tcp_pcb *listener;

// TCP clients
LinkedList2 tcp_clients;

// number of clients
int num_clients;

static void terminate (void);
static void print_help (const char *name);
static void print_version (void);
static int parse_arguments (int argc, char *argv[]);
static int process_arguments (void);
static void signal_handler (void *unused);
static void lwip_init_job_hadler (void *unused);
static void tcp_timer_handler (void *unused);
static void device_error_handler (void *unused);
static void device_read_handler_send (void *unused, uint8_t *data, int data_len);
static err_t netif_init_func (struct netif *netif);
static err_t netif_output_func (struct netif *netif, struct pbuf *p, ip_addr_t *ipaddr);
static void client_log (struct tcp_client *client, int level, const char *fmt, ...);
static err_t listener_accept_func (void *arg, struct tcp_pcb *newpcb, err_t err);
static void client_handle_freed_client (struct tcp_client *client, int was_abrt);
static int client_free_client (struct tcp_client *client);
static void client_abort_client (struct tcp_client *client);
static void client_free_socks (struct tcp_client *client);
static void client_murder (struct tcp_client *client);
static void client_dealloc (struct tcp_client *client);
static void client_err_func (void *arg, err_t err);
static err_t client_recv_func (void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void client_socks_handler (struct tcp_client *client, int event);
static void client_send_to_socks (struct tcp_client *client);
static void client_socks_send_handler_done (struct tcp_client *client, int data_len);
static void client_socks_send_finished_job_handler (struct tcp_client *client);
static void client_socks_recv_initiate (struct tcp_client *client);
static void client_socks_recv_handler_done (struct tcp_client *client, int data_len);
static int client_socks_recv_send_out (struct tcp_client *client);
static err_t client_sent_func (void *arg, struct tcp_pcb *tpcb, u16_t len);

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
    
    // initialize sockets
    if (BSocket_GlobalInit() < 0) {
        BLog(BLOG_ERROR, "BSocket_GlobalInit failed");
        goto fail1;
    }
    
    // process arguments
    if (!process_arguments()) {
        BLog(BLOG_ERROR, "Failed to process arguments");
        goto fail1;
    }
    
    // init time
    BTime_Init();
    
    // init reactor
    if (!BReactor_Init(&ss)) {
        BLog(BLOG_ERROR, "BReactor_Init failed");
        goto fail1;
    }
    
    // set not quitting
    quitting = 0;
    
    // setup signal handler
    if (!BSignal_Init()) {
        BLog(BLOG_ERROR, "BSignal_Init failed");
        goto fail2;
    }
    BSignal_Capture();
    if (!BSignal_SetHandler(&ss, signal_handler, NULL)) {
        BLog(BLOG_ERROR, "BSignal_SetHandler failed");
        goto fail2;
    }
    
    // init TUN device
    if (!BTap_Init(&device, &ss, options.tapdev, device_error_handler, NULL, 1)) {
        BLog(BLOG_ERROR, "BTap_Init failed");
        goto fail3;
    }
    
    // NOTE: the order of the following is important:
    // first device writing must evaluate,
    // then lwip (so it can send packets to the device),
    // then device reading (so it can pass received packets to lwip).
    
    // init device reading
    PacketPassInterface_Init(&device_read_interface, BTap_GetMTU(&device), device_read_handler_send, NULL, BReactor_PendingGroup(&ss));
    if (!SinglePacketBuffer_Init(&device_read_buffer, BTap_GetOutput(&device), &device_read_interface, BReactor_PendingGroup(&ss))) {
        BLog(BLOG_ERROR, "SinglePacketBuffer_Init failed");
        goto fail4;
    }
    
    // init lwip init job
    BPending_Init(&lwip_init_job, BReactor_PendingGroup(&ss), lwip_init_job_hadler, NULL);
    BPending_Set(&lwip_init_job);
    
    // init device writing
    BufferWriter_Init(&device_write_writer, BTap_GetMTU(&device), BReactor_PendingGroup(&ss));
    if (!PacketBuffer_Init(&device_write_buffer, BufferWriter_GetOutput(&device_write_writer), BTap_GetInput(&device), DEVICE_WRITE_BUFFER_SIZE, BReactor_PendingGroup(&ss))) {
        BLog(BLOG_ERROR, "PacketBuffer_Init failed");
        goto fail5;
    }
    
    // init TCP timer
    // it won't trigger before lwip is initialized, becuase the lwip init is a job
    BTimer_Init(&tcp_timer, TCP_TMR_INTERVAL, tcp_timer_handler, NULL);
    BReactor_SetTimer(&ss, &tcp_timer);
    
    // set no netif
    have_netif = 0;
    
    // set no listener
    listener = NULL;
    
    // init clients list
    LinkedList2_Init(&tcp_clients);
    
    // init number of clients
    num_clients = 0;
    
    goto event_loop;
    
fail5:
    BufferWriter_Free(&device_write_writer);
    BPending_Free(&lwip_init_job);
    SinglePacketBuffer_Free(&device_read_buffer);
fail4:
    PacketPassInterface_Free(&device_read_interface);
    BTap_Free(&device);
fail3:
    BSignal_RemoveHandler();
fail2:
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
    
    // free clients
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&tcp_clients)) {
        struct tcp_client *client = UPPER_OBJECT(node, struct tcp_client, list_node);
        
        client_log(client, BLOG_INFO, "killing");
        
        client_murder(client);
    }
    
    // free listener
    if (listener) {
        tcp_close(listener);
    }
    
    // free netif
    if (have_netif) {
        netif_remove(&netif);
    }
    
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
    ASSERT(!quitting)
    
    BLog(BLOG_NOTICE, "tearing down");
    
    // free TCP timer
    BReactor_RemoveTimer(&ss, &tcp_timer);
    
    // free device writing
    PacketBuffer_Free(&device_write_buffer);
    BufferWriter_Free(&device_write_writer);
    
    // free lwip init job
    BPending_Free(&lwip_init_job);
    
    // free device reading
    SinglePacketBuffer_Free(&device_read_buffer);
    PacketPassInterface_Free(&device_read_interface);
    
    // free device
    BTap_Free(&device);
    
    // remove signal handler
    BSignal_RemoveHandler();
    
    // set quitting
    quitting = 1;
    
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
        "        [--tapdev <name>]\n"
        "        --netif-ipaddr <ipaddr>\n"
        "        --netif-netmask <ipnetmask>\n"
        "        --socks-server-addr <addr>\n"
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
    options.tapdev = NULL;
    options.netif_ipaddr = NULL;
    options.netif_netmask = NULL;
    options.socks_server_addr = NULL;
    
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
        else if (!strcmp(arg, "--tapdev")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.tapdev = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--netif-ipaddr")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.netif_ipaddr = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--netif-netmask")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.netif_netmask = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--socks-server-addr")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.socks_server_addr = argv[i + 1];
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
    
    if (!options.netif_ipaddr) {
        fprintf(stderr, "--netif-ipaddr is required\n");
        return 0;
    }
    
    if (!options.netif_netmask) {
        fprintf(stderr, "--netif-netmask is required\n");
        return 0;
    }
    
    if (!options.socks_server_addr) {
        fprintf(stderr, "--socks-server-addr is required\n");
        return 0;
    }
    
    return 1;
}

int process_arguments (void)
{
    // resolve netif ipaddr
    if (!BIPAddr_Resolve(&netif_ipaddr, options.netif_ipaddr, 0)) {
        BLog(BLOG_ERROR, "netif ipaddr: BIPAddr_Resolve failed");
        return 0;
    }
    if (netif_ipaddr.type != BADDR_TYPE_IPV4) {
        BLog(BLOG_ERROR, "netif ipaddr: must be an IPv4 address");
        return 0;
    }
    
    // resolve netif netmask
    if (!BIPAddr_Resolve(&netif_netmask, options.netif_netmask, 0)) {
        BLog(BLOG_ERROR, "netif netmask: BIPAddr_Resolve failed");
        return 0;
    }
    if (netif_netmask.type != BADDR_TYPE_IPV4) {
        BLog(BLOG_ERROR, "netif netmask: must be an IPv4 address");
        return 0;
    }
    
    // resolve SOCKS server address
    if (!BAddr_Parse2(&socks_server_addr, options.socks_server_addr, NULL, 0, 0)) {
        BLog(BLOG_ERROR, "socks server addr: BAddr_Parse2 failed");
        return 0;
    }
    
    return 1;
}

void signal_handler (void *unused)
{
    ASSERT(!quitting)
    
    BLog(BLOG_NOTICE, "termination requested");
    
    terminate();
    return;
}

void lwip_init_job_hadler (void *unused)
{
    ASSERT(!quitting)
    ASSERT(netif_ipaddr.type == BADDR_TYPE_IPV4)
    ASSERT(netif_netmask.type == BADDR_TYPE_IPV4)
    ASSERT(!have_netif)
    
    BLog(BLOG_DEBUG, "lwip init");
    
    // NOTE: the device may fail during this, but there's no harm in not checking
    // for that at every step
    
    int res;
    
    // init lwip
    lwip_init();
    
    // make addresses for netif
    ip_addr_t addr;
    addr.addr = netif_ipaddr.ipv4;
    ip_addr_t netmask;
    netmask.addr = netif_netmask.ipv4;
    ip_addr_t gw;
    ip_addr_set_any(&gw);
    
    // init netif
    if (!netif_add(&netif, &addr, &netmask, &gw, NULL, netif_init_func, ip_input)) {
        BLog(BLOG_ERROR, "netif_add failed");
        goto fail;
    }
    have_netif = 1;
    
    // set netif up
    netif_set_up(&netif);
    
    // set netif pretend TCP
    netif_set_pretend_tcp(&netif, 1);
    
    // init listener
    struct tcp_pcb *l = tcp_new();
    if (!l) {
        BLog(BLOG_ERROR, "tcp_new failed");
        goto fail;
    }
    
    // bind listener
    if (tcp_bind_to_netif(l, "ho0") != ERR_OK) {
        BLog(BLOG_ERROR, "tcp_bind_to_netif failed");
        tcp_close(l);
        goto fail;
    }
    
    // listen listener
    struct tcp_pcb *l2 = tcp_listen(l);
    if (!l2) {
        BLog(BLOG_ERROR, "tcp_listen failed");
        tcp_close(l);
        goto fail;
    }
    listener = l2;
    
    // setup accept handler
    tcp_accept(listener, listener_accept_func);
    
    return;
    
fail:
    if (!quitting) {
        terminate();
    }
}

void tcp_timer_handler (void *unused)
{
    ASSERT(!quitting)
    
    BLog(BLOG_DEBUG, "TCP timer");
    
    // schedule next timer
    // TODO: calculate timeout so we don't drift
    BReactor_SetTimer(&ss, &tcp_timer);
    
    tcp_tmr();
    return;
}

void device_error_handler (void *unused)
{
    ASSERT(!quitting)
    
    BLog(BLOG_ERROR, "device error");
    
    terminate();
    return;
}

void device_read_handler_send (void *unused, uint8_t *data, int data_len)
{
    ASSERT(!quitting)
    
    BLog(BLOG_DEBUG, "device: received packet");
    
    // accept packet
    PacketPassInterface_Done(&device_read_interface);
    
    // obtain pbuf
    struct pbuf *p = pbuf_alloc(PBUF_RAW, data_len, PBUF_POOL);
    if (!p) {
        BLog(BLOG_WARNING, "device read: pbuf_alloc failed");
        return;
    }
    
    // write packet to pbuf
    ASSERT_FORCE(pbuf_take(p, data, data_len) == ERR_OK)
    
    // pass pbuf to input
    if (netif.input(p, &netif) != ERR_OK) {
        BLog(BLOG_WARNING, "device read: input failed");
        pbuf_free(p);
    }
}

err_t netif_init_func (struct netif *netif)
{
    BLog(BLOG_DEBUG, "netif func init");
    
    netif->name[0] = 'h';
    netif->name[1] = 'o';
    netif->output = netif_output_func;
    
    return ERR_OK;
}

err_t netif_output_func (struct netif *netif, struct pbuf *p, ip_addr_t *ipaddr)
{
    SYNC_DECL
    
    BLog(BLOG_DEBUG, "device write: send packet");
    
    if (quitting) {
        return ERR_OK;
    }
    
    uint8_t *out;
    if (!BufferWriter_StartPacket(&device_write_writer, &out)) {
        DEBUG("netif func output: BufferWriter_StartPacket failed");
        return ERR_OK;
    }
    
    int len = 0;
    while (p) {
        int remain = BTap_GetMTU(&device) - len;
        if (p->len > remain) {
            BLog(BLOG_WARNING, "netif func output: no space left");
            break;
        }
        memcpy(out + len, p->payload, p->len);
        len += p->len;
        p = p->next;
    }
    
    SYNC_FROMHERE
    BufferWriter_EndPacket(&device_write_writer, len);
    SYNC_COMMIT
    
    return ERR_OK;
}

void client_log (struct tcp_client *client, int level, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    
    char local_addr_s[BADDR_MAX_PRINT_LEN];
    BAddr_Print(&client->local_addr, local_addr_s);
    
    char remote_addr_s[BADDR_MAX_PRINT_LEN];
    BAddr_Print(&client->remote_addr, remote_addr_s);
    
    BLog_Append("%05d (%s %s): ", num_clients, local_addr_s, remote_addr_s);
    BLog_LogToChannelVarArg(BLOG_CURRENT_CHANNEL, level, fmt, vl);
    
    va_end(vl);
}

err_t listener_accept_func (void *arg, struct tcp_pcb *newpcb, err_t err)
{
    ASSERT(listener)
    ASSERT(err == ERR_OK)
    
    // signal accepted
    tcp_accepted(listener);
    
    // allocate client structure
    struct tcp_client *client = malloc(sizeof(*client));
    if (!client) {
        BLog(BLOG_ERROR, "listener accept: malloc failed");
        return ERR_MEM;
    }
    
    SYNC_DECL
    SYNC_FROMHERE
    
    // init SOCKS
    BAddr addr;
    BAddr_InitIPv4(&addr, newpcb->local_ip.addr, hton16(newpcb->local_port));
    #ifdef OVERRIDE_DEST_ADDR
    ASSERT_FORCE(BAddr_Parse2(&addr, OVERRIDE_DEST_ADDR, NULL, 0, 1))
    #endif
    if (!BSocksClient_Init(&client->socks_client, socks_server_addr, addr, (BSocksClient_handler)client_socks_handler, client, &ss)) {
        BLog(BLOG_ERROR, "listener accept: BSocksClient_Init failed");
        SYNC_BREAK
        free(client);
        return ERR_MEM;
    }
    
    // init dead vars
    DEAD_INIT(client->dead);
    DEAD_INIT(client->dead_client);
    
    // add to linked list
    LinkedList2_Append(&tcp_clients, &client->list_node);
    
    // increment counter
    ASSERT(num_clients >= 0)
    num_clients++;
    
    // set pcb
    client->pcb = newpcb;
    
    // set client not closed
    client->client_closed = 0;
    
    // read addresses
    BAddr_InitIPv4(&client->local_addr, client->pcb->local_ip.addr, hton16(client->pcb->local_port));
    BAddr_InitIPv4(&client->remote_addr, client->pcb->remote_ip.addr, hton16(client->pcb->remote_port));
    
    // setup handler argument
    tcp_arg(client->pcb, client);
    
    // setup handlers
    tcp_err(client->pcb, client_err_func);
    tcp_recv(client->pcb, client_recv_func);
    
    // setup buffer
    client->buf_used = 0;
    
    // set SOCKS not up, not closed
    client->socks_up = 0;
    client->socks_closed = 0;
    
    client_log(client, BLOG_INFO, "accepted");
    
    DEAD_ENTER(client->dead_client)
    SYNC_COMMIT
    if (DEAD_LEAVE(client->dead_client) == -1) {
        return ERR_ABRT;
    }
    
    return ERR_OK;
}

void client_handle_freed_client (struct tcp_client *client, int was_abrt)
{
    ASSERT(!client->client_closed)
    ASSERT(was_abrt == 0 || was_abrt == 1)
    
    // pcb was taken care of by the caller
    
    // kill client dead var
    DEAD_KILL_WITH(client->dead_client, (was_abrt ? -1 : 1));
    
    // set client closed
    client->client_closed = 1;
    
    // if we have data to be sent to SOCKS and can send it, keep sending
    if (client->buf_used > 0 && !client->socks_closed) {
        client_log(client, BLOG_INFO, "waiting untill buffered data is sent to SOCKS");
    } else {
        if (!client->socks_closed) {
            client_free_socks(client);
        } else {
            client_dealloc(client);
        }
    }
}

int client_free_client (struct tcp_client *client)
{
    ASSERT(!client->client_closed)
    
    int was_abrt = 0;
    
    // remove callbacks
    tcp_err(client->pcb, NULL);
    tcp_recv(client->pcb, NULL);
    tcp_sent(client->pcb, NULL);
    
    // free pcb
    err_t err = tcp_close(client->pcb);
    if (err != ERR_OK) {
        client_log(client, BLOG_ERROR, "tcp_close failed (%d)", err);
        
        tcp_abort(client->pcb);
        was_abrt = 1;
    }
    
    client_handle_freed_client(client, was_abrt);
    
    return was_abrt;
}

void client_abort_client (struct tcp_client *client)
{
    ASSERT(!client->client_closed)
    
    // remove callbacks
    tcp_err(client->pcb, NULL);
    tcp_recv(client->pcb, NULL);
    tcp_sent(client->pcb, NULL);
    
    // free pcb
    tcp_abort(client->pcb);
    
    client_handle_freed_client(client, 1);
}

void client_free_socks (struct tcp_client *client)
{
    ASSERT(!client->socks_closed)
    
    // stop sending to SOCKS
    if (client->socks_up) {
        // remove send finished job
        BPending_Free(&client->socks_send_finished_job);
        
        // stop receiving from client
        if (!client->client_closed) {
            tcp_recv(client->pcb, NULL);
        }
    }
    
    // free SOCKS
    BSocksClient_Free(&client->socks_client);
    
    // set SOCKS closed
    client->socks_closed = 1;
    
    // if we have data to be sent to the client and we can send it, keep sending
    if (client->socks_up && (client->socks_recv_buf_used >= 0 || client->socks_recv_tcp_pending > 0) && !client->client_closed) {
        client_log(client, BLOG_INFO, "waiting until buffered data is sent to client");
    } else {
        if (!client->client_closed) {
            client_free_client(client);
        } else {
            client_dealloc(client);
        }
    }
}

void client_murder (struct tcp_client *client)
{
    // free client
    if (!client->client_closed) {
        // remove callbacks
        tcp_err(client->pcb, NULL);
        tcp_recv(client->pcb, NULL);
        tcp_sent(client->pcb, NULL);
        
        // abort
        tcp_abort(client->pcb);
        
        // kill client dead var
        DEAD_KILL_WITH(client->dead_client, -1);
        
        // set client closed
        client->client_closed = 1;
    }
    
    // free SOCKS
    if (!client->socks_closed) {
        if (client->socks_up) {
            // remove send finished job
            BPending_Free(&client->socks_send_finished_job);
        }
        
        // free SOCKS
        BSocksClient_Free(&client->socks_client);
        
        // set SOCKS closed
        client->socks_closed = 1;
    }
    
    // dealloc entry
    client_dealloc(client);
}

void client_dealloc (struct tcp_client *client)
{
    ASSERT(client->client_closed)
    ASSERT(client->socks_closed)
    
    // decrement counter
    ASSERT(num_clients > 0)
    num_clients--;
    
    // remove client entry
    LinkedList2_Remove(&tcp_clients, &client->list_node);
    
    // kill dead var
    DEAD_KILL(client->dead);
    
    // free memory
    free(client);
}

void client_err_func (void *arg, err_t err)
{
    struct tcp_client *client = arg;
    ASSERT(!client->client_closed)
    
    client_log(client, BLOG_INFO, "client error (%d)", (int)err);
    
    // the pcb was taken care of by the caller
    client_handle_freed_client(client, 0);
}

err_t client_recv_func (void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    struct tcp_client *client = arg;
    ASSERT(!client->client_closed)
    ASSERT(err == ERR_OK) // checked in lwIP source. Otherwise, I've no idea what should
                          // be done with the pbuf in case of an error.
    
    if (!p) {
        client_log(client, BLOG_INFO, "client closed");
        
        int ret = client_free_client(client);
        
        return (err ? ERR_ABRT : ERR_OK);
    }
    
    ASSERT(p->tot_len > 0)
    
    // check if we have enough buffer
    if (p->tot_len > sizeof(client->buf) - client->buf_used) {
        client_log(client, BLOG_ERROR, "no buffer for data !?!");
        return ERR_MEM;
    }
    
    // copy data to buffer
    ASSERT_EXECUTE(pbuf_copy_partial(p, client->buf + client->buf_used, p->tot_len, 0) == p->tot_len)
    client->buf_used += p->tot_len;
    
    // if there was nothing in the buffer before, and SOCKS is up, start send data
    if (client->buf_used == p->tot_len && client->socks_up) {
        ASSERT(!client->socks_closed) // this callback is removed when SOCKS is closed
        
        SYNC_DECL
        SYNC_FROMHERE
        client_send_to_socks(client);
        DEAD_ENTER(client->dead_client)
        SYNC_COMMIT
        if (DEAD_LEAVE(client->dead_client) == -1) {
            return ERR_ABRT;
        }
    }
    
    // free pbuff
    pbuf_free(p);
    
    return ERR_OK;
}

void client_socks_handler (struct tcp_client *client, int event)
{
    ASSERT(!client->socks_closed)
    
    switch (event) {
        case BSOCKSCLIENT_EVENT_ERROR: {
            client_log(client, BLOG_INFO, "SOCKS error");
            
            client_free_socks(client);
        } break;
        
        case BSOCKSCLIENT_EVENT_UP: {
            ASSERT(!client->socks_up)
            
            client_log(client, BLOG_INFO, "SOCKS up");
            
            // init sending
            client->socks_send_if = BSocksClient_GetSendInterface(&client->socks_client);
            StreamPassInterface_Sender_Init(client->socks_send_if, (StreamPassInterface_handler_done)client_socks_send_handler_done, client);
            client->socks_send_prev_buf_used = -1;
            BPending_Init(&client->socks_send_finished_job, BReactor_PendingGroup(&ss), (BPending_handler)client_socks_send_finished_job_handler, client);
            
            // init receiving
            client->socks_recv_if = BSocksClient_GetRecvInterface(&client->socks_client);
            StreamRecvInterface_Receiver_Init(client->socks_recv_if, (StreamRecvInterface_handler_done)client_socks_recv_handler_done, client);
            client->socks_recv_buf_used = -1;
            client->socks_recv_tcp_pending = 0;
            tcp_sent(client->pcb, client_sent_func);
            
            // set up
            client->socks_up = 1;
            
            // start sending data if there is any
            if (client->buf_used > 0) {
                client_send_to_socks(client);
            }
            
            // start receiving data if client is still up
            if (!client->client_closed) {
                client_socks_recv_initiate(client);
            }
        } break;
        
        case BSOCKSCLIENT_EVENT_ERROR_CLOSED: {
            ASSERT(client->socks_up)
            
            client_log(client, BLOG_INFO, "SOCKS closed");
            
            client_free_socks(client);
        } break;
        
        default:
            ASSERT(0);
    }
}

void client_send_to_socks (struct tcp_client *client)
{
    ASSERT(!client->socks_closed)
    ASSERT(client->socks_up)
    ASSERT(client->buf_used > 0)
    ASSERT(client->socks_send_prev_buf_used == -1)
    
    // remember amount of data in buffer
    client->socks_send_prev_buf_used = client->buf_used;
    
    // schedule finished job
    BPending_Set(&client->socks_send_finished_job);
    
    // schedule sending
    StreamPassInterface_Sender_Send(client->socks_send_if, client->buf, client->buf_used);
}

void client_socks_send_handler_done (struct tcp_client *client, int data_len)
{
    ASSERT(!client->socks_closed)
    ASSERT(client->socks_up)
    ASSERT(client->buf_used > 0)
    ASSERT(client->socks_send_prev_buf_used > 0)
    ASSERT(data_len > 0)
    ASSERT(data_len <= client->buf_used)
    
    // remove sent data from buffer
    memmove(client->buf, client->buf + data_len, client->buf_used - data_len);
    client->buf_used -= data_len;
    
    // send any further data
    if (client->buf_used > 0) {
        StreamPassInterface_Sender_Send(client->socks_send_if, client->buf, client->buf_used);
    }
}

void client_socks_send_finished_job_handler (struct tcp_client *client)
{
    ASSERT(!client->socks_closed)
    ASSERT(client->socks_up)
    ASSERT(client->socks_send_prev_buf_used > 0)
    ASSERT(client->buf_used <= client->socks_send_prev_buf_used)
    
    // calculate how much data was sent
    int sent = client->socks_send_prev_buf_used - client->buf_used;
    
    // unset remembered amount of data in buffer
    client->socks_send_prev_buf_used = -1;
    
    if (client->client_closed) {
        // client was closed we've sent everything we had buffered; we're done with it
        client_log(client, BLOG_INFO, "removing after client went down");
        
        client_free_socks(client);
    } else {
        // confirm sent data
        if (sent > 0) {
            tcp_recved(client->pcb, sent);
        }
    }
}

void client_socks_recv_initiate (struct tcp_client *client)
{
    ASSERT(!client->client_closed)
    ASSERT(!client->socks_closed)
    ASSERT(client->socks_up)
    ASSERT(client->socks_recv_buf_used == -1)
    
    StreamRecvInterface_Receiver_Recv(client->socks_recv_if, client->socks_recv_buf, sizeof(client->socks_recv_buf));
}

void client_socks_recv_handler_done (struct tcp_client *client, int data_len)
{
    ASSERT(data_len > 0)
    ASSERT(data_len <= sizeof(client->socks_recv_buf))
    ASSERT(!client->socks_closed)
    ASSERT(client->socks_up)
    ASSERT(client->socks_recv_buf_used == -1)
    
    // if client was closed, stop receiving
    if (client->client_closed) {
        return;
    }
    
    // set amount of data in buffer
    client->socks_recv_buf_used = data_len;
    client->socks_recv_buf_sent = 0;
    client->socks_recv_waiting = 0;
    
    // send to client
    if (client_socks_recv_send_out(client) < 0) {
        return;
    }
    
    // continue receiving if needed
    if (client->socks_recv_buf_used == -1) {
        client_socks_recv_initiate(client);
    }
}

int client_socks_recv_send_out (struct tcp_client *client)
{
    ASSERT(!client->client_closed)
    ASSERT(client->socks_up)
    ASSERT(client->socks_recv_buf_used > 0)
    ASSERT(client->socks_recv_buf_sent < client->socks_recv_buf_used)
    ASSERT(!client->socks_recv_waiting)
    
    // return value -1 means tcp_abort() was done,
    // 0 means it wasn't and the client (pcb) is still up
    
    do {
        int to_write = BMIN(client->socks_recv_buf_used - client->socks_recv_buf_sent, tcp_sndbuf(client->pcb));
        if (to_write == 0) {
            break;
        }
        
        err_t err = tcp_write(client->pcb, client->socks_recv_buf + client->socks_recv_buf_sent, to_write, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            if (err == ERR_MEM) {
                break;
            }
            
            client_log(client, BLOG_INFO, "tcp_write failed (%d)", (int)err);
            
            client_abort_client(client);
            return -1;
        }
        
        client->socks_recv_buf_sent += to_write;
        client->socks_recv_tcp_pending += to_write;
    } while (client->socks_recv_buf_sent < client->socks_recv_buf_used);
    
    // start sending now
    err_t err = tcp_output(client->pcb);
    if (err != ERR_OK) {
        client_log(client, BLOG_INFO, "tcp_output failed (%d)", (int)err);
        
        client_abort_client(client);
        return -1;
    }
    
    // more data to queue?
    if (client->socks_recv_buf_sent < client->socks_recv_buf_used) {
        if (client->socks_recv_tcp_pending == 0) {
            client_log(client, BLOG_ERROR, "can't queue data, but all data was confirmed !?!");
            
            client_abort_client(client);
            return -1;
        }
        
        // set waiting, continue in client_sent_func
        client->socks_recv_waiting = 1;
        return 0;
    }
    
    // everything was queued
    client->socks_recv_buf_used = -1;
    
    return 0;
}

err_t client_sent_func (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct tcp_client *client = arg;
    
    ASSERT(!client->client_closed)
    ASSERT(client->socks_up)
    ASSERT(len > 0)
    ASSERT(len <= client->socks_recv_tcp_pending)
    
    // decrement pending
    client->socks_recv_tcp_pending -= len;
    
    // continue queuing
    if (client->socks_recv_buf_used > 0) {
        ASSERT(client->socks_recv_waiting)
        ASSERT(client->socks_recv_buf_sent < client->socks_recv_buf_used)
        
        // set not waiting
        client->socks_recv_waiting = 0;
        
        // possibly send more data
        if (client_socks_recv_send_out(client) < 0) {
            return ERR_ABRT;
        }
        
        // we just queued some data, so it can't have been confirmed yet
        ASSERT(client->socks_recv_tcp_pending > 0)
        
        // continue receiving if needed
        if (client->socks_recv_buf_used == -1 && !client->socks_closed) {
            SYNC_DECL
            SYNC_FROMHERE
            client_socks_recv_initiate(client);
            DEAD_ENTER(client->dead)
            SYNC_COMMIT
            if (DEAD_LEAVE(client->dead)) {
                return ERR_ABRT;
            }
        }
        
        return ERR_OK;
    }
    
    // have we sent everything after SOCKS was closed?
    if (client->socks_closed && client->socks_recv_tcp_pending == 0) {
        client_log(client, BLOG_INFO, "removing after SOCKS went down");
        
        int ret = client_free_client(client);
        
        return (ret ? ERR_ABRT : ERR_OK);
    }
    
    return ERR_OK;
}
