/**
 * @file NCDModuleIndex.h
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

#ifndef BADVPN_NCDMODULEINDEX_H
#define BADVPN_NCDMODULEINDEX_H

#include <misc/debug.h>
#include <structure/BAVL.h>
#include <base/DebugObject.h>
#include <ncd/NCDModule.h>

#define NCDMODULEINDEX_MAX_TYPE_LEN 64

typedef struct {
    BAVL modules_tree;
    DebugObject d_obj;
} NCDModuleIndex;

struct NCDModuleIndex_module {
    char type[NCDMODULEINDEX_MAX_TYPE_LEN + 1];
    const struct NCDModule *module;
    BAVLNode modules_tree_node;
};

void NCDModuleIndex_Init (NCDModuleIndex *o);
void NCDModuleIndex_Free (NCDModuleIndex *o);
int NCDModuleIndex_AddGroup (NCDModuleIndex *o, const struct NCDModuleGroup *group) WARN_UNUSED;
const struct NCDModule * NCDModuleIndex_FindModule (NCDModuleIndex *o, const char *type);

#endif
