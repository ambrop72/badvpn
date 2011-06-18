/**
 * @file StreamPeerIO.h
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
 * Object used for communicating with a peer over TCP.
 */

#ifndef BADVPN_CLIENT_STREAMPEERIO_H
#define BADVPN_CLIENT_STREAMPEERIO_H

#include <stdint.h>

#include <cert.h>
#include <keyhi.h>

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <system/BReactor.h>
#include <system/BConnection.h>
#include <structure/LinkedList2.h>
#include <flow/PacketProtoDecoder.h>
#include <flow/PacketStreamSender.h>
#include <flow/SinglePacketBuffer.h>
#include <flow/PacketProtoEncoder.h>
#include <flow/PacketCopier.h>
#include <flow/PacketPassConnector.h>
#include <flow/StreamRecvConnector.h>
#include <flow/SingleStreamSender.h>
#include <client/PasswordListener.h>

#define STREAMPEERIO_SOCKET_SEND_BUFFER 4096

/**
 * Callback function invoked when an error occurs with the peer connection.
 * The object has entered default state.
 * May be called from within a sending Send call.
 *
 * @param user value given to {@link StreamPeerIO_Init}.
 */
typedef void (*StreamPeerIO_handler_error) (void *user);

/**
 * Object used for communicating with a peer over TCP.
 * The object has a logical state which can be one of the following:
 *     - default state
 *     - listening state
 *     - connecting state
 */
typedef struct {
    // common arguments
    BReactor *reactor;
    int ssl;
    uint8_t *ssl_peer_cert;
    int ssl_peer_cert_len;
    int payload_mtu;
    StreamPeerIO_handler_error handler_error;
    void *user;
    
    // persistent I/O modules
    
    // base sending objects
    PacketCopier output_user_copier;
    PacketProtoEncoder output_user_ppe;
    SinglePacketBuffer output_user_spb;
    PacketPassConnector output_connector;
    
    // receiving objects
    StreamRecvConnector input_connector;
    PacketProtoDecoder input_decoder;
    
    // connection side
    int mode;
    
    union {
        // listening data
        struct {
            int state;
            PasswordListener *listener;
            PasswordListener_pwentry pwentry;
            sslsocket *sock;
        } listen;
        // connecting data
        struct {
            int state;
            CERTCertificate *ssl_cert;
            SECKEYPrivateKey *ssl_key;
            BConnector connector;
            sslsocket sock;
            BSSLConnection sslcon;
            uint64_t password;
            SingleStreamSender pwsender;
        } connect;
    };
    
    // socket data
    sslsocket *sock;
    BSSLConnection sslcon;
    
    // sending objects
    PacketStreamSender output_pss;
    
    DebugObject d_obj;
} StreamPeerIO;

/**
 * Initializes the object.
 * The object is initialized in default state.
 * {@link BLog_Init} must have been done.
 * {@link BNetwork_GlobalInit} must have been done.
 * {@link BSSLConnection_GlobalInit} must have been done if using SSL.
 *
 * @param pio the object
 * @param reactor reactor we live in
 * @param ssl if nonzero, SSL will be used for peer connection
 * @param ssl_peer_cert if using SSL, the certificate we expect the peer to have
 * @param ssl_peer_cert_len if using SSL, the length of the certificate
 * @param payload_mtu maximum packet size as seen from the user. Must be >=0.
 * @param user_recv_if interface to use for submitting received packets. Its MTU
 *                     must be >=payload_mtu.
 * @param handler_error handler function invoked when a connection error occurs
 * @param user value to pass to handler functions
 * @return 1 on success, 0 on failure
 */
int StreamPeerIO_Init (
    StreamPeerIO *pio,
    BReactor *reactor,
    int ssl,
    uint8_t *ssl_peer_cert,
    int ssl_peer_cert_len,
    int payload_mtu,
    PacketPassInterface *user_recv_if,
    StreamPeerIO_handler_error handler_error,
    void *user
) WARN_UNUSED;

/**
 * Frees the object.
 *
 * @param pio the object
 */
void StreamPeerIO_Free (StreamPeerIO *pio);

/**
 * Returns the interface for sending packets to the peer.
 * The OTP warning handler may be called from within Send calls
 * to the interface.
 *
 * @param pio the object
 * @return interface for sending packets to the peer
 */
PacketPassInterface * StreamPeerIO_GetSendInput (StreamPeerIO *pio);

/**
 * Starts an attempt to connect to the peer.
 * On success, the object enters connecting state.
 * On failure, the object enters default state.
 *
 * @param pio the object
 * @param addr address to connect to. Must be supported according to {@link BConnection_AddressSupported}.
 * @param password identification code to send to the peer
 * @param ssl_cert if using SSL, the client certificate to use. This object does not
 *                 take ownership of the certificate; it must remain valid until
 *                 the object is reset.
 * @param ssl_key if using SSL, the private key to use. This object does not take
 *                ownership of the key; it must remain valid until the object is reset.
 * @return 1 on success, 0 on failure
 */
int StreamPeerIO_Connect (StreamPeerIO *pio, BAddr addr, uint64_t password, CERTCertificate *ssl_cert, SECKEYPrivateKey *ssl_key) WARN_UNUSED;

/**
 * Starts an attempt to accept a connection from the peer.
 * The object enters listening state.
 *
 * @param pio the object
 * @param listener {@link PasswordListener} object to use for accepting a connection.
 *                 The listener must have SSL enabled if and only if this object has
 *                 SSL enabled. The listener must be available until the object is
 *                 reset or {@link StreamPeerIO_handler_up} is called.
 * @param password will return the identification code the peer should send when connecting
 */
void StreamPeerIO_Listen (StreamPeerIO *pio, PasswordListener *listener, uint64_t *password);

#endif
