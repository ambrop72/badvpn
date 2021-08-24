/**
 * @file BHttpProxyClient.h
 * @author B. Blechschmidt, based on the SocksClient implementation by Ambroz Bizjak
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
 *
 * @section DESCRIPTION
 *
 * HTTP client. HTTP/1.1 only. Support for basic authorization.
 */

#ifndef BADVPN_HTTP_BHTTPPROXYCLIENT_H
#define BADVPN_HTTP_BHTTPPROXYCLIENT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <misc/debug.h>
#include <misc/debugerror.h>
#include <misc/packed.h>
#include <base/DebugObject.h>
#include <base/BPending.h>
#include <system/BConnection.h>
#include <flow/PacketStreamSender.h>

#define BHTTPPROXYCLIENT_EVENT_ERROR 1
#define BHTTPPROXYCLIENT_EVENT_UP 2
#define BHTTPPROXYCLIENT_EVENT_ERROR_CLOSED 3
#define BHTTPPROXYCLIENT_EVENT_CONNECTED 4

/**
 * Handler for events generated by the HTTP client.
 *
 * The event is one of the following:
 * - BHttpProxyClient_EVENT_ERROR: An error has occured. The object must be freed from the
 *   job closure of the handler and no further I/O must be attempted.
 * - BHttpProxyClient_EVENT_ERROR_CLOSED: The server has closed the connection. This event
 *   can only be reported after BHttpProxyClient_EVENT_UP. The object must be freed from
 *   the job closure of the handler and no further I/O must be attempted.
 * - BHttpProxyClient_EVENT_UP: The CONNECT operation was successful. In
 *   the case of CONNECT, application I/O may now begin.
 * - BHttpProxyClient_EVENT_CONNECTED: The TCP connection to the server has been established
 *   and the HTTP protocol is about to begin.
 *
 * @param user as in {@link BHttpProxyClient_Init}
 * @param event See above.
 */
typedef void (*BHttpProxyClient_handler) (void *user, int event);

typedef struct {
    BAddr dest_addr;
    BAddr bind_addr;
    BHttpProxyClient_handler handler;
    void *user;
    BReactor *reactor;
    int state;
    int crlf_state;
    char *headers;
    char *buffer;
    BConnector connector;
    BConnection con;
    BPending continue_job;
    union {
        struct {
            PacketPassInterface *send_if;
            PacketStreamSender send_sender;
            StreamRecvInterface *recv_if;
            uint8_t *recv_dest;
            int recv_len;
            int recv_total;
        } control;
    };
    DebugError d_err;
    DebugObject d_obj;
} BHttpProxyClient;

/**
 * Initializes the object.
 *
 * This object connects to a HTTP server and performs a CONNECT
 * operation. In any case, the object reports the BHttpProxyClient_EVENT_UP event via the
 * handler when the operation was completed successfully. In the case of CONNECT, the
 * user may then use the send and receive interfaces to exchange data through the
 * connection (@ref BHttpProxyClient_GetSendInterface and @ref BHttpProxyClient_GetRecvInterface).
 *
 * @param o the object
 * @param server_addr HTTP server address
 * @param username Username for basic auth through the Proxy-Authorization header.
 * @param password Password for basic auth.
 * @param dest_addr Address to send as DST.ADDR in the CONNECT or UDP ASSOCIATE request.
 *        It is also possible to specify it later from the BHttpProxyClient_EVENT_CONNECTED
 *        event callback using @ref BHttpProxyClient_SetDestAddr; this is necessary for UDP
 *        if the local TCP connection address must be known to bind the UDP socket.
 * @param udp false to perform a CONNECT, true to perform a UDP ASSOCIATE
 * @param handler handler for up and error events
 * @param user value passed to handler
 * @param reactor reactor we live in
 * @return 1 on success, 0 on failure
 */
int BHttpProxyClient_Init (BHttpProxyClient *o, BAddr server_addr,
    const char *username, const char *password, BAddr dest_addr,
    BHttpProxyClient_handler handler, void *user, BReactor *reactor) WARN_UNUSED;

/**
 * Frees the object.
 *
 * @param o the object
 */
void BHttpProxyClient_Free (BHttpProxyClient *o);

/**
 * Returns the send interface.
 * The object must be in up state.
 *
 * @param o the object
 * @return send interface
 */
StreamPassInterface * BHttpProxyClient_GetSendInterface (BHttpProxyClient *o);

/**
 * Returns the receive interface.
 * The object must be in up state.
 *
 * @param o the object
 * @return receive interface
 */
StreamRecvInterface * BHttpProxyClient_GetRecvInterface (BHttpProxyClient *o);

#endif
