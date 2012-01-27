/**
 * @file NCDModuleIndex.h
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BADVPN_NCDMODULEINDEX_H
#define BADVPN_NCDMODULEINDEX_H

#include <misc/debug.h>
#include <structure/BAVL.h>
#include <base/DebugObject.h>
#include <ncd/NCDModule.h>

#define NCDMODULEINDEX_MAX_TYPE_LEN 63

typedef struct {
    BAVL modules_tree;
    BAVL base_types_tree;
    DebugObject d_obj;
} NCDModuleIndex;

struct NCDModuleIndex_module {
    char type[NCDMODULEINDEX_MAX_TYPE_LEN + 1];
    const struct NCDModule *module;
    BAVLNode modules_tree_node;
};

struct NCDModuleIndex_base_type {
    const char *base_type;
    const struct NCDModuleGroup *group;
    BAVLNode base_types_tree_node;
};

void NCDModuleIndex_Init (NCDModuleIndex *o);
void NCDModuleIndex_Free (NCDModuleIndex *o);
int NCDModuleIndex_AddGroup (NCDModuleIndex *o, const struct NCDModuleGroup *group) WARN_UNUSED;
const struct NCDModule * NCDModuleIndex_FindModule (NCDModuleIndex *o, const char *type);

#endif
