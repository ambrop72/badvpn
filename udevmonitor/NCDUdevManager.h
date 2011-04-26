/**
 * @file NCDUdevManager.h
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

#ifndef BADVPN_UDEVMONITOR_NCDUDEVMANAGER_H
#define BADVPN_UDEVMONITOR_NCDUDEVMANAGER_H

#include <misc/debug.h>
#include <structure/LinkedList1.h>
#include <system/DebugObject.h>
#include <udevmonitor/NCDUdevMonitor.h>
#include <udevmonitor/NCDUdevCache.h>
#include <stringmap/BStringMap.h>

typedef void (*NCDUdevClient_handler) (void *user, char *devpath, int have_map, BStringMap map);

typedef struct {
    BReactor *reactor;
    BProcessManager *manager;
    LinkedList1 clients_list;
    NCDUdevCache cache;
    BTimer restart_timer;
    int have_monitor;
    NCDUdevMonitor monitor;
    int have_info_monitor;
    NCDUdevMonitor info_monitor;
    DebugObject d_obj;
} NCDUdevManager;

typedef struct {
    NCDUdevManager *m;
    void *user;
    NCDUdevClient_handler handler;
    LinkedList1Node clients_list_node;
    LinkedList1 events_list;
    BPending next_job;
    int running;
    DebugObject d_obj;
} NCDUdevClient;

struct NCDUdevClient_event {
    char *devpath;
    int have_map;
    BStringMap map;
    LinkedList1Node events_list_node;
};

void NCDUdevManager_Init (NCDUdevManager *o, BReactor *reactor, BProcessManager *manager);
void NCDUdevManager_Free (NCDUdevManager *o);
const BStringMap * NCDUdevManager_Query (NCDUdevManager *o, const char *devpath);

void NCDUdevClient_Init (NCDUdevClient *o, NCDUdevManager *m, void *user,
                         NCDUdevClient_handler handler);
void NCDUdevClient_Free (NCDUdevClient *o);
void NCDUdevClient_Pause (NCDUdevClient *o);
void NCDUdevClient_Continue (NCDUdevClient *o);

#endif
