/**
 * @file NCDRequest.h
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

#ifndef BADVPN_NCDREQUEST_H
#define BADVPN_NCDREQUEST_H

#include <stdint.h>

#include <misc/debug.h>
#include <misc/debugerror.h>
#include <base/DebugObject.h>
#include <system/BConnection.h>
#include <flow/PacketProtoDecoder.h>
#include <flow/PacketStreamSender.h>
#include <ncd/NCDValueGenerator.h>
#include <ncd/NCDValueParser.h>

typedef void (*NCDRequest_handler_finished) (void *user, int is_error);
typedef void (*NCDRequest_handler_reply) (void *user, NCDValue reply_data);

typedef struct {
    BReactor *reactor;
    void *user;
    NCDRequest_handler_finished handler_finished;
    NCDRequest_handler_reply handler_reply;
    uint32_t request_id;
    uint8_t *request_data;
    int request_len;
    BConnector connector;
    BConnection con;
    PacketPassInterface recv_if;
    PacketProtoDecoder recv_decoder;
    PacketStreamSender send_sender;
    PacketPassInterface *send_sender_iface;
    int state;
    int processing;
    DebugError d_err;
    DebugObject d_obj;
} NCDRequest;

int NCDRequest_Init (NCDRequest *o, const char *socket_path, NCDValue *payload_value, BReactor *reactor, void *user, NCDRequest_handler_finished handler_finished, NCDRequest_handler_reply handler_reply) WARN_UNUSED;
void NCDRequest_Free (NCDRequest *o);
void NCDRequest_Next (NCDRequest *o);

#endif
