/**
 * @file BStringMap.h
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

#ifndef BADVPN_STRINGMAP_BSTRINGMAP_H
#define BADVPN_STRINGMAP_BSTRINGMAP_H

#include <misc/debug.h>
#include <structure/BAVL.h>
#include <system/DebugObject.h>

struct BStringMap_entry {
    char *key;
    char *value;
    BAVLNode tree_node;
};

typedef struct {
    BAVL tree;
    DebugObject d_obj;
} BStringMap;

void BStringMap_Init (BStringMap *o);
int BStringMap_InitCopy (BStringMap *o, const BStringMap *src) WARN_UNUSED;
void BStringMap_Free (BStringMap *o);
const char * BStringMap_Get (const BStringMap *o, const char *key);
int BStringMap_Set (BStringMap *o, const char *key, const char *value) WARN_UNUSED;
void BStringMap_Unset (BStringMap *o, const char *key);
const char * BStringMap_First (const BStringMap *o);
const char * BStringMap_Next (const BStringMap *o, const char *key);

#endif
