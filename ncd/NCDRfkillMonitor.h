/**
 * @file NCDRfkillMonitor.h
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

#ifndef BADVPN_NCD_NCDRFKILLMONITOR_H
#define BADVPN_NCD_NCDRFKILLMONITOR_H

#include <linux/rfkill.h>

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <system/BReactor.h>

typedef void (*NCDRfkillMonitor_handler) (void *user, struct rfkill_event event);

typedef struct {
    BReactor *reactor;
    NCDRfkillMonitor_handler handler;
    void *user;
    int rfkill_fd;
    BFileDescriptor bfd;
    DebugObject d_obj;
} NCDRfkillMonitor;

int NCDRfkillMonitor_Init (NCDRfkillMonitor *o, BReactor *reactor, NCDRfkillMonitor_handler handler, void *user) WARN_UNUSED;
void NCDRfkillMonitor_Free (NCDRfkillMonitor *o);

#endif
