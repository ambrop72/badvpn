/**
 * @file PeerChat.h
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

#ifndef BADVPN_PEERCHAT_H
#define BADVPN_PEERCHAT_H

#include <cert.h>
#include <keyhi.h>

#include <protocol/packetproto.h>
#include <protocol/scproto.h>
#include <misc/debug.h>
#include <misc/debugerror.h>
#include <base/DebugObject.h>
#include <base/BPending.h>
#include <base/BLog.h>
#include <flow/SinglePacketSender.h>
#include <flow/PacketProtoEncoder.h>
#include <flow/PacketCopier.h>
#include <flow/StreamPacketSender.h>
#include <flow/PacketStreamSender.h>
#include <flow/SinglePacketBuffer.h>
#include <flow/PacketProtoDecoder.h>
#include <flow/PacketBuffer.h>
#include <flow/BufferWriter.h>
#include <nspr_support/BSSLConnection.h>
#include <client/SCOutmsgEncoder.h>
#include <client/SimpleStreamBuffer.h>

#define PEERCHAT_SSL_NONE 0
#define PEERCHAT_SSL_CLIENT 1
#define PEERCHAT_SSL_SERVER 2

#define PEERCHAT_SSL_RECV_BUF_SIZE 4096
#define PEERCHAT_SEND_BUF_SIZE 200

//#define PEERCHAT_SIMULATE_ERROR 40

typedef void (*PeerChat_handler_error) (void *user);
typedef void (*PeerChat_handler_message) (void *user, uint8_t *data, int data_len);

typedef struct {
    int ssl_mode;
    CERTCertificate *ssl_cert;
    SECKEYPrivateKey *ssl_key;
    uint8_t *ssl_peer_cert;
    int ssl_peer_cert_len;
    void *user;
    BLog_logfunc logfunc;
    PeerChat_handler_error handler_error;
    PeerChat_handler_message handler_message;
    
    // transport
    PacketProtoEncoder pp_encoder;
    SCOutmsgEncoder sc_encoder;
    PacketCopier copier;
    BPending recv_job;
    uint8_t *recv_data;
    int recv_data_len;
    
    // SSL transport
    StreamPacketSender ssl_sp_sender;
    SimpleStreamBuffer ssl_recv_buf;
    
    // SSL connection
    PRFileDesc ssl_bottom_prfd;
    PRFileDesc *ssl_prfd;
    BSSLConnection ssl_con;
    
    // SSL higher layer
    PacketStreamSender ssl_ps_sender;
    SinglePacketBuffer ssl_buffer;
    PacketProtoEncoder ssl_encoder;
    PacketCopier ssl_copier;
    PacketProtoDecoder ssl_recv_decoder;
    PacketPassInterface ssl_recv_if;
    
    // higher layer send buffer
    PacketBuffer send_buf;
    BufferWriter send_writer;
    
    DebugError d_err;
    DebugObject d_obj;
} PeerChat;

int PeerChat_Init (PeerChat *o, peerid_t peer_id, int ssl_mode, CERTCertificate *ssl_cert, SECKEYPrivateKey *ssl_key,
                   uint8_t *ssl_peer_cert, int ssl_peer_cert_len, BPendingGroup *pg, void *user,
                   BLog_logfunc logfunc,
                   PeerChat_handler_error handler_error,
                   PeerChat_handler_message handler_message) WARN_UNUSED;
void PeerChat_Free (PeerChat *o);
PacketRecvInterface * PeerChat_GetSendOutput (PeerChat *o);
void PeerChat_InputReceived (PeerChat *o, uint8_t *data, int data_len);
int PeerChat_StartMessage (PeerChat *o, uint8_t **data) WARN_UNUSED;
void PeerChat_EndMessage (PeerChat *o, int data_len);

#endif
