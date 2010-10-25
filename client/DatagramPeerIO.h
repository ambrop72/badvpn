/**
 * @file DatagramPeerIO.h
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
 * 
 * @section DESCRIPTION
 * 
 * Object for comminicating with a peer using a datagram socket.
 */

#ifndef BADVPN_CLIENT_DATAGRAMPEERIO_H
#define BADVPN_CLIENT_DATAGRAMPEERIO_H

#include <stdint.h>

#include <misc/dead.h>
#include <misc/debug.h>
#include <protocol/spproto.h>
#include <protocol/fragmentproto.h>
#include <system/DebugObject.h>
#include <system/BReactor.h>
#include <system/BAddr.h>
#include <system/BSocket.h>
#include <system/BTime.h>
#include <flow/PacketPassInterface.h>
#include <flow/DatagramSocketSink.h>
#include <flow/PacketPassConnector.h>
#include <flow/SinglePacketBuffer.h>
#include <flow/SPProtoEncoder.h>
#include <flow/FragmentProtoDisassembler.h>
#include <flow/DatagramSocketSource.h>
#include <flow/PacketRecvConnector.h>
#include <flow/SPProtoDecoder.h>
#include <flow/FragmentProtoAssembler.h>
#include <flow/PacketPassNotifier.h>
#include <flow/PacketRecvNotifier.h>

/**
 * Handler function invoked when the number of used OTPs has reached
 * the specified warning number in {@link DatagramPeerIO_SetOTPWarningHandler}.
 * May be called from within a sending Send call.
 *
 * @param user as in {@link DatagramPeerIO_SetOTPWarningHandler}
 */
typedef void (*DatagramPeerIO_handler_otp_warning) (void *user);

/**
 * Object for comminicating with a peer using a datagram socket.
 *
 * The user provides data for sending to the peer through {@link PacketPassInterface}.
 * Received data is provided to the user through {@link PacketPassInterface}.
 *
 * The object has a logical state called a mode, which is one of the following:
 *     - default - nothing is send or received
 *     - connecting - an address was provided by the user for sending datagrams to.
 *                    Datagrams are being sent to that address through a socket,
 *                    and datagrams are being received on the same socket.
 *     - binding - an address was provided by the user to bind a socket to.
 *                 Datagrams are being received on the socket. Datagrams are not being
 *                 sent initially. When a datagram is received, its source address is
 *                 used as a destination address for sending datagrams.
 */
typedef struct {
    DebugObject d_obj;
    dead_t dead;
    BReactor *reactor;
    int payload_mtu;
    struct spproto_security_params sp_params;
    int spproto_payload_mtu;
    int effective_socket_mtu;
    
    // flow error domain
    FlowErrorDomain domain;
    
    // encoder group
    SPProtoEncoderGroup encoder_group;
    
    // persistent I/O objects
    
    // sending base
    FragmentProtoDisassembler send_disassembler;
    SPProtoEncoder send_encoder;
    PacketRecvNotifier send_notifier;
    SinglePacketBuffer send_buffer;
    PacketPassConnector send_connector;
    
    // receiving
    PacketRecvConnector recv_connector;
    SinglePacketBuffer recv_buffer;
    SPProtoDecoder recv_decoder;
    PacketPassNotifier recv_notifier;
    FragmentProtoAssembler recv_assembler;
    
    // OTP warning handler
    DatagramPeerIO_handler_otp_warning handler_otp_warning;
    void *handler_otp_warning_user;
    int handler_otp_warning_num_used;
    
    // mode
    int mode;
    dead_t mode_dead;
    
    // in binded mode, whether sending is up
    int bind_sending_up;
    
    // datagram socket
    BSocket sock;
    
    // non-persistent sending objects
    DatagramSocketSink send_sink;
    
    // non-persistent receiving objects
    DatagramSocketSource recv_source;
} DatagramPeerIO;

/**
 * Initializes the object.
 * The interface is initialized in default mode.
 * {@link BLog_Init} must have been done.
 *
 * @param o the object
 * @param reactor {@link BReactor} we live in
 * @param payload_mtu maximum payload size. Must be >=0.
 * @param socket_mtu maximum datagram size for the socket. Must be >=0. Must be large enough so it is possible to
 *                   send a FragmentProto chunk with one byte of data over SPProto, i.e. the following has to hold:
 *                   spproto_payload_mtu_for_carrier_mtu(sp_params, socket_mtu) > sizeof(struct fragmentproto_chunk_header)
 * @param sp_params SPProto security parameters. Must be valid according to {@link spproto_validate_security_params}.
 * @param latency latency parameter to {@link FragmentProtoDisassembler_Init}.
 * @param recv_userif interface to pass received packets to the user. Its MTU must be >=payload_mtu.
 * @return 1 on success, 0 on failure
 */
int DatagramPeerIO_Init (DatagramPeerIO *o, BReactor *reactor, int payload_mtu, int socket_mtu, struct spproto_security_params sp_params, btime_t latency, PacketPassInterface *recv_userif) WARN_UNUSED;

/**
 * Frees the object.
 *
 * @param o the object
 */
void DatagramPeerIO_Free (DatagramPeerIO *o);

/**
 * Returns an interface the user should use to send packets.
 * The OTP warning handler may be called from within Send calls
 * to the interface.
 *
 * @param o the object
 * @return sending interface
 */
PacketPassInterface * DatagramPeerIO_GetSendInput (DatagramPeerIO *o);

/**
 * Breaks down the connection if one is configured.
 * The interface enters default mode.
 *
 * @param o the object
 */
void DatagramPeerIO_Disconnect (DatagramPeerIO *o);

/**
 * Attempts to establish connection to the peer which has bound to an address.
 * On success, the interface enters connecting mode.
 * On failure, the interface enters default mode.
 *
 * @param o the object
 * @param addr address to send packets to. Must be recognized and not invalid.
 * @return 1 on success, 0 on failure
 */
int DatagramPeerIO_Connect (DatagramPeerIO *o, BAddr addr) WARN_UNUSED;

/**
 * Attempts to establish connection to the peer by binding to an address.
 * On success, the interface enters connecting mode.
 * On failure, the interface enters default mode.
 *
 * @param o the object
 * @param addr address to bind to. Must be recognized and not invalid.
 * @return 1 on success, 0 on failure
 */
int DatagramPeerIO_Bind (DatagramPeerIO *o, BAddr addr) WARN_UNUSED;

/**
 * Removes any internally buffered packets for sending.
 * This can be used when configuring a new connecion to prevent packets encoded with
 * previous parameters from being sent over the new connection.
 *
 * @param o the object
 */
void DatagramPeerIO_Flush (DatagramPeerIO *o);

/**
 * Sets the encryption key to use for sending and receiving.
 * Encryption must be enabled.
 *
 * @param o the object
 * @param encryption_key key to use
 */
void DatagramPeerIO_SetEncryptionKey (DatagramPeerIO *o, uint8_t *encryption_key);

/**
 * Removed the encryption key to use for sending and receiving.
 * Encryption must be enabled.
 *
 * @param o the object
 */
void DatagramPeerIO_RemoveEncryptionKey (DatagramPeerIO *o);

/**
 * Sets the OTP seed for sending.
 * OTPs must be enabled.
 *
 * @param o the object
 * @param seed_id seed identifier
 * @param key OTP encryption key
 * @param iv OTP initialization vector
 */
void DatagramPeerIO_SetOTPSendSeed (DatagramPeerIO *o, uint16_t seed_id, uint8_t *key, uint8_t *iv);

/**
 * Removes the OTP seed for sending of one is configured.
 * OTPs must be enabled.
 *
 * @param o the object
 */
void DatagramPeerIO_RemoveOTPSendSeed (DatagramPeerIO *o);

/**
 * Adds an OTP seed for reciving.
 * OTPs must be enabled.
 *
 * @param o the object
 * @param seed_id seed identifier
 * @param key OTP encryption key
 * @param iv OTP initialization vector
 */
void DatagramPeerIO_AddOTPRecvSeed (DatagramPeerIO *o, uint16_t seed_id, uint8_t *key, uint8_t *iv);

/**
 * Removes all OTP seeds for reciving.
 * OTPs must be enabled.
 *
 * @param o the object
 */
void DatagramPeerIO_RemoveOTPRecvSeeds (DatagramPeerIO *o);

/**
 * Sets the OTP warning handler.
 * OTPs must be enabled.
 *
 * @param o the object
 * @param handler handler function. NULL to disable handler.
 * @param user value passed to handler function
 * @param num_used after how many used OTPs to invoke the handler. Must be >0 unless handler is NULL.
 *                 The handler will be invoked when exactly that many OTPs have been used. If the handler
 *                 is configured when the warning level has already been reached, it will not be called
 *                 until a new send seed is set or the handler is reconfigured.
 */
void DatagramPeerIO_SetOTPWarningHandler (DatagramPeerIO *o, DatagramPeerIO_handler_otp_warning handler, void *user, int num_used);

#endif
