/**
 * @file NCDUdevCache.h
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

#ifndef BADVPN_UDEVMONITOR_NCDUDEVCACHE_H
#define BADVPN_UDEVMONITOR_NCDUDEVCACHE_H

#include <misc/debug.h>
#include <structure/BAVL.h>
#include <structure/LinkedList1.h>
#include <base/DebugObject.h>
#include <stringmap/BStringMap.h>

struct NCDUdevCache_device {
    BStringMap map;
    const char *devpath;
    int is_cleaned;
    union {
        BAVLNode devices_tree_node;
        LinkedList1Node cleaned_devices_list_node;
    };
    int is_refreshed;
};

typedef struct {
    BAVL devices_tree;
    LinkedList1 cleaned_devices_list;
    DebugObject d_obj;
} NCDUdevCache;

void NCDUdevCache_Init (NCDUdevCache *o);
void NCDUdevCache_Free (NCDUdevCache *o);
const BStringMap * NCDUdevCache_Query (NCDUdevCache *o, const char *devpath);
int NCDUdevCache_Event (NCDUdevCache *o, BStringMap map) WARN_UNUSED;
void NCDUdevCache_StartClean (NCDUdevCache *o);
void NCDUdevCache_FinishClean (NCDUdevCache *o);
int NCDUdevCache_GetCleanedDevice (NCDUdevCache *o, BStringMap *out_map);
const char * NCDUdevCache_First (NCDUdevCache *o);
const char * NCDUdevCache_Next (NCDUdevCache *o, const char *key);

#endif
