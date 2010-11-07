/**
 * @file ServerConnection.h
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
 * Object used to communicate with a VPN chat server.
 */

#ifndef BADVPN_SERVERCONNECTION_SERVERCONNECTION_H
#define BADVPN_SERVERCONNECTION_SERVERCONNECTION_H

#include <stdint.h>

#include <prinit.h>
#include <prio.h>
#include <prerror.h>
#include <prtypes.h>
#include <nss.h>
#include <ssl.h>
#include <pk11func.h>
#include <cert.h>
#include <keyhi.h>

#include <misc/dead.h>
#include <misc/debug.h>
#include <protocol/scproto.h>
#include <protocol/msgproto.h>
#include <system/DebugObject.h>
#include <system/BSocket.h>
#include <flow/error.h>
#include <flow/SCKeepaliveSource.h>
#include <flow/PacketProtoEncoder.h>
#include <flow/KeepaliveIO.h>
#include <flow/PacketStreamSender.h>
#include <flow/StreamSocketSink.h>
#include <flow/StreamSocketSource.h>
#include <flow/PacketProtoDecoder.h>
#include <flow/PacketPassPriorityQueue.h>
#include <flow/PacketProtoFlow.h>
#include <nspr_support/BPRFileDesc.h>
#include <nspr_support/PRStreamSink.h>
#include <nspr_support/PRStreamSource.h>

/**
 * Handler function invoked when an error occurs.
 * The object must be freed from withing this function.
 *
 * @param user value passed to {@link ServerConnection_Init}
 */
typedef void (*ServerConnection_handler_error) (void *user);

/**
 * Handler function invoked when the server becomes ready, i.e.
 * the hello packet has been received.
 * The object was in not ready state before.
 * The object enters ready state before the handler is invoked.
 *
 * @param user value passed to {@link ServerConnection_Init}
 * @param my_id our ID as reported by the server
 * @param ext_ip the clientAddr field in the server's hello packet
 */
typedef void (*ServerConnection_handler_ready) (void *user, peerid_t my_id, uint32_t ext_ip);

/**
 * Handler function invoked when a newclient packet is received.
 * The object was in ready state.
 *
 * @param user value passed to {@link ServerConnection_Init}
 * @param peer_id ID of the peer
 * @param flags flags field from the newclient message
 * @param cert peer's certificate (if any)
 * @param cert_len certificate length. Will be >=0.
 */
typedef void (*ServerConnection_handler_newclient) (void *user, peerid_t peer_id, int flags, const uint8_t *cert, int cert_len);

/**
 * Handler function invoked when an enclient packet is received.
 * The object was in ready state.
 *
 * @param user value passed to {@link ServerConnection_Init}
 * @param peer_id ID of the peer
 */
typedef void (*ServerConnection_handler_endclient) (void *user, peerid_t peer_id);

/**
 * Handler function invoked when an inmsg packet is received.
 * The object was in ready state.
 *
 * @param user value passed to {@link ServerConnection_Init}
 * @param peer_id ID of the peer from which the message came
 * @param data message payload
 * @param data_len message length. Will be >=0.
 */
typedef void (*ServerConnection_handler_message) (void *user, peerid_t peer_id, uint8_t *data, int data_len);

/**
 * Object used to communicate with a VPN chat server.
 */
typedef struct {
    // dead var
    dead_t dead;
    
    // reactor
    BReactor *reactor;
    
    // keepalive interval
    int keepalive_interval;
    
    // send buffer size
    int buffer_size;
    
    // whether we use SSL
    int have_ssl;
    
    // client certificate if using SSL
    CERTCertificate *client_cert;

    // client private key if using SSL
    SECKEYPrivateKey *client_key;
    
    // server name if using SSL
    char server_name[256];
    
    // handlers
    void *user;
    ServerConnection_handler_error handler_error;
    ServerConnection_handler_ready handler_ready;
    ServerConnection_handler_newclient handler_newclient;
    ServerConnection_handler_endclient handler_endclient;
    ServerConnection_handler_message handler_message;
    
    // socket
    BSocket sock;
    
    // state
    int state;
    
    // whether an error is being reported
    int error;
    
    // defined when state > SERVERCONNECTION_STATE_CONNECTING
    
    // SSL file descriptor, defined only if using SSL
    PRFileDesc bottom_prfd;
    PRFileDesc *ssl_prfd;
    BPRFileDesc ssl_bprfd;
    
    // I/O error domain
    FlowErrorDomain ioerrdomain;
    
    // input
    union {
        StreamSocketSource plain;
        PRStreamSource ssl;
    } input_source;
    PacketProtoDecoder input_decoder;
    PacketPassInterface input_interface;
    
    // keepalive output branch
    SCKeepaliveSource output_ka_zero;
    PacketProtoEncoder output_ka_encoder;
    
    // output common
    PacketPassPriorityQueue output_queue;
    KeepaliveIO output_keepaliveio;
    PacketStreamSender output_sender;
    union {
        StreamSocketSink plain;
        PRStreamSink ssl;
    } output_sink;
    
    // output local flow
    int output_local_packet_len;
    uint8_t *output_local_packet;
    BufferWriter *output_local_if;
    PacketProtoFlow output_local_oflow;
    PacketPassPriorityQueueFlow output_local_qflow;
    
    // output user flow
    PacketPassPriorityQueueFlow output_user_qflow;
    
    // job to start client I/O
    BPending start_job;
    
    DebugObject d_obj;
} ServerConnection;

/**
 * Initializes the object.
 * The object is initialized in not ready state.
 * {@link BLog_Init} must have been done.
 * {@link BSocket_GlobalInit} must have been done.
 * {@link BSocketPRFileDesc_GlobalInit} must have been done if using SSL.
 *
 * @param o the object
 * @param reactor {@link BReactor} we live in
 * @param addr address to connect to
 * @param keepalive_interval keep-alive sending interval. Must be >0.
 * @param buffer_size minimum size of send buffer in number of packets. Must be >0.
 * @param have_ssl whether to use SSL for connecting to the server. Must be 1 or 0.
 * @param client_cert if using SSL, client certificate to use. Must remain valid as
 *                    long as this object is alive.
 * @param client_key if using SSL, prvate ket to use. Must remain valid as
 *                   long as this object is alive.
 * @param server_name if using SSL, the name of the server. The string is copied.
 * @param user value passed to callback functions
 * @param handler_error error handler. The object must be freed from within the error
 *                      handler before doing anything else with this object.
 * @param handler_ready handler when the server becomes ready, i.e. the hello message has
 *                      been received.
 * @param handler_newclient handler when a newclient message has been received
 * @param handler_endclient handler when an endclient message has been received
 * @param handler_message handler when a peer message has been reveived
 * @return 1 on success, 0 on failure
 */
int ServerConnection_Init (
    ServerConnection *o,
    BReactor *reactor,
    BAddr addr,
    int keepalive_interval,
    int buffer_size,
    int have_ssl,
    CERTCertificate *client_cert,
    SECKEYPrivateKey *client_key,
    const char *server_name,
    void *user,
    ServerConnection_handler_error handler_error,
    ServerConnection_handler_ready handler_ready,
    ServerConnection_handler_newclient handler_newclient,
    ServerConnection_handler_endclient handler_endclient,
    ServerConnection_handler_message handler_message
) WARN_UNUSED;

/**
 * Frees the object.
 *
 * @param o the object
 */
void ServerConnection_Free (ServerConnection *o);

/**
 * Provides a buffer for writing a message to be sent to a peer.
 * The object must be in ready and not writing state.
 * On success, the object enters writing state.
 * Must not be called from the error handler.
 * May invoke the error handler.
 *
 * @param o the object
 * @param data the buffer will be returned here on success. Must not be NULL unless len is 0.
 * @param peer_id ID of peer the message goes to
 * @param len length of the message. Must be >=0 and <=SC_MAX_MSGLEN.
 * @return 1 on success, 0 on out of buffer
 */
int ServerConnection_StartMessage (ServerConnection *o, void **data, peerid_t peer_id, int len) WARN_UNUSED;

/**
 * Submits a written message for sending to a peer.
 * The object must be in ready and writing state.
 * The object enters not writing state.
 * Must not be called from the error handler.
 * May invoke the error handler.
 *
 * @param o the object
 */
void ServerConnection_EndMessage (ServerConnection *o);

/**
 * Returns an interface for sending data to the server (just one).
 * This goes directly into the link (i.e. TCP, possibly via SSL), so packets
 * need to be manually encoded according to PacketProto.
 * The interface must not be used after an error was reported.
 * The object must be in ready and writing state.
 * Must not be called from the error handler.
 *
 * @param o the object
 * @return the interface
 */
PacketPassInterface * ServerConnection_GetSendInterface (ServerConnection *o);

#endif
