/**
 * @file SingleStreamSender.h
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

#ifndef BADVPN_SINGLESTREAMSENDER_H
#define BADVPN_SINGLESTREAMSENDER_H

#include <misc/debugerror.h>
#include <base/DebugObject.h>
#include <flow/StreamPassInterface.h>

typedef void (*SingleStreamSender_handler) (void *user);

typedef struct {
    uint8_t *packet;
    int packet_len;
    StreamPassInterface *output;
    void *user;
    SingleStreamSender_handler handler;
    int pos;
    DebugError d_err;
    DebugObject d_obj;
} SingleStreamSender;

void SingleStreamSender_Init (SingleStreamSender *o, uint8_t *packet, int packet_len, StreamPassInterface *output, BPendingGroup *pg, void *user, SingleStreamSender_handler handler);
void SingleStreamSender_Free (SingleStreamSender *o);

#endif
