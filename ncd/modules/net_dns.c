/**
 * @file net_dns.c
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
 * 
 * @section DESCRIPTION
 * 
 * DNS servers module.
 * 
 * Synopsis: net.dns(list(string) servers, string priority)
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <misc/offset.h>
#include <misc/bsort.h>
#include <structure/LinkedList2.h>
#include <ncd/NCDModule.h>
#include <ncd/NCDIfConfig.h>

#include <generated/blog_channel_ncd_net_dns.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    LinkedList2 ipv4_dns_servers;
    LinkedList2Node instances_node; // node in instances
};

struct ipv4_dns_entry {
    LinkedList2Node list_node; // node in instance.ipv4_dns_servers
    uint32_t addr;
    int priority;
};

static LinkedList2 instances;

static struct ipv4_dns_entry * add_ipv4_dns_entry (struct instance *o, uint32_t addr, int priority)
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
    LinkedList2_Append(&o->ipv4_dns_servers, &entry->list_node);
    
    return entry;
}

static void remove_ipv4_dns_entry (struct instance *o, struct ipv4_dns_entry *entry)
{
    // remove from list
    LinkedList2_Remove(&o->ipv4_dns_servers, &entry->list_node);
    
    // free entry
    free(entry);
}

static void remove_ipv4_dns_entries (struct instance *o)
{
    LinkedList2Node *n;
    while (n = LinkedList2_GetFirst(&o->ipv4_dns_servers)) {
        struct ipv4_dns_entry *e = UPPER_OBJECT(n, struct ipv4_dns_entry, list_node);
        remove_ipv4_dns_entry(o, e);
    }
}

static size_t num_servers (void)
{
    size_t c = 0;
    
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &instances);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        struct instance *o = UPPER_OBJECT(n, struct instance, instances_node);
        LinkedList2Iterator eit;
        LinkedList2Iterator_InitForward(&eit, &o->ipv4_dns_servers);
        while (LinkedList2Iterator_Next(&eit)) {
            c++;
        }
    }
    
    return c;
}

struct dns_sort_entry {
    uint32_t addr;
    int priority;
};

static int dns_sort_comparator (const void *v1, const void *v2)
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

static int set_servers (void)
{
    // count servers
    size_t num_ipv4_dns_servers = num_servers();
    
    // allocate sort array
    struct dns_sort_entry servers[num_ipv4_dns_servers];
    size_t num_servers = 0;
    
    // fill sort array
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &instances);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        struct instance *o = UPPER_OBJECT(n, struct instance, instances_node);
        LinkedList2Iterator eit;
        LinkedList2Iterator_InitForward(&eit, &o->ipv4_dns_servers);
        LinkedList2Node *en;
        while (en = LinkedList2Iterator_Next(&eit)) {
            struct ipv4_dns_entry *e = UPPER_OBJECT(en, struct ipv4_dns_entry, list_node);
            servers[num_servers].addr = e->addr;
            servers[num_servers].priority= e->priority;
            num_servers++;
        }
    }
    ASSERT(num_servers == num_ipv4_dns_servers)
    
    // sort by priority
    // use a custom insertion sort instead of qsort() because we want a stable sort
    BInsertionSort(servers, num_servers, sizeof(servers[0]), dns_sort_comparator);
    
    // copy addresses into an array
    uint32_t addrs[num_servers];
    for (size_t i = 0; i < num_servers; i++) {
        addrs[i] = servers[i].addr;
    }
    
    // set servers
    if (!NCDIfConfig_set_dns_servers(addrs, num_servers)) {
        return 0;
    }
    
    return 1;
}

static int func_globalinit (struct NCDModuleInitParams params)
{
    LinkedList2_Init(&instances);
    
    return 1;
}

static void * func_new (NCDModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    
    // init arguments
    o->i = i;
    
    // init servers list
    LinkedList2_Init(&o->ipv4_dns_servers);
    
    // get arguments
    NCDValue *servers_arg;
    NCDValue *priority_arg;
    if (!NCDValue_ListRead(o->i->args, 2, &servers_arg, &priority_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(servers_arg) != NCDVALUE_LIST || NCDValue_Type(priority_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    int priority = atoi(NCDValue_StringValue(priority_arg));
    
    // read servers
    NCDValue *server_arg = NCDValue_ListFirst(servers_arg);
    while (server_arg) {
        if (NCDValue_Type(server_arg) != NCDVALUE_STRING) {
            ModuleLog(o->i, BLOG_ERROR, "wrong type");
            goto fail1;
        }
        
        uint32_t addr;
        if (!ipaddr_parse_ipv4_addr(NCDValue_StringValue(server_arg), &addr)) {
            ModuleLog(o->i, BLOG_ERROR, "wrong addr");
            goto fail1;
        }
        
        if (!add_ipv4_dns_entry(o, addr, priority)) {
            ModuleLog(o->i, BLOG_ERROR, "failed to add dns entry");
            goto fail1;
        }
        
        server_arg = NCDValue_ListNext(servers_arg, server_arg);
    }
    
    // add to instances
    LinkedList2_Append(&instances, &o->instances_node);
    
    // set servers
    if (!set_servers()) {
        ModuleLog(o->i, BLOG_ERROR, "failed to set DNS servers");
        goto fail2;
    }
    
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    return o;
    
fail2:
    LinkedList2_Remove(&instances, &o->instances_node);
fail1:
    remove_ipv4_dns_entries(o);
    free(o);
fail0:
    return NULL;
}

static void func_free (void *vo)
{
    struct instance *o = vo;
    
    // remove from instances
    LinkedList2_Remove(&instances, &o->instances_node);
    
    // set servers
    set_servers();
    
    // free servers
    remove_ipv4_dns_entries(o);
    
    // free instance
    free(o);
}

static const struct NCDModule modules[] = {
    {
        .type = "net.dns",
        .func_new = func_new,
        .func_free = func_free
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_dns = {
    .func_globalinit = func_globalinit,
    .modules = modules
};
