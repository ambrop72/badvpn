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

static int init_persistent_io (DatagramPeerIO *o, btime_t latency, PacketPassInterface *recv_userif);
static void free_persistent_io (DatagramPeerIO *o);
static void init_sending (DatagramPeerIO *o, BAddr addr, BIPAddr local_addr);
static void free_sending (DatagramPeerIO *o);
static void init_receiving (DatagramPeerIO *o);
static void free_receiving (DatagramPeerIO *o);
static void error_handler (DatagramPeerIO *o, int component, const void *data);
static void reset_mode (DatagramPeerIO *o);
static void recv_decoder_notifier_handler (DatagramPeerIO *o, uint8_t *data, int data_len);
static void send_encoder_notifier_handler (DatagramPeerIO *o, uint8_t *data, int data_len);

int init_persistent_io (DatagramPeerIO *o, btime_t latency, PacketPassInterface *recv_userif)
{
    // init error domain
    FlowErrorDomain_Init(&o->domain, (FlowErrorDomain_handler)error_handler, o);
    
    // init sending base
    
    // init disassembler
    FragmentProtoDisassembler_Init(&o->send_disassembler, o->reactor, o->payload_mtu, o->spproto_payload_mtu, -1, latency);
    
    // init encoder
    if (!SPProtoEncoder_Init(&o->send_encoder, o->sp_params, FragmentProtoDisassembler_GetOutput(&o->send_disassembler), BReactor_PendingGroup(o->reactor))) {
        BLog(BLOG_ERROR, "SPProtoEncoder_Init failed");
        goto fail1;
    }
    
    // init notifier
    PacketRecvNotifier_Init(&o->send_notifier, SPProtoEncoder_GetOutput(&o->send_encoder), BReactor_PendingGroup(o->reactor));
    if (SPPROTO_HAVE_OTP(o->sp_params)) {
        PacketRecvNotifier_SetHandler(&o->send_notifier, (PacketRecvNotifier_handler_notify)send_encoder_notifier_handler, o);
    }
    
    // init connector
    PacketPassConnector_Init(&o->send_connector, o->effective_socket_mtu, BReactor_PendingGroup(o->reactor));
    
    // init buffer
    if (!SinglePacketBuffer_Init(&o->send_buffer, PacketRecvNotifier_GetOutput(&o->send_notifier), PacketPassConnector_GetInput(&o->send_connector), BReactor_PendingGroup(o->reactor))) {
        BLog(BLOG_ERROR, "SinglePacketBuffer_Init failed");
        goto fail2;
    }
    
    // init receiving
    
    // init assembler
    if (!FragmentProtoAssembler_Init(&o->recv_assembler, o->spproto_payload_mtu, recv_userif, 1, fragmentproto_max_chunks_for_frame(o->spproto_payload_mtu, o->payload_mtu), BReactor_PendingGroup(o->reactor))) {
        goto fail3;
    }
    
    // init notifier
    PacketPassNotifier_Init(&o->recv_notifier, FragmentProtoAssembler_GetInput(&o->recv_assembler), BReactor_PendingGroup(o->reactor));
    
    // init decoder
    if (!SPProtoDecoder_Init(&o->recv_decoder, PacketPassNotifier_GetInput(&o->recv_notifier), o->sp_params, 2, BReactor_PendingGroup(o->reactor))) {
        goto fail4;
    }
    
    // init connector
    PacketRecvConnector_Init(&o->recv_connector, o->effective_socket_mtu, BReactor_PendingGroup(o->reactor));
    
    // init buffer
    if (!SinglePacketBuffer_Init(&o->recv_buffer, PacketRecvConnector_GetOutput(&o->recv_connector), SPProtoDecoder_GetInput(&o->recv_decoder), BReactor_PendingGroup(o->reactor))) {
        goto fail5;
    }
    
    return 1;
    
fail5:
    PacketRecvConnector_Free(&o->recv_connector);
    SPProtoDecoder_Free(&o->recv_decoder);
fail4:
    PacketPassNotifier_Free(&o->recv_notifier);
    FragmentProtoAssembler_Free(&o->recv_assembler);
fail3:
    SinglePacketBuffer_Free(&o->send_buffer);
fail2:
    PacketPassConnector_Free(&o->send_connector);
    PacketRecvNotifier_Free(&o->send_notifier);
    SPProtoEncoder_Free(&o->send_encoder);
fail1:
    FragmentProtoDisassembler_Free(&o->send_disassembler);
    return 0;
}

void free_persistent_io (DatagramPeerIO *o)
{
    // free receiving
    SinglePacketBuffer_Free(&o->recv_buffer);
    PacketRecvConnector_Free(&o->recv_connector);
    SPProtoDecoder_Free(&o->recv_decoder);
    PacketPassNotifier_Free(&o->recv_notifier);
    FragmentProtoAssembler_Free(&o->recv_assembler);
    
    // free sending base
    SinglePacketBuffer_Free(&o->send_buffer);
    PacketPassConnector_Free(&o->send_connector);
    PacketRecvNotifier_Free(&o->send_notifier);
    SPProtoEncoder_Free(&o->send_encoder);
    FragmentProtoDisassembler_Free(&o->send_disassembler);
}

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

void error_handler (DatagramPeerIO *o, int component, const void *data)
{
    ASSERT(o->mode == DATAGRAMPEERIO_MODE_CONNECT || o->mode == DATAGRAMPEERIO_MODE_BIND)
    
    int error = *((int *)data);
    
    switch (component) {
        case DATAGRAMPEERIO_COMPONENT_SINK:
            switch (error) {
                case DATAGRAMSOCKETSINK_ERROR_BSOCKET:
                    BLog(BLOG_NOTICE, "sink BSocket error %d", BSocket_GetError(&o->sock));
                    break;
                case DATAGRAMSOCKETSINK_ERROR_WRONGSIZE:
                    BLog(BLOG_NOTICE, "sink wrong size error");
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case DATAGRAMPEERIO_COMPONENT_SOURCE:
            switch (error) {
                case DATAGRAMSOCKETSOURCE_ERROR_BSOCKET:
                    BLog(BLOG_NOTICE, "source BSocket error %d", BSocket_GetError(&o->sock));
                    break;
                default:
                    ASSERT(0);
            }
            break;
        default:
            ASSERT(0);
    }
}

void reset_mode (DatagramPeerIO *o)
{
    switch (o->mode) {
        case DATAGRAMPEERIO_MODE_NONE:
            break;
        case DATAGRAMPEERIO_MODE_CONNECT:
            // kill mode dead var
            DEAD_KILL(o->mode_dead);
            // set default mode
            o->mode = DATAGRAMPEERIO_MODE_NONE;
            // free receiving
            free_receiving(o);
            // free sending
            free_sending(o);
            // free socket
            BSocket_Free(&o->sock);
            break;
        case DATAGRAMPEERIO_MODE_BIND:
            // kill mode dead var
            DEAD_KILL(o->mode_dead);
            // set default mode
            o->mode = DATAGRAMPEERIO_MODE_NONE;
            // remove recv notifier handler
            PacketPassNotifier_SetHandler(&o->recv_notifier, NULL, NULL);
            // free receiving
            free_receiving(o);
            // free sending
            if (o->bind_sending_up) {
                free_sending(o);
            }
            // free socket
            BSocket_Free(&o->sock);
            break;
        default:
            ASSERT(0);
    }
}

void recv_decoder_notifier_handler (DatagramPeerIO *o, uint8_t *data, int data_len)
{
    ASSERT(o->mode == DATAGRAMPEERIO_MODE_BIND)
    
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

void send_encoder_notifier_handler (DatagramPeerIO *o, uint8_t *data, int data_len)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    
    if (o->handler_otp_warning && SPProtoEncoder_GetOTPPosition(&o->send_encoder) == o->handler_otp_warning_num_used) { 
        o->handler_otp_warning(o->handler_otp_warning_user);
        return;
    }
}

int DatagramPeerIO_Init (DatagramPeerIO *o, BReactor *reactor, int payload_mtu, int socket_mtu, struct spproto_security_params sp_params, btime_t latency, PacketPassInterface *recv_userif)
{
    ASSERT(payload_mtu >= 0)
    ASSERT(payload_mtu <= UINT16_MAX)
    ASSERT(socket_mtu >= 0)
    ASSERT(spproto_validate_security_params(sp_params))
    ASSERT(spproto_payload_mtu_for_carrier_mtu(sp_params, socket_mtu) > sizeof(struct fragmentproto_chunk_header))
    ASSERT(PacketPassInterface_GetMTU(recv_userif) >= payload_mtu)
    
    // set parameters
    o->reactor = reactor;
    o->payload_mtu = payload_mtu;
    o->sp_params = sp_params;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // calculate SPProto payload MTU
    o->spproto_payload_mtu = spproto_payload_mtu_for_carrier_mtu(o->sp_params, socket_mtu);
    
    // calculate effective socket MTU
    o->effective_socket_mtu = spproto_carrier_mtu_for_payload_mtu(o->sp_params, o->spproto_payload_mtu);
    
    // set no OTP warning handler
    if (SPPROTO_HAVE_OTP(o->sp_params)) {
        o->handler_otp_warning = NULL;
    }
    
    // set mode none
    o->mode = DATAGRAMPEERIO_MODE_NONE;
    
    // init persistent I/O objects
    if (!init_persistent_io(o, latency, recv_userif)) {
        goto fail1;
    }
    
    // init debug object
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    return 0;
}

void DatagramPeerIO_Free (DatagramPeerIO *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);

    // reset mode
    reset_mode(o);
    
    // free persistent I/O objects
    free_persistent_io(o);
    
    // free dead var
    DEAD_KILL(o->dead);
}

PacketPassInterface * DatagramPeerIO_GetSendInput (DatagramPeerIO *o)
{
    return FragmentProtoDisassembler_GetInput(&o->send_disassembler);
}

void DatagramPeerIO_Disconnect (DatagramPeerIO *o)
{
    // reset mode
    reset_mode(o);
}

int DatagramPeerIO_Connect (DatagramPeerIO *o, BAddr addr)
{
    ASSERT(BAddr_IsRecognized(&addr) && !BAddr_IsInvalid(&addr))
    
    // reset mode
    reset_mode(o);
    
    // init socket
    if (BSocket_Init(&o->sock, o->reactor, addr.type, BSOCKET_TYPE_DGRAM) < 0) {
        BLog(BLOG_ERROR, "BSocket_Init failed");
        goto fail1;
    }
    
    // connect the socket
    // Windows needs this or receive will fail
    if (BSocket_Connect(&o->sock, &addr) < 0) {
        BLog(BLOG_ERROR, "BSocket_Connect failed");
        goto fail2;
    }
    
    // init sending
    BIPAddr local_addr;
    BIPAddr_InitInvalid(&local_addr);
    init_sending(o, addr, local_addr);
    
    // init receiving
    init_receiving(o);
    
    // set mode
    o->mode = DATAGRAMPEERIO_MODE_CONNECT;
    
    // init mode dead var
    DEAD_INIT(o->mode_dead);
    
    return 1;
    
fail2:
    BSocket_Free(&o->sock);
fail1:
    return 0;
}

int DatagramPeerIO_Bind (DatagramPeerIO *o, BAddr addr)
{
    ASSERT(BAddr_IsRecognized(&addr) && !BAddr_IsInvalid(&addr))
    
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
    
    // init mode dead var
    DEAD_INIT(o->mode_dead);
    
    // set sending not up
    o->bind_sending_up = 0;
    
    return 1;
    
fail2:
    BSocket_Free(&o->sock);
fail1:
    return 0;
}

void DatagramPeerIO_Flush (DatagramPeerIO *o)
{
    BLog(BLOG_ERROR, "Flushing not implemented");
}

void DatagramPeerIO_SetEncryptionKey (DatagramPeerIO *o, uint8_t *encryption_key)
{
    ASSERT(o->sp_params.encryption_mode != SPPROTO_ENCRYPTION_MODE_NONE)
    
    // set sending key
    SPProtoEncoder_SetEncryptionKey(&o->send_encoder, encryption_key);
    
    // set receiving key
    SPProtoDecoder_SetEncryptionKey(&o->recv_decoder, encryption_key);
}

void DatagramPeerIO_RemoveEncryptionKey (DatagramPeerIO *o)
{
    ASSERT(o->sp_params.encryption_mode != SPPROTO_ENCRYPTION_MODE_NONE)
    
    // remove sending key
    SPProtoEncoder_RemoveEncryptionKey(&o->send_encoder);
    
    // remove receiving key
    SPProtoDecoder_RemoveEncryptionKey(&o->recv_decoder);
}

void DatagramPeerIO_SetOTPSendSeed (DatagramPeerIO *o, uint16_t seed_id, uint8_t *key, uint8_t *iv)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    
    // set sending seed
    SPProtoEncoder_SetOTPSeed(&o->send_encoder, seed_id, key, iv);
}

void DatagramPeerIO_RemoveOTPSendSeed (DatagramPeerIO *o)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    
    // remove sending seed
    SPProtoEncoder_RemoveOTPSeed(&o->send_encoder);
}

void DatagramPeerIO_AddOTPRecvSeed (DatagramPeerIO *o, uint16_t seed_id, uint8_t *key, uint8_t *iv)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    
    // add receiving seed
    SPProtoDecoder_AddOTPSeed(&o->recv_decoder, seed_id, key, iv);
}

void DatagramPeerIO_RemoveOTPRecvSeeds (DatagramPeerIO *o)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    
    // remove receiving seeds
    SPProtoDecoder_RemoveOTPSeeds(&o->recv_decoder);
}

void DatagramPeerIO_SetOTPWarningHandler (DatagramPeerIO *o, DatagramPeerIO_handler_otp_warning handler, void *user, int num_used)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    ASSERT(!handler || num_used > 0)
    
    o->handler_otp_warning = handler;
    o->handler_otp_warning_user = user;
    o->handler_otp_warning_num_used = num_used;
}
