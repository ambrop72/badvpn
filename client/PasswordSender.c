/**
 * @file PasswordSender.c
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

#include <misc/debug.h>

#include <client/PasswordSender.h>

#define COMPONENT_SINK 1

static void call_handler (PasswordSender *o, int is_error)
{
    DEBUGERROR(&o->d_err, o->handler(o->user, is_error))
}

static void error_handler (PasswordSender *o, int component, const void *data)
{
    ASSERT(component == COMPONENT_SINK)
    DebugObject_Access(&o->d_obj);
    
    call_handler(o, 1);
    return;
}

static void sent_handler (PasswordSender *o)
{
    DebugObject_Access(&o->d_obj);
    
    call_handler(o, 0);
    return;
}

void PasswordSender_Init (PasswordSender *o, uint64_t password, int ssl, BSocket *plain_sock, BPRFileDesc *ssl_bprfd, PasswordSender_handler handler, void *user, BReactor *reactor)
{
    ASSERT(ssl == 0 || ssl == 1)
    
    // init arguments
    o->password = password;
    o->ssl = ssl;
    if (ssl) {
        o->ssl_bprfd = ssl_bprfd;
    } else {
        o->plain_sock = plain_sock;
    }
    o->handler = handler;
    o->user = user;
    
    // init error handler
    FlowErrorDomain_Init(&o->domain, (FlowErrorDomain_handler)error_handler, o);
    
    // init sink
    StreamPassInterface *sink_if;
    if (o->ssl) {
        PRStreamSink_Init(&o->sink.ssl, FlowErrorReporter_Create(&o->domain, COMPONENT_SINK), o->ssl_bprfd, BReactor_PendingGroup(reactor));
        sink_if = PRStreamSink_GetInput(&o->sink.ssl);
    } else {
        StreamSocketSink_Init(&o->sink.plain, FlowErrorReporter_Create(&o->domain, COMPONENT_SINK), o->plain_sock, BReactor_PendingGroup(reactor));
        sink_if = StreamSocketSink_GetInput(&o->sink.plain);
    }
    
    // init PacketStreamSender
    PacketStreamSender_Init(&o->pss, sink_if, sizeof(o->password), BReactor_PendingGroup(reactor));
    
    // init SinglePacketSender
    SinglePacketSender_Init(&o->sps, (uint8_t *)&o->password, sizeof(o->password), PacketStreamSender_GetInput(&o->pss), (SinglePacketSender_handler)sent_handler, o, BReactor_PendingGroup(reactor));
    
    DebugObject_Init(&o->d_obj);
    DebugError_Init(&o->d_err, BReactor_PendingGroup(reactor));
}

void PasswordSender_Free (PasswordSender *o)
{
    DebugError_Free(&o->d_err);
    DebugObject_Free(&o->d_obj);
    
    // free SinglePacketSender
    SinglePacketSender_Free(&o->sps);
    
    // free PacketStreamSender
    PacketStreamSender_Free(&o->pss);
    
    // free sink
    if (o->ssl) {
        PRStreamSink_Free(&o->sink.ssl);
    } else {
        StreamSocketSink_Free(&o->sink.plain);
    }
}
