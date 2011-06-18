/**
 * @file NCDUdevMonitor.h
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

#ifndef BADVPN_UDEVMONITOR_NCDUDEVMONITOR_H
#define BADVPN_UDEVMONITOR_NCDUDEVMONITOR_H

#include <misc/debug.h>
#include <misc/debugerror.h>
#include <flow/StreamRecvConnector.h>
#include <system/BInputProcess.h>
#include <udevmonitor/NCDUdevMonitorParser.h>

typedef void (*NCDUdevMonitor_handler_event) (void *user);
typedef void (*NCDUdevMonitor_handler_error) (void *user, int is_error);

typedef struct {
    void *user;
    NCDUdevMonitor_handler_event handler_event;
    NCDUdevMonitor_handler_error handler_error;
    BInputProcess process;
    int process_running;
    int process_was_error;
    int input_running;
    int input_was_error;
    StreamRecvConnector connector;
    NCDUdevMonitorParser parser;
    DebugObject d_obj;
    DebugError d_err;
} NCDUdevMonitor;

int NCDUdevMonitor_Init (NCDUdevMonitor *o, BReactor *reactor, BProcessManager *manager, int is_info_mode, void *user,
                         NCDUdevMonitor_handler_event handler_event,
                         NCDUdevMonitor_handler_error handler_error) WARN_UNUSED;
void NCDUdevMonitor_Free (NCDUdevMonitor *o);
void NCDUdevMonitor_Done (NCDUdevMonitor *o);
void NCDUdevMonitor_AssertReady (NCDUdevMonitor *o);
int NCDUdevMonitor_IsReadyEvent (NCDUdevMonitor *o);
int NCDUdevMonitor_GetNumProperties (NCDUdevMonitor *o);
void NCDUdevMonitor_GetProperty (NCDUdevMonitor *o, int index, const char **name, const char **value);

#endif
