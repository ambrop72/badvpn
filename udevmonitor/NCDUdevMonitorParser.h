/**
 * @file NCDUdevMonitorParser.h
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

#ifndef BADVPN_UDEVMONITOR_NCDUDEVMONITORPARSER_H
#define BADVPN_UDEVMONITOR_NCDUDEVMONITORPARSER_H

#include <stdint.h>
#include <regex.h>

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <flow/StreamRecvInterface.h>

typedef void (*NCDUdevMonitorParser_handler) (void *user);

struct NCDUdevMonitorParser_property {
    char *name;
    char *value;
};

typedef struct {
    StreamRecvInterface *input;
    int buf_size;
    int max_properties;
    int is_info_mode;
    void *user;
    NCDUdevMonitorParser_handler handler;
    regex_t property_regex;
    BPending done_job;
    uint8_t *buf;
    int buf_used;
    int is_ready;
    int ready_len;
    int ready_is_ready_event;
    struct NCDUdevMonitorParser_property *ready_properties;
    int ready_num_properties;
    DebugObject d_obj;
} NCDUdevMonitorParser;

int NCDUdevMonitorParser_Init (NCDUdevMonitorParser *o, StreamRecvInterface *input, int buf_size, int max_properties,
                               int is_info_mode, BPendingGroup *pg, void *user,
                               NCDUdevMonitorParser_handler handler) WARN_UNUSED;
void NCDUdevMonitorParser_Free (NCDUdevMonitorParser *o);
void NCDUdevMonitorParser_AssertReady (NCDUdevMonitorParser *o);
void NCDUdevMonitorParser_Done (NCDUdevMonitorParser *o);
int NCDUdevMonitorParser_IsReadyEvent (NCDUdevMonitorParser *o);
int NCDUdevMonitorParser_GetNumProperties (NCDUdevMonitorParser *o);
void NCDUdevMonitorParser_GetProperty (NCDUdevMonitorParser *o, int index, const char **name, const char **value);

#endif
