/**
 * @file DatagramPeerIO.c
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

#include <stdint.h>

#include <misc/debug.h>
#include <system/BLog.h>

#include <client/DatagramPeerIO.h>

#include <generated/blog_channel_DatagramPeerIO.h>

#define DATAGRAMPEERIO_MODE_NONE 0
#define DATAGRAMPEERIO_MODE_CONNECT 1
#define DATAGRAMPEERIO_MODE_BIND 2

#define DATAGRAMPEERIO_COMPONENT_SINK 1
#define DATAGRAMPEERIO_COMPONENT_SOURCE 2

static void init_sending (DatagramPeerIO *o, BAddr addr, BIPAddr local_addr);
static void free_sending (DatagramPeerIO *o);
static void init_receiving (DatagramPeerIO *o);
static void free_receiving (DatagramPeerIO *o);
static void error_handler (DatagramPeerIO *o, int component, int code);
static void reset_mode (DatagramPeerIO *o);
static void recv_decoder_notifier_handler (DatagramPeerIO *o, uint8_t *data, int data_len);

void init_sending (DatagramPeerIO *o, BAddr addr, BIPAddr local_addr)
{
    // init sink
    DatagramSocketSink_Init(&o->send_sink, FlowErrorReporter_Create(&o->domain, DATAGRAMPEERIO_COMPONENT_SINK), &o->sock, o->effective_socket_mtu, addr, local_addr, BReactor_PendingGroup(o->reactor));
    
    // connect sink
    PacketPassConnector_ConnectOutput(&o->send_connector, DatagramSocketSink_GetInput(&o->send_sink));
}

void free_sending (DatagramPeerIO *o)
{
    // disconnect sink
    PacketPassConnector_DisconnectOutput(&o->send_connector);
    
    // free sink
    DatagramSocketSink_Free(&o->send_sink);
}

void init_receiving (DatagramPeerIO *o)
{
    // init source
    DatagramSocketSource_Init(&o->recv_source, FlowErrorReporter_Create(&o->domain, DATAGRAMPEERIO_COMPONENT_SOURCE), &o->sock, o->effective_socket_mtu, BReactor_PendingGroup(o->reactor));
    
    // connect source
    PacketRecvConnector_ConnectInput(&o->recv_connector, DatagramSocketSource_GetOutput(&o->recv_source));
}

void free_receiving (DatagramPeerIO *o)
{
    // disconnect source
    PacketRecvConnector_DisconnectInput(&o->recv_connector);
    
    // free source
    DatagramSocketSource_Free(&o->recv_source);
}

void error_handler (DatagramPeerIO *o, int component, int code)
{
    ASSERT(o->mode == DATAGRAMPEERIO_MODE_CONNECT || o->mode == DATAGRAMPEERIO_MODE_BIND)
    DebugObject_Access(&o->d_obj);
    
    BLog(BLOG_NOTICE, "error");
}

void reset_mode (DatagramPeerIO *o)
{
    ASSERT(o->mode == DATAGRAMPEERIO_MODE_NONE || o->mode == DATAGRAMPEERIO_MODE_CONNECT || o->mode == DATAGRAMPEERIO_MODE_BIND)
    
    if (o->mode == DATAGRAMPEERIO_MODE_NONE) {
        return;
    }
    
    // free sending
    if (o->mode == DATAGRAMPEERIO_MODE_CONNECT || o->bind_sending_up) {
        free_sending(o);
    }
    
    // remove recv notifier handler
    PacketPassNotifier_SetHandler(&o->recv_notifier, NULL, NULL);
    
    // free receiving
    free_receiving(o);
    
    // free socket
    BSocket_Free(&o->sock);
    
    // set mode
    o->mode = DATAGRAMPEERIO_MODE_NONE;
}

void recv_decoder_notifier_handler (DatagramPeerIO *o, uint8_t *data, int data_len)
{
    ASSERT(o->mode == DATAGRAMPEERIO_MODE_BIND)
    DebugObject_Access(&o->d_obj);
    
    // obtain addresses from last received packet
    BAddr addr;
    BIPAddr local_addr;
    DatagramSocketSource_GetLastAddresses(&o->recv_source, &addr, &local_addr);
    
    if (!o->bind_sending_up) {
        // init sending
        init_sending(o, addr, local_addr);
        
        // set sending up
        o->bind_sending_up = 1;
    } else {
        // update addresses
        DatagramSocketSink_SetAddresses(&o->send_sink, addr, local_addr);
    }
}

int DatagramPeerIO_Init (
    DatagramPeerIO *o,
    BReactor *reactor,
    int payload_mtu,
    int socket_mtu,
    struct spproto_security_params sp_params,
    btime_t latency,
    int num_frames,
    PacketPassInterface *recv_userif,
    int otp_warning_count,
    DatagramPeerIO_handler_otp_warning handler_otp_warning,
    DatagramPeerIO_handler_otp_ready handler_otp_ready,
    void *user,
    BThreadWorkDispatcher *twd
)
{
    ASSERT(payload_mtu >= 0)
    ASSERT(socket_mtu >= 0)
    spproto_assert_security_params(sp_params);
    ASSERT(num_frames > 0)
    ASSERT(PacketPassInterface_GetMTU(recv_userif) >= payload_mtu)
    if (SPPROTO_HAVE_OTP(sp_params)) {
        ASSERT(otp_warning_count > 0)
        ASSERT(otp_warning_count <= sp_params.otp_num)
        ASSERT(handler_otp_warning)
    }
    
    // set parameters
    o->reactor = reactor;
    o->payload_mtu = payload_mtu;
    o->sp_params = sp_params;
    
    // check payload MTU (for FragmentProto)
    if (o->payload_mtu > UINT16_MAX) {
        BLog(BLOG_ERROR, "payload MTU is too big");
        goto fail0;
    }
    
    // calculate SPProto payload MTU
    if ((o->spproto_payload_mtu = spproto_payload_mtu_for_carrier_mtu(o->sp_params, socket_mtu)) <= (int)sizeof(struct fragmentproto_chunk_header)) {
        BLog(BLOG_ERROR, "socket MTU is too small");
        goto fail0;
    }
    
    // calculate effective socket MTU
    if ((o->effective_socket_mtu = spproto_carrier_mtu_for_payload_mtu(o->sp_params, o->spproto_payload_mtu)) < 0) {
        BLog(BLOG_ERROR, "spproto_carrier_mtu_for_payload_mtu failed !?");
        goto fail0;
    }
    
    // init error domain
    FlowErrorDomain_Init(&o->domain, (FlowErrorDomain_handler)error_handler, o);
    
    // init receiving
    
    // init assembler
    if (!FragmentProtoAssembler_Init(&o->recv_assembler, o->spproto_payload_mtu, recv_userif, num_frames, fragmentproto_max_chunks_for_frame(o->spproto_payload_mtu, o->payload_mtu), BReactor_PendingGroup(o->reactor))) {
        BLog(BLOG_ERROR, "FragmentProtoAssembler_Init failed");
        goto fail0;
    }
    
    // init notifier
    PacketPassNotifier_Init(&o->recv_notifier, FragmentProtoAssembler_GetInput(&o->recv_assembler), BReactor_PendingGroup(o->reactor));
    
    // init decoder
    if (!SPProtoDecoder_Init(&o->recv_decoder, PacketPassNotifier_GetInput(&o->recv_notifier), o->sp_params, 2, BReactor_PendingGroup(o->reactor), twd, handler_otp_ready, user)) {
        BLog(BLOG_ERROR, "SPProtoDecoder_Init failed");
        goto fail1;
    }
    
    // init connector
    PacketRecvConnector_Init(&o->recv_connector, o->effective_socket_mtu, BReactor_PendingGroup(o->reactor));
    
    // init buffer
    if (!SinglePacketBuffer_Init(&o->recv_buffer, PacketRecvConnector_GetOutput(&o->recv_connector), SPProtoDecoder_GetInput(&o->recv_decoder), BReactor_PendingGroup(o->reactor))) {
        BLog(BLOG_ERROR, "SinglePacketBuffer_Init failed");
        goto fail2;
    }
    
    // init sending base
    
    // init disassembler
    FragmentProtoDisassembler_Init(&o->send_disassembler, o->reactor, o->payload_mtu, o->spproto_payload_mtu, -1, latency);
    
    // init encoder
    if (!SPProtoEncoder_Init(&o->send_encoder, FragmentProtoDisassembler_GetOutput(&o->send_disassembler), o->sp_params, otp_warning_count, handler_otp_warning, user, BReactor_PendingGroup(o->reactor), twd)) {
        BLog(BLOG_ERROR, "SPProtoEncoder_Init failed");
        goto fail3;
    }
    
    // init connector
    PacketPassConnector_Init(&o->send_connector, o->effective_socket_mtu, BReactor_PendingGroup(o->reactor));
    
    // init buffer
    if (!SinglePacketBuffer_Init(&o->send_buffer, SPProtoEncoder_GetOutput(&o->send_encoder), PacketPassConnector_GetInput(&o->send_connector), BReactor_PendingGroup(o->reactor))) {
        BLog(BLOG_ERROR, "SinglePacketBuffer_Init failed");
        goto fail4;
    }
    
    // set mode
    o->mode = DATAGRAMPEERIO_MODE_NONE;
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail4:
    PacketPassConnector_Free(&o->send_connector);
    SPProtoEncoder_Free(&o->send_encoder);
fail3:
    FragmentProtoDisassembler_Free(&o->send_disassembler);
    SinglePacketBuffer_Free(&o->recv_buffer);
fail2:
    PacketRecvConnector_Free(&o->recv_connector);
    SPProtoDecoder_Free(&o->recv_decoder);
fail1:
    PacketPassNotifier_Free(&o->recv_notifier);
    FragmentProtoAssembler_Free(&o->recv_assembler);
fail0:
    return 0;
}

void DatagramPeerIO_Free (DatagramPeerIO *o)
{
    DebugObject_Free(&o->d_obj);

    // reset mode
    reset_mode(o);
    
    // free sending base
    SinglePacketBuffer_Free(&o->send_buffer);
    PacketPassConnector_Free(&o->send_connector);
    SPProtoEncoder_Free(&o->send_encoder);
    FragmentProtoDisassembler_Free(&o->send_disassembler);
    
    // free receiving
    SinglePacketBuffer_Free(&o->recv_buffer);
    PacketRecvConnector_Free(&o->recv_connector);
    SPProtoDecoder_Free(&o->recv_decoder);
    PacketPassNotifier_Free(&o->recv_notifier);
    FragmentProtoAssembler_Free(&o->recv_assembler);
}

PacketPassInterface * DatagramPeerIO_GetSendInput (DatagramPeerIO *o)
{
    DebugObject_Access(&o->d_obj);
    
    return FragmentProtoDisassembler_GetInput(&o->send_disassembler);
}

int DatagramPeerIO_Connect (DatagramPeerIO *o, BAddr addr)
{
    ASSERT(!BAddr_IsInvalid(&addr))
    DebugObject_Access(&o->d_obj);
    
    // reset mode
    reset_mode(o);
    
    // init socket
    if (BSocket_Init(&o->sock, o->reactor, addr.type, BSOCKET_TYPE_DGRAM) < 0) {
        BLog(BLOG_ERROR, "BSocket_Init failed");
        goto fail1;
    }
    
    // connect the socket
    // Windows needs this or receive will fail; however, FreeBSD will refuse to send
    // if this is done
    #ifdef BADVPN_USE_WINAPI
    if (BSocket_Connect(&o->sock, &addr, 0) < 0) {
        BLog(BLOG_ERROR, "BSocket_Connect failed");
        goto fail2;
    }
    #endif
    
    // init receiving
    init_receiving(o);
    
    // init sending
    BIPAddr local_addr;
    BIPAddr_InitInvalid(&local_addr);
    init_sending(o, addr, local_addr);
    
    // set mode
    o->mode = DATAGRAMPEERIO_MODE_CONNECT;
    
    return 1;
    
fail2:
    BSocket_Free(&o->sock);
fail1:
    return 0;
}

int DatagramPeerIO_Bind (DatagramPeerIO *o, BAddr addr)
{
    ASSERT(!BAddr_IsInvalid(&addr))
    DebugObject_Access(&o->d_obj);
    
    // reset mode
    reset_mode(o);
    
    // init socket
    if (BSocket_Init(&o->sock, o->reactor, addr.type, BSOCKET_TYPE_DGRAM) < 0) {
        BLog(BLOG_ERROR, "BSocket_Init failed");
        goto fail1;
    }
    
    // bind socket
    if (BSocket_Bind(&o->sock, &addr) < 0) {
        BLog(BLOG_INFO, "BSocket_Bind failed");
        goto fail2;
    }
    
    // init receiving
    init_receiving(o);
    
    // set recv notifier handler
    PacketPassNotifier_SetHandler(&o->recv_notifier, (PacketPassNotifier_handler_notify)recv_decoder_notifier_handler, o);
    
    // set mode
    o->mode = DATAGRAMPEERIO_MODE_BIND;
    
    // set sending not up
    o->bind_sending_up = 0;
    
    return 1;
    
fail2:
    BSocket_Free(&o->sock);
fail1:
    return 0;
}

void DatagramPeerIO_SetEncryptionKey (DatagramPeerIO *o, uint8_t *encryption_key)
{
    ASSERT(SPPROTO_HAVE_ENCRYPTION(o->sp_params))
    DebugObject_Access(&o->d_obj);
    
    // set sending key
    SPProtoEncoder_SetEncryptionKey(&o->send_encoder, encryption_key);
    
    // set receiving key
    SPProtoDecoder_SetEncryptionKey(&o->recv_decoder, encryption_key);
}

void DatagramPeerIO_RemoveEncryptionKey (DatagramPeerIO *o)
{
    ASSERT(SPPROTO_HAVE_ENCRYPTION(o->sp_params))
    DebugObject_Access(&o->d_obj);
    
    // remove sending key
    SPProtoEncoder_RemoveEncryptionKey(&o->send_encoder);
    
    // remove receiving key
    SPProtoDecoder_RemoveEncryptionKey(&o->recv_decoder);
}

void DatagramPeerIO_SetOTPSendSeed (DatagramPeerIO *o, uint16_t seed_id, uint8_t *key, uint8_t *iv)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    DebugObject_Access(&o->d_obj);
    
    // set sending seed
    SPProtoEncoder_SetOTPSeed(&o->send_encoder, seed_id, key, iv);
}

void DatagramPeerIO_RemoveOTPSendSeed (DatagramPeerIO *o)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    DebugObject_Access(&o->d_obj);
    
    // remove sending seed
    SPProtoEncoder_RemoveOTPSeed(&o->send_encoder);
}

void DatagramPeerIO_AddOTPRecvSeed (DatagramPeerIO *o, uint16_t seed_id, uint8_t *key, uint8_t *iv)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    DebugObject_Access(&o->d_obj);
    
    // add receiving seed
    SPProtoDecoder_AddOTPSeed(&o->recv_decoder, seed_id, key, iv);
}

void DatagramPeerIO_RemoveOTPRecvSeeds (DatagramPeerIO *o)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    DebugObject_Access(&o->d_obj);
    
    // remove receiving seeds
    SPProtoDecoder_RemoveOTPSeeds(&o->recv_decoder);
}
