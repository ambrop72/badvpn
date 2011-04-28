/**
 * @file event_template.h
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

#ifndef BADVPN_NCD_MODULES_EVENT_TEMPLATE_H
#define BADVPN_NCD_MODULES_EVENT_TEMPLATE_H

#include <structure/LinkedList1.h>
#include <stringmap/BStringMap.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_net_iptables.h>

typedef void (*event_template_func_free) (void *user);

typedef struct {
    NCDModuleInst *i;
    int blog_channel;
    void *user;
    event_template_func_free func_free;
    LinkedList1 events_list;
    int enabled;
    BStringMap enabled_map;
} event_template;

struct event_template_event {
    BStringMap map;
    LinkedList1Node events_list_node;
};

void event_template_new (event_template *o, NCDModuleInst *i, int blog_channel, void *user,
                         event_template_func_free func_free);
void event_template_die (event_template *o);
int event_template_getvar (event_template *o, const char *name, NCDValue *out);
int event_template_queue (event_template *o, BStringMap map, int *out_was_empty);
void event_template_next (event_template *o, int *out_is_empty);
void event_template_assert_enabled (event_template *o);

#endif
