/**
 * @file ncd.c
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
#include <misc/offset.h>
#include <misc/read_file.h>
#include <misc/string_begins_with.h>
#include <misc/ipaddr.h>
#include <structure/LinkedList2.h>
#include <system/BLog.h>
#include <system/BReactor.h>
#include <system/BSignal.h>
#include <system/BSocket.h>
#include <dhcpclient/BDHCPClient.h>
#include <ncdconfig/NCDConfigParser.h>
#include <ncd/NCDIfConfig.h>
#include <ncd/NCDInterfaceMonitor.h>

#ifndef BADVPN_USE_WINAPI
#include <system/BLog_syslog.h>
#endif

#include <ncd/ncd.h>

#include <generated/blog_channel_ncd.h>

#define LOGGER_STDOUT 1
#define LOGGER_SYSLOG 2

#define INTERFACE_TYPE_PHYSICAL 1

#define INTERFACE_STATE_RESETTING 2
#define INTERFACE_STATE_WAITDEVICE 3
#define INTERFACE_STATE_WAITLINK 4
#define INTERFACE_STATE_DHCP 5
#define INTERFACE_STATE_FINISHED 6

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
    char *config_file;
} options;

struct interface {
    LinkedList2Node list_node; // node in interfaces
    struct NCDConfig_interfaces *conf;
    int type;
    int state;
    BTimer reset_timer;
    int up;
    int have_dhcp;
    BDHCPClient dhcp;
    LinkedList2 ipv4_addresses;
    LinkedList2 ipv4_routes;
    LinkedList2 ipv4_dns_servers;
};

struct ipv4_addr_entry {
    LinkedList2Node list_node; // node in interface.ipv4_addresses
    struct ipv4_ifaddr ifaddr;
};

struct ipv4_route_entry {
    LinkedList2Node list_node; // node in interface.ipv4_routes
    struct ipv4_ifaddr dest;
    uint32_t gateway;
    int metric;
};

struct ipv4_dns_entry {
    LinkedList2Node list_node; // node in interface.ipv4_dns_servers
    uint32_t addr;
    int priority;
};

// reactor
BReactor ss;

// configuration
struct NCDConfig_interfaces *configuration;

// interface monitor
NCDInterfaceMonitor monitor;

// interfaces
LinkedList2 interfaces;

// number of DNS servers
size_t num_ipv4_dns_servers;

static void terminate (void);
static void print_help (const char *name);
static void print_version (void);
static int parse_arguments (int argc, char *argv[]);
static void signal_handler (void *unused);
static int statement_name_is (struct NCDConfig_statements *st, const char *needle);
static int statement_has_one_arg (struct NCDConfig_statements *st, char **arg1_out);
static int statement_has_two_args (struct NCDConfig_statements *st, char **arg1_out, char **arg2_out);
static int statement_has_three_args (struct NCDConfig_statements *st, char **arg1_out, char **arg2_out, char **arg3_out);
static struct NCDConfig_statements * find_statement (struct NCDConfig_statements *st, const char *needle);
static void load_interfaces (struct NCDConfig_interfaces *conf);
static void free_interfaces (void);
static int set_dns_servers (void);
static int dns_qsort_comparator (const void *v1, const void *v2);
static struct interface * find_interface (const char *name);
static void monitor_handler (void *unused, const char *ifname, int if_flags);
static int interface_init (struct NCDConfig_interfaces *conf);
static void interface_free (struct interface *iface);
static void interface_reset (struct interface *iface);
static void interface_start (struct interface *iface);
static void interface_link_up (struct interface *iface);
static void interface_link_down (struct interface *iface);
static void interface_deconfigure (struct interface *iface);
static void interface_log (struct interface *iface, int level, const char *fmt, ...);
static void interface_reset_timer_handler (struct interface *iface);
static void interface_dhcp_handler (struct interface *iface, int event);
static int interface_set_up (struct interface *iface);
static void interface_set_down (struct interface *iface);
static int interface_configure_ipv4 (struct interface *iface);
static void interface_deconfigure_ipv4 (struct interface *iface);
static int interface_add_ipv4_addresses (struct interface *iface);
static void interface_remove_ipv4_addresses (struct interface *iface);
static int interface_add_ipv4_routes (struct interface *iface);
static void interface_remove_ipv4_routes (struct interface *iface);
static int interface_add_ipv4_dns_servers (struct interface *iface);
static void interface_remove_ipv4_dns_servers (struct interface *iface);
static int interface_add_ipv4_addr (struct interface *iface, struct ipv4_ifaddr ifaddr);
static void interface_remove_ipv4_addr (struct interface *iface, struct ipv4_addr_entry *entry);
static int interface_add_ipv4_route (struct interface *iface, struct ipv4_ifaddr dest, uint32_t gateway, int metric);
static void interface_remove_ipv4_route (struct interface *iface, struct ipv4_route_entry *entry);
static struct ipv4_addr_entry * interface_add_ipv4_addr_entry (struct interface *iface, struct ipv4_ifaddr ifaddr);
static void interface_remove_ipv4_addr_entry (struct interface *iface, struct ipv4_addr_entry *entry);
static struct ipv4_route_entry * interface_add_ipv4_route_entry (struct interface *iface, struct ipv4_ifaddr dest, uint32_t gateway, int metric);
static void interface_remove_ipv4_route_entry (struct interface *iface, struct ipv4_route_entry *entry);
static struct ipv4_dns_entry * interface_add_ipv4_dns_entry (struct interface *iface, uint32_t addr, int priority);
static void interface_remove_ipv4_dns_entry (struct interface *iface, struct ipv4_dns_entry *entry);

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
    
    // init time
    BTime_Init();
    
    // init reactor
    if (!BReactor_Init(&ss)) {
        BLog(BLOG_ERROR, "BReactor_Init failed");
        goto fail1;
    }
    
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
    
    // read config file
    uint8_t *file;
    size_t file_len;
    if (!read_file(options.config_file, &file, &file_len)) {
        BLog(BLOG_ERROR, "failed to read config file");
        goto fail3;
    }
    
    // parse config file
    if (!NCDConfigParser_Parse(file, file_len, &configuration)) {
        BLog(BLOG_ERROR, "NCDConfigParser_Parse failed");
        free(file);
        goto fail3;
    }
    
    // fee config file memory
    free(file);
    
    // init interface monitor
    if (!NCDInterfaceMonitor_Init(&monitor, &ss, monitor_handler, NULL)) {
        BLog(BLOG_ERROR, "NCDInterfaceMonitor_Init failed");
        goto fail4;
    }
    
    // init interfaces list
    LinkedList2_Init(&interfaces);
    
    // set no DNS servers
    num_ipv4_dns_servers = 0;
    if (!set_dns_servers()) {
        BLog(BLOG_ERROR, "failed to set no DNS servers");
        goto fail5;
    }
    
    // init interfaces
    load_interfaces(configuration);
    
    goto event_loop;
    
fail5:
    NCDInterfaceMonitor_Free(&monitor);
fail4:
    NCDConfig_free_interfaces(configuration);
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
    
    // free interfaces
    free_interfaces();
    
    // free interface monitor
    NCDInterfaceMonitor_Free(&monitor);
    
    // free configuration
    NCDConfig_free_interfaces(configuration);
    
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
        "        --config-file <file>\n",
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
    options.config_file = NULL;
    
    for (int i = 1; i < argc; i++) {
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
        else if (!strcmp(arg, "--config-file")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.config_file = argv[i + 1];
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
    
    if (!options.config_file) {
        fprintf(stderr, "--config-file is required\n");
        return 0;
    }
    
    return 1;
}

void signal_handler (void *unused)
{
    BLog(BLOG_NOTICE, "termination requested");
    
    terminate();
    return;
}

int statement_name_is (struct NCDConfig_statements *st, const char *needle)
{
    ASSERT(st->names)
    
    size_t l;
    
    struct NCDConfig_strings *name = st->names;
    if (!(l = string_begins_with(needle, name->value))) {
        return 0;
    }
    needle += l;
    
    name = name->next;
    
    while (name) {
        if (!(l = string_begins_with(needle, "."))) {
            return 0;
        }
        needle += l;
        
        if (!(l = string_begins_with(needle, name->value))) {
            return 0;
        }
        needle += l;
        
        name = name->next;
    }
    
    if (*needle) {
        return 0;
    }
    
    return 1;
}

int statement_has_one_arg (struct NCDConfig_statements *st, char **arg1_out)
{
    if (!(st->args && !st->args->next)) {
        return 0;
    }
    
    *arg1_out = st->args->value;
    return 1;
}

int statement_has_two_args (struct NCDConfig_statements *st, char **arg1_out, char **arg2_out)
{
    if (!(st->args && st->args->next && !st->args->next->next)) {
        return 0;
    }
    
    *arg1_out = st->args->value;
    *arg2_out = st->args->next->value;
    return 1;
}

int statement_has_three_args (struct NCDConfig_statements *st, char **arg1_out, char **arg2_out, char **arg3_out)
{
    if (!(st->args && st->args->next && st->args->next->next && !st->args->next->next->next)) {
        return 0;
    }
    
    *arg1_out = st->args->value;
    *arg2_out = st->args->next->value;
    *arg3_out = st->args->next->next->value;
    return 1;
}

struct NCDConfig_statements * find_statement (struct NCDConfig_statements *st, const char *needle)
{
    while (st) {
        if (statement_name_is(st, needle)) {
            return st;
        }
        
        st = st->next;
    }
    
    return NULL;
}

void load_interfaces (struct NCDConfig_interfaces *conf)
{
    while (conf) {
        interface_init(conf);
        conf = conf->next;
    }
}

void free_interfaces (void)
{
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&interfaces)) {
        struct interface *iface = UPPER_OBJECT(node, struct interface, list_node);
        interface_free(iface);
    }
}

struct dns_sort_entry {
    uint32_t addr;
    int priority;
};

int set_dns_servers (void)
{
    // collect servers
    
    struct dns_sort_entry servers[num_ipv4_dns_servers];
    size_t num_servers = 0;
    
    LinkedList2Iterator if_it;
    LinkedList2Iterator_InitForward(&if_it, &interfaces);
    LinkedList2Node *if_node;
    while (if_node = LinkedList2Iterator_Next(&if_it)) {
        struct interface *iface = UPPER_OBJECT(if_node, struct interface, list_node);
        
        LinkedList2Iterator serv_it;
        LinkedList2Iterator_InitForward(&serv_it, &iface->ipv4_dns_servers);
        LinkedList2Node *serv_node;
        while (serv_node = LinkedList2Iterator_Next(&serv_it)) {
            struct ipv4_dns_entry *dns = UPPER_OBJECT(serv_node, struct ipv4_dns_entry, list_node);
            
            servers[num_servers].addr = dns->addr;
            servers[num_servers].priority= dns->priority;
            num_servers++;
        }
    }
    
    ASSERT(num_servers == num_ipv4_dns_servers)
    
    // sort by priority
    qsort(servers, num_servers, sizeof(servers[0]), dns_qsort_comparator);
    
    // copy addresses into an array
    uint32_t addrs[num_servers];
    for (size_t i = 0; i < num_servers; i++) {
        addrs[i] = servers[i].addr;
    }
    
    // set servers
    if (!NCDIfConfig_set_dns_servers(addrs, num_servers)) {
        BLog(BLOG_ERROR, "failed to set DNS servers");
        return 0;
    }
    
    return 1;
}

int dns_qsort_comparator (const void *v1, const void *v2)
{
    const struct dns_sort_entry *e1 = v1;
    const struct dns_sort_entry *e2 = v2;
    
    if (e1->priority < e2->priority) {
        return -1;
    }
    if (e1->priority > e2->priority) {
        return 1;
    }
    return 0;
}

struct interface * find_interface (const char *name)
{
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &interfaces);
    LinkedList2Node *node;
    while (node = LinkedList2Iterator_Next(&it)) {
        struct interface *iface = UPPER_OBJECT(node, struct interface, list_node);
        if (!strcmp(iface->conf->name, name)) {
            LinkedList2Iterator_Free(&it);
            return iface;
        }
    }
    
    return NULL;
}

void monitor_handler (void *unused, const char *ifname, int if_flags)
{
    struct interface *iface = find_interface(ifname);
    if (!iface) {
        return;
    }
    
    if (!(if_flags&NCDIFCONFIG_FLAG_EXISTS)) {
        if (iface->state > INTERFACE_STATE_WAITDEVICE) {
            interface_log(iface, BLOG_INFO, "device down");
        
            // deconfigure
            interface_deconfigure(iface);
            
            // set state waitdevice
            iface->state = INTERFACE_STATE_WAITDEVICE;
        }
    } else {
        if (iface->state == INTERFACE_STATE_RESETTING || iface->state == INTERFACE_STATE_WAITDEVICE) {
            interface_log(iface, BLOG_INFO, "device up");
        
            // stop reset timer (if it was resetting)
            BReactor_RemoveTimer(&ss, &iface->reset_timer);
            
            // start
            interface_start(iface);
            
            return;
        }
        
        if ((if_flags&NCDIFCONFIG_FLAG_RUNNING)) {
            if (iface->state == INTERFACE_STATE_WAITLINK) {
                interface_log(iface, BLOG_INFO, "link up");
                
                interface_link_up(iface);
            }
        } else {
            if (iface->state > INTERFACE_STATE_WAITLINK) {
                interface_log(iface, BLOG_INFO, "link down");
                
                interface_link_down(iface);
            }
        }
    }
}

int interface_init (struct NCDConfig_interfaces *conf)
{
    // check for existing interface
    if (find_interface(conf->name)) {
        BLog(BLOG_ERROR, "interface %s already exists", conf->name);
        goto fail0;
    }
    
    // allocate interface entry
    struct interface *iface = malloc(sizeof(*iface));
    if (!iface) {
        BLog(BLOG_ERROR, "malloc failed for interface %s", conf->name);
        goto fail0;
    }
    
    // set conf
    iface->conf = conf;
    
    // set type
    struct NCDConfig_statements *type_st = find_statement(conf->statements, "type");
    if (!type_st) {
        interface_log(iface, BLOG_ERROR, "missing type");
        goto fail1;
    }
    char *type;
    if (!statement_has_one_arg(type_st, &type)) {
        interface_log(iface, BLOG_ERROR, "type: wrong arity");
        goto fail1;
    }
    if (!strcmp(type, "physical")) {
        iface->type = INTERFACE_TYPE_PHYSICAL;
    }
    else {
        interface_log(iface, BLOG_ERROR, "type: unknown value");
        goto fail1;
    }
    
    // init reset timer
    BTimer_Init(&iface->reset_timer, INTERFACE_RETRY_TIME, (BTimer_handler)interface_reset_timer_handler, iface);
    
    // set not up
    iface->up = 0;
    
    // set no DHCP
    iface->have_dhcp = 0;
    
    // init ipv4 addresses list
    LinkedList2_Init(&iface->ipv4_addresses);
    
    // init ipv4 routes list
    LinkedList2_Init(&iface->ipv4_routes);
    
    // init ipv4 dns servers list
    LinkedList2_Init(&iface->ipv4_dns_servers);
    
    // insert to interfaces list
    LinkedList2_Append(&interfaces, &iface->list_node);
    
    interface_start(iface);
    
    return 1;
    
fail1:
    free(iface);
fail0:
    return 0;
}

void interface_free (struct interface *iface)
{
    // deconfigure
    interface_deconfigure(iface);
    
    // remove from interfaces list
    LinkedList2_Remove(&interfaces, &iface->list_node);
    
    // stop reset timer
    BReactor_RemoveTimer(&ss, &iface->reset_timer);
    
    // free memory
    free(iface);
}

void interface_reset (struct interface *iface)
{
    interface_log(iface, BLOG_INFO, "will try again later");
    
    // deconfigure
    interface_deconfigure(iface);
    
    // set reset timer
    BReactor_SetTimer(&ss, &iface->reset_timer);
    
    // set state
    iface->state = INTERFACE_STATE_RESETTING;
}

void interface_start (struct interface *iface)
{
    ASSERT(!iface->up)
    ASSERT(!iface->have_dhcp)
    ASSERT(LinkedList2_IsEmpty(&iface->ipv4_addresses))
    ASSERT(LinkedList2_IsEmpty(&iface->ipv4_routes))
    ASSERT(LinkedList2_IsEmpty(&iface->ipv4_dns_servers))
    
    interface_log(iface, BLOG_INFO, "starting");
    
    // query interface state
    int flags = NCDIfConfig_query(iface->conf->name);
    
    if (!(flags&NCDIFCONFIG_FLAG_EXISTS)) {
        interface_log(iface, BLOG_INFO, "device doesn't exist");
        
        // waiting for device
        iface->state = INTERFACE_STATE_WAITDEVICE;
        return;
    }
    
    if ((flags&NCDIFCONFIG_FLAG_UP)) {
        interface_log(iface, BLOG_ERROR, "device already up - NOT configuring");
        goto fail;
    }
    
    // set interface up
    if (!interface_set_up(iface)) {
        goto fail;
    }
    
    interface_log(iface, BLOG_INFO, "waiting for link");
    
    // waiting for link
    iface->state = INTERFACE_STATE_WAITLINK;
    
    return;
    
fail:
    interface_reset(iface);
}

void interface_link_up (struct interface *iface)
{
    ASSERT(iface->up)
    ASSERT(!iface->have_dhcp)
    ASSERT(LinkedList2_IsEmpty(&iface->ipv4_addresses))
    ASSERT(LinkedList2_IsEmpty(&iface->ipv4_routes))
    ASSERT(LinkedList2_IsEmpty(&iface->ipv4_dns_servers))
    
    // check for DHCP
    struct NCDConfig_statements *dhcp_st = find_statement(iface->conf->statements, "dhcp");
    if (dhcp_st) {
        if (dhcp_st->args) {
            interface_log(iface, BLOG_ERROR, "dhcp: wrong arity");
            goto fail;
        }
        
        // init DHCP client
        if (!BDHCPClient_Init(&iface->dhcp, iface->conf->name, &ss, (BDHCPClient_handler)interface_dhcp_handler, iface)) {
            interface_log(iface, BLOG_ERROR, "BDHCPClient_Init failed");
            goto fail;
        }
        
        // set have DHCP
        iface->have_dhcp = 1;
        
        // set state
        iface->state = INTERFACE_STATE_DHCP;
        
        return;
    }
    
    // configure IPv4
    if (!interface_configure_ipv4(iface)) {
        goto fail;
    }
    
    // set state finished
    iface->state = INTERFACE_STATE_FINISHED;
    
    return;
    
fail:
    interface_reset(iface);
}

void interface_link_down (struct interface *iface)
{
    ASSERT(iface->up)
    
    // deconfigure IPv4
    interface_deconfigure_ipv4(iface);
    
    // free DHCP
    if (iface->have_dhcp) {
        BDHCPClient_Free(&iface->dhcp);
        iface->have_dhcp = 0;
    }
    
    // set state waitlink
    iface->state = INTERFACE_STATE_WAITLINK;
}

void interface_deconfigure (struct interface *iface)
{
    // deconfigure IPv4
    interface_deconfigure_ipv4(iface);
    
    // free DHCP
    if (iface->have_dhcp) {
        BDHCPClient_Free(&iface->dhcp);
        iface->have_dhcp = 0;
    }
    
    // set down
    if (iface->up) {
        interface_set_down(iface);
    }
}

void interface_log (struct interface *iface, int level, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    BLog_Append("interface %s: ", iface->conf->name);
    BLog_LogToChannelVarArg(BLOG_CURRENT_CHANNEL, level, fmt, vl);
    va_end(vl);
}

void interface_reset_timer_handler (struct interface *iface)
{
    ASSERT(iface->state == INTERFACE_STATE_RESETTING)
    
    // start interface
    interface_start(iface);
}

void interface_dhcp_handler (struct interface *iface, int event)
{
    ASSERT(iface->have_dhcp)
    
    switch (event) {
        case BDHCPCLIENT_EVENT_UP: {
            ASSERT(iface->state == INTERFACE_STATE_DHCP)
            
            interface_log(iface, BLOG_INFO, "DHCP up");
            
            // configure IPv4
            if (!interface_configure_ipv4(iface)) {
                goto fail;
            }
            
            // set state
            iface->state = INTERFACE_STATE_FINISHED;
            
            return;
            
        fail:
            interface_reset(iface);
        } break;
        
        case BDHCPCLIENT_EVENT_DOWN: {
            ASSERT(iface->state == INTERFACE_STATE_FINISHED)
            
            interface_log(iface, BLOG_INFO, "DHCP down");
            
            // deconfigure IPv4
            interface_deconfigure_ipv4(iface);
            
            // set state
            iface->state = INTERFACE_STATE_DHCP;
        } break;
        
        default:
            ASSERT(0);
    }
}

int interface_set_up (struct interface *iface)
{
    ASSERT(!iface->up)
    
    // set up
    if (!NCDIfConfig_set_up(iface->conf->name)) {
        interface_log(iface, BLOG_ERROR, "failed to set up");
        return 0;
    }
    
    iface->up = 1;
    
    return 1;
}

void interface_set_down (struct interface *iface)
{
    ASSERT(iface->up)
    
    // set down
    if (!NCDIfConfig_set_down(iface->conf->name)) {
        interface_log(iface, BLOG_ERROR, "failed to set down");
    }
    
    iface->up = 0;
}

int interface_configure_ipv4 (struct interface *iface)
{
    ASSERT(!(iface->have_dhcp) || BDHCPClient_IsUp(&iface->dhcp))
    ASSERT(LinkedList2_IsEmpty(&iface->ipv4_addresses))
    ASSERT(LinkedList2_IsEmpty(&iface->ipv4_routes))
    ASSERT(LinkedList2_IsEmpty(&iface->ipv4_dns_servers))
    
    // configure addresses
    if (!interface_add_ipv4_addresses(iface)) {
        return 0;
    }
    
    // configure routes
    if (!interface_add_ipv4_routes(iface)) {
        return 0;
    }
    
    // configure DNS servers
    if (!interface_add_ipv4_dns_servers(iface)) {
        return 0;
    }
    
    return 1;
}

void interface_deconfigure_ipv4 (struct interface *iface)
{
    // remove DNS servers
    interface_remove_ipv4_dns_servers(iface);
    
    // remove routes
    interface_remove_ipv4_routes(iface);
    
    // remove addresses
    interface_remove_ipv4_addresses(iface);
}

int interface_add_ipv4_addresses (struct interface *iface)
{
    ASSERT(!(iface->have_dhcp) || BDHCPClient_IsUp(&iface->dhcp))
    ASSERT(LinkedList2_IsEmpty(&iface->ipv4_addresses))
    
    struct NCDConfig_statements *st = iface->conf->statements;
    
    while (st = find_statement(st, "ipv4.addr")) {
        // get address string
        char *addrstr;
        if (!statement_has_one_arg(st, &addrstr)) {
            interface_log(iface, BLOG_ERROR, "ipv4.addr: wrong arity");
            return 0;
        }
        
        struct ipv4_ifaddr ifaddr;
        
        // is this a DHCP address?
        if (!strcmp(addrstr, "dhcp_addr")) {
            // check if DHCP is enabled
            if (!iface->have_dhcp) {
                interface_log(iface, BLOG_ERROR, "ipv4.addr: %s: DHCP not enabled", addrstr);
                return 0;
            }
            
            // get address and mask
            uint32_t addr;
            BDHCPClient_GetClientIP(&iface->dhcp, &addr);
            uint32_t mask;
            BDHCPClient_GetClientMask(&iface->dhcp, &mask);
            
            // convert to ifaddr
            if (!ipaddr_ipv4_ifaddr_from_addr_mask(addr, mask, &ifaddr)) {
                interface_log(iface, BLOG_ERROR, "ipv4.addr: %s: wrong mask", addrstr);
                return 0;
            }
        } else {
            // parse address string
            if (!ipaddr_parse_ipv4_ifaddr(addrstr, &ifaddr)) {
                interface_log(iface, BLOG_ERROR, "ipv4.addr: %s: wrong format", addrstr);
                return 0;
            }
        }
        
        // add this address
        if (!interface_add_ipv4_addr(iface, ifaddr)) {
            interface_log(iface, BLOG_ERROR, "ipv4.addr: %s: failed to add", addrstr);
            return 0;
        }
        
        st = st->next;
    }
    
    return 1;
}

void interface_remove_ipv4_addresses (struct interface *iface)
{
    LinkedList2Node *node;
    while (node = LinkedList2_GetLast(&iface->ipv4_addresses)) {
        struct ipv4_addr_entry *entry = UPPER_OBJECT(node, struct ipv4_addr_entry, list_node);
        interface_remove_ipv4_addr(iface, entry);
    }
}

int interface_add_ipv4_routes (struct interface *iface)
{
    ASSERT(!(iface->have_dhcp) || BDHCPClient_IsUp(&iface->dhcp))
    ASSERT(LinkedList2_IsEmpty(&iface->ipv4_routes))
    
    struct NCDConfig_statements *st = iface->conf->statements;
    
    while (st = find_statement(st, "ipv4.route")) {
        // read statement
        char *dest_str;
        char *gateway_str;
        char *metric_str;
        if (!statement_has_three_args(st, &dest_str, &gateway_str, &metric_str)) {
            interface_log(iface, BLOG_ERROR, "ipv4.route: wrong arity");
            return 0;
        }
        
        // parse dest string
        struct ipv4_ifaddr dest;
        if (!ipaddr_parse_ipv4_ifaddr(dest_str, &dest)) {
            interface_log(iface, BLOG_ERROR, "ipv4.route: (%s,%s,%s): dest: wrong format", dest_str, gateway_str, metric_str);
            return 0;
        }
        
        uint32_t gateway;
        
        // is gateway a DHCP router?
        if (!strcmp(gateway_str, "dhcp_router")) {
            // check if DHCP is enabled
            if (!iface->have_dhcp) {
                interface_log(iface, BLOG_ERROR, "ipv4.route: (%s,%s,%s): gateway: DHCP not enabled", dest_str, gateway_str, metric_str);
                return 0;
            }
            
            // obtain gateway from DHCP
            if (!BDHCPClient_GetRouter(&iface->dhcp, &gateway)) {
                interface_log(iface, BLOG_ERROR, "ipv4.route: (%s,%s,%s): gateway: DHCP did not provide a router", dest_str, gateway_str, metric_str);
                return 0;
            }
        } else {
            // parse gateway string
            if (!ipaddr_parse_ipv4_addr(gateway_str, strlen(gateway_str), &gateway)) {
                interface_log(iface, BLOG_ERROR, "ipv4.route: (%s,%s,%s): gateway: wrong format", dest_str, gateway_str, metric_str);
                return 0;
            }
        }
        
        // parse metric string
        int metric = atoi(metric_str);
        
        // add this address
        if (!interface_add_ipv4_route(iface, dest, gateway, metric)) {
            interface_log(iface, BLOG_ERROR, "ipv4.route: (%s,%s,%s): failed to add", dest_str, gateway_str, metric_str);
            return 0;
        }
        
        st = st->next;
    }
    
    return 1;
}

void interface_remove_ipv4_routes (struct interface *iface)
{
    LinkedList2Node *node;
    while (node = LinkedList2_GetLast(&iface->ipv4_routes)) {
        struct ipv4_route_entry *entry = UPPER_OBJECT(node, struct ipv4_route_entry, list_node);
        interface_remove_ipv4_route(iface, entry);
    }
}

int interface_add_ipv4_dns_servers (struct interface *iface)
{
    ASSERT(!(iface->have_dhcp) || BDHCPClient_IsUp(&iface->dhcp))
    ASSERT(LinkedList2_IsEmpty(&iface->ipv4_dns_servers))
    
    // read servers into entries
    
    struct NCDConfig_statements *st = iface->conf->statements;
    
    while (st = find_statement(st, "ipv4.dns")) {
        // read statement
        char *addr_str;
        char *priority_str;
        if (!statement_has_two_args(st, &addr_str, &priority_str)) {
            interface_log(iface, BLOG_ERROR, "ipv4.dns: wrong arity");
            return 0;
        }
        
        // parse priority string
        int priority = atoi(priority_str);
        
        // are these DHCP DNS servers?
        if (!strcmp(addr_str, "dhcp_dns_servers")) {
            // get servers from DHCP
            uint32_t addrs[BDHCPCLIENT_MAX_DOMAIN_NAME_SERVERS];
            int num_addrs = BDHCPClient_GetDNS(&iface->dhcp, addrs, BDHCPCLIENT_MAX_DOMAIN_NAME_SERVERS);
            
            // add entries
            for (int i = 0; i < num_addrs; i++) {
                // add entry
                if (!interface_add_ipv4_dns_entry(iface, addrs[i], priority)) {
                    interface_log(iface, BLOG_ERROR, "ipv4.dns: (%s,%s): failed to add entry", addr_str, priority_str);
                    return 0;
                }
            }
        } else {
            // parse addr string
            uint32_t addr;
            if (!ipaddr_parse_ipv4_addr(addr_str, strlen(addr_str), &addr)) {
                interface_log(iface, BLOG_ERROR, "ipv4.dns: (%s,%s): addr: wrong format", addr_str, priority_str);
                return 0;
            }
            
            // add entry
            if (!interface_add_ipv4_dns_entry(iface, addr, priority)) {
                interface_log(iface, BLOG_ERROR, "ipv4.dns: (%s,%s): failed to add entry", addr_str, priority_str);
                return 0;
            }
        }
        
        st = st->next;
    }
    
    // set servers
    if (!set_dns_servers()) {
        return 0;
    }
    
    return 1;
}

void interface_remove_ipv4_dns_servers (struct interface *iface)
{
    // remove entries
    LinkedList2Node *node;
    while (node = LinkedList2_GetFirst(&iface->ipv4_dns_servers)) {
        struct ipv4_dns_entry *entry = UPPER_OBJECT(node, struct ipv4_dns_entry, list_node);
        interface_remove_ipv4_dns_entry(iface, entry);
    }
    
    // set servers
    set_dns_servers();
}

int interface_add_ipv4_addr (struct interface *iface, struct ipv4_ifaddr ifaddr)
{
    // add address entry
    struct ipv4_addr_entry *entry = interface_add_ipv4_addr_entry(iface, ifaddr);
    if (!entry) {
        interface_log(iface, BLOG_ERROR, "failed to add ipv4 address entry");
        return 0;
    }
    
    // assign the address
    if (!NCDIfConfig_add_ipv4_addr(iface->conf->name, ifaddr)) {
        interface_log(iface, BLOG_ERROR, "failed to assign ipv4 address");
        interface_remove_ipv4_addr_entry(iface, entry);
        return 0;
    }
    
    return 1;
}

void interface_remove_ipv4_addr (struct interface *iface, struct ipv4_addr_entry *entry)
{
    // remove the address
    if (!NCDIfConfig_remove_ipv4_addr(iface->conf->name, entry->ifaddr)) {
        interface_log(iface, BLOG_ERROR, "failed to remove ipv4 address");
    }
    
    // remove address entry
    interface_remove_ipv4_addr_entry(iface, entry);
}

int interface_add_ipv4_route (struct interface *iface, struct ipv4_ifaddr dest, uint32_t gateway, int metric)
{
    // add address entry
    struct ipv4_route_entry *entry = interface_add_ipv4_route_entry(iface, dest, gateway, metric);
    if (!entry) {
        interface_log(iface, BLOG_ERROR, "failed to add ipv4 route entry");
        return 0;
    }
    
    // add the route
    if (!NCDIfConfig_add_ipv4_route(dest, gateway, metric, iface->conf->name)) {
        interface_log(iface, BLOG_ERROR, "failed to add ipv4 route");
        interface_remove_ipv4_route_entry(iface, entry);
        return 0;
    }
    
    return 1;
}

void interface_remove_ipv4_route (struct interface *iface, struct ipv4_route_entry *entry)
{
    // remove the route
    if (!NCDIfConfig_remove_ipv4_route(entry->dest, entry->gateway, entry->metric, iface->conf->name)) {
        interface_log(iface, BLOG_ERROR, "failed to remove ipv4 route");
    }
    
    // remove address entry
    interface_remove_ipv4_route_entry(iface, entry);
}


struct ipv4_addr_entry * interface_add_ipv4_addr_entry (struct interface *iface, struct ipv4_ifaddr ifaddr)
{
    // allocate entry
    struct ipv4_addr_entry *entry = malloc(sizeof(*entry));
    if (!entry) {
        return NULL;
    }
    
    // set ifaddr
    entry->ifaddr = ifaddr;
    
    // add to list
    LinkedList2_Append(&iface->ipv4_addresses, &entry->list_node);
    
    return entry;
}

void interface_remove_ipv4_addr_entry (struct interface *iface, struct ipv4_addr_entry *entry)
{
    // remove from list
    LinkedList2_Remove(&iface->ipv4_addresses, &entry->list_node);
    
    // free entry
    free(entry);
}

struct ipv4_route_entry * interface_add_ipv4_route_entry (struct interface *iface, struct ipv4_ifaddr dest, uint32_t gateway, int metric)
{
    // allocate entry
    struct ipv4_route_entry *entry = malloc(sizeof(*entry));
    if (!entry) {
        return NULL;
    }
    
    // set info
    entry->dest = dest;
    entry->gateway = gateway;
    entry->metric = metric;
    
    // add to list
    LinkedList2_Append(&iface->ipv4_routes, &entry->list_node);
    
    return entry;
}

void interface_remove_ipv4_route_entry (struct interface *iface, struct ipv4_route_entry *entry)
{
    // remove from list
    LinkedList2_Remove(&iface->ipv4_routes, &entry->list_node);
    
    // free entry
    free(entry);
}

struct ipv4_dns_entry * interface_add_ipv4_dns_entry (struct interface *iface, uint32_t addr, int priority)
{
    // allocate entry
    struct ipv4_dns_entry *entry = malloc(sizeof(*entry));
    if (!entry) {
        return NULL;
    }
    
    // set info
    entry->addr = addr;
    entry->priority = priority;
    
    // add to list
    LinkedList2_Append(&iface->ipv4_dns_servers, &entry->list_node);
    
    // increment number of DNS servers
    num_ipv4_dns_servers++;
    
    return entry;
}

void interface_remove_ipv4_dns_entry (struct interface *iface, struct ipv4_dns_entry *entry)
{
    // decrement number of DNS servers
    num_ipv4_dns_servers--;
    
    // remove from list
    LinkedList2_Remove(&iface->ipv4_dns_servers, &entry->list_node);
    
    // free entry
    free(entry);
}
