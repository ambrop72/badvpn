/**
 * @file BSocket.h
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
 * A wrapper around OS-specific socket functions, integrated into
 * the event system.
 */

#ifndef BADVPN_SYSTEM_BSOCKET_H
#define BADVPN_SYSTEM_BSOCKET_H

#include <misc/dead.h>
#include <misc/debug.h>
#include <system/BAddr.h>
#include <system/BReactor.h>
#include <system/DebugObject.h>

// errors
#define BSOCKET_ERROR_NONE 0
#define BSOCKET_ERROR_UNKNOWN 1
#define BSOCKET_ERROR_LATER 2
#define BSOCKET_ERROR_IN_PROGRESS 3
#define BSOCKET_ERROR_ACCESS_DENIED 4
#define BSOCKET_ERROR_ADDRESS_NOT_AVAILABLE 5
#define BSOCKET_ERROR_ADDRESS_IN_USE 6
#define BSOCKET_ERROR_CONNECTION_REFUSED 7
#define BSOCKET_ERROR_CONNECTION_TIMED_OUT 8
#define BSOCKET_ERROR_CONNECTION_RESET 9
#define BSOCKET_ERROR_NETWORK_UNREACHABLE 10
#define BSOCKET_ERROR_NO_MEMORY 11

// socket types
#define BSOCKET_TYPE_STREAM 1
#define BSOCKET_TYPE_DGRAM 2
#define BSOCKET_TYPE_SEQPACKET 3

// socket events
#define BSOCKET_READ 1
#define BSOCKET_WRITE 2
#define BSOCKET_ACCEPT 4
#define BSOCKET_CONNECT 8

// default backlog if backlog is <0
#define BSOCKET_DEFAULT_BACKLOG 128

// default limit for number of consecutive receive operations
// must be -1 (no limit) or >0
#define BSOCKET_DEFAULT_RECV_MAX 2

struct BSocket_t;

typedef void (*BSocket_handler) (void *user, int event);

/**
 * A wrapper around OS-specific socket functions, integrated into
 * the event system.
 *
 * To simplify implementation, most functions just call the corresponding
 * socket function. Only required and most common errors are translated.
 */
typedef struct BSocket_t {
    DebugObject d_obj;
    dead_t dead;
    BReactor *bsys;
    int type;
    int domain;
    int socket;
    int have_pktinfo;
    int error;
    BSocket_handler global_handler;
    void *global_handler_user;
    BSocket_handler handlers[4];
    void *handlers_user[4];
    uint8_t waitEvents;
    int connecting_status; // 0 not connecting, 1 connecting, 2 finished
    int connecting_result;
    int recv_max;
    int recv_num;
    
    #ifdef BADVPN_USE_WINAPI
    WSAEVENT event;
    BHandle bhandle;
    #else
    BFileDescriptor fd;
    #endif
} BSocket;

/**
 * Initializes global socket data.
 * This must be called once in program before sockets are used.
 *
 * @return 0 for success, -1 for failure
 */
int BSocket_GlobalInit (void) WARN_UNUSED;

/**
 * Initializes a socket.
 *
 * @param bs the object
 * @param bsys {@link BReactor} to operate in
 * @param domain domain (same as address type). Must be one of BADDR_TYPE_IPV4, BADDR_TYPE_IPV6
 *               and BADDR_TYPE_UNIX (non-Windows only).
 * @param type socket type. Must be one of BSOCKET_TYPE_STREAM, BSOCKET_TYPE_DGRAM and
 *             BSOCKET_TYPE_SEQPACKET.
 * @return 0 for success,
 *         -1 for failure
 */
int BSocket_Init (BSocket *bs, BReactor *bsys, int domain, int type) WARN_UNUSED;

/**
 * Frees a socket.
 *
 * @param bs the object
 */
void BSocket_Free (BSocket *bs);

/**
 * Sets the maximum number of consecutive receive operations.
 * This limit prevents starvation that might occur when data is being
 * received on a socket faster than in can be processed.
 * The default limit is BSOCKET_DEFAULT_RECV_MAX.
 *
 * @param bs the object
 * @param max number of consecutive receive operations allowed. Muse be >0,
 *            or -1 for no limit.
 */
void BSocket_SetRecvMax (BSocket *bs, int max);

/**
 * Returns the socket's current error code.
 *
 * @param bs the object
 */
int BSocket_GetError (BSocket *bs);

/**
 * Registers a socket-global event handler.
 * The socket-global event handler must not be registered.
 * No event-specific handlers must be registered.
 * When the handler is invoked, it is passed a bitmask of events
 * that occured, instead of a single event.
 *
 * @param bs the object
 * @param handler event handler
 * @param user value to be passed to event handler
 */
void BSocket_AddGlobalEventHandler (BSocket *bs, BSocket_handler handler, void *user);

/**
 * Unregisters the socket-global event handler.
 * The socket-global event handler must be registered.
 *
 * @param bs the object
 * @param handler event handler
 * @param user value to be passed to event handler
 */
void BSocket_RemoveGlobalEventHandler (BSocket *bs);

/**
 * Sets events for the socket-global event handler.
 * The socket-global event handler must be registered.
 *
 * @param bs the object
 * @param events bitmask containing socket events the user is interested in
 */
void BSocket_SetGlobalEvents (BSocket *bs, int events);

/**
 * Registers an event handler for a socket event.
 * When the handler is registered, the corresponding event will
 * initially be disabled.
 * The event must be valid and must not have a handler.
 * The socket-global event handler must not be registered.
 *
 * @param bs the object
 * @param event event to register handler for
 * @param handler event handler
 * @param user value to be passed to event handler
 */
void BSocket_AddEventHandler (BSocket *bs, uint8_t event, BSocket_handler handler, void *user);

/**
 * Unregisters an event handler for a socket event.
 * The event must be valid and must have a handler.
 *
 * @param bs the object
 * @param event event to unregister handler for
 */
void BSocket_RemoveEventHandler (BSocket *bs, uint8_t event);

/**
 * Enables a socket event.
 * The event must be valid, must not be enabled, and must have a handler.
 *
 * @param bs the object
 * @param event event to enable
 */
void BSocket_EnableEvent (BSocket *bs, uint8_t event);

/**
 * Disables a socket event.
 * The event must be valid, must be enabled, and must have a handler.
 *
 * @param bs the object
 * @param event event to enable
 */
void BSocket_DisableEvent (BSocket *bs, uint8_t event);

/**
 * Connects the socket to the specifed address, or starts a connection attempt.
 *
 * There must be no pending connection attempt.
 * 
 * For stream sockets, the user will have to wait for the connection result. See the
 * BSOCKET_ERROR_IN_PROGRESS error for details.
 *
 * Datagram sockets can be connected at any time, since connecting such a socket only means
 * specifying an addres where datagrams will be sent and received from.
 * An associated address can be removed by specifying a BADDR_TYPE_NONE address.
 *
 * @param bs the object
 * @param addr remote address. Must not be an invalid address.
 * @return 0 for immediate success,
 *         -1 for failure, where the error code can be:
 *             - BSOCKET_ERROR_IN_PROGRESS the socket is a stream socket and the connection attempt has started.
 *                                         The user should wait for the BSOCKET_CONNECT event and obtain the
 *                                         result of attempt with {@link BSocket_GetConnectResult}.
 *             - BSOCKET_ERROR_UNKNOWN unhandled error
 */
int BSocket_Connect (BSocket *bs, BAddr *addr) WARN_UNUSED;

/**
 * Retreives the result of a connection attempt.
 * The socket must have completed a connection attempt whose result has not yet been retrieved.
 *
 * @param bs the object
 * @return connection attempt result. Possible values:
 *             - 0 connection successful
 *             - BSOCKET_ERROR_CONNECTION_TIMED_OUT timeout while attempting connection
 *             - BSOCKET_ERROR_CONNECTION_REFUSED no one is listening on the remote address
 *             - BSOCKET_ERROR_UNKNOWN unhandled error
 */
int BSocket_GetConnectResult (BSocket *bs);

/**
 * Binds the socket to the specified address.
 *
 * @param bs the object
 * @param addr local address. Must not be an invalid address.
 * @return 0 for success,
 *         -1 for failure, where the error code can be:
 *             - BSOCKET_ERROR_ADDRESS_NOT_AVAILABLE the address is not a local address
 *             - BSOCKET_ERROR_ADDRESS_IN_USE the address is already in use
 *             - BSOCKET_ERROR_ACCESS_DENIED the address is protected
 *             - BSOCKET_ERROR_UNKNOWN unhandled error
 */
int BSocket_Bind (BSocket *bs, BAddr *addr) WARN_UNUSED;

/**
 * Marks the socket as a listening socket.
 *
 * @param bs the object
 * @param backlog whatever this means in the system's listen() function. If it's
 *                negative, BSOCKET_DEFAULT_BACKLOG will be used.
 * @return 0 for success,
 *         -1 for failure, where the error code can be:
 *             - BSOCKET_ERROR_ADDRESS_IN_USE the address is already in use
 *             - BSOCKET_ERROR_UNKNOWN unhandled error
 */
int BSocket_Listen (BSocket *bs, int backlog) WARN_UNUSED;

/**
 * Accepts a connection on a listening socket.
 *
 * @param bs the object
 * @param newsock on success, the new socket will be stored here. If it is NULL and a connection
 *                was accepted, it is closed immediately (but the function succeeds).
 * @param addr if not NULL, the client address will be stored here on success.
 *             The returned address may be an invalid address.
 * @return 0 for success,
 *         -1 for failure, where the error code can be:
 *             - BSOCKET_ERROR_LATER a connection cannot be accepted at the moment
 *             - BSOCKET_ERROR_UNKNOWN unhandled error
 */
int BSocket_Accept (BSocket *bs, BSocket *newsock, BAddr *addr) WARN_UNUSED;

/**
 * Sends data on a socket.
 *
 * @param bs the object
 * @param data buffer to read data from
 * @param len amount of data. Must be >=0.
 * @return non-negative value for amount of data sent,
 *         -1 for failure, where the error code can be:
 *             - BSOCKET_ERROR_LATER no data can be sent at the moment
 *             - BSOCKET_ERROR_CONNECTION_REFUSED the remote host refused to allow the network connection.
 *                   For UDP sockets, this means the remote sent an ICMP Port Unreachable packet.
 *             - BSOCKET_ERROR_CONNECTION_RESET connection was reset by the remote peer
 *             - BSOCKET_ERROR_UNKNOWN unhandled error
 */
int BSocket_Send (BSocket *bs, uint8_t *data, int len) WARN_UNUSED;

/**
 * Receives data on a socket.
 *
 * @param bs the object
 * @param data buffer to write data to
 * @param len maximum amount of data to read. Must be >=0.
 * @return - non-negative value for amount of data read; on stream sockets the value 0
 *           means that the peer has shutdown the connection gracefully
 *         - -1 for failure, where the error code can be:
 *             - BSOCKET_ERROR_LATER no data can be read at the moment
 *             - BSOCKET_ERROR_CONNECTION_REFUSED the remote host refused to allow the network connection.
 *                   For UDP sockets, this means the remote sent an ICMP Port Unreachable packet.
 *             - BSOCKET_ERROR_CONNECTION_RESET connection was reset by the remote peer
 *             - BSOCKET_ERROR_UNKNOWN unhandled error
 */
int BSocket_Recv (BSocket *bs, uint8_t *data, int len) WARN_UNUSED;

/**
 * Sends a datagram on a datagram socket to the specified address.
 *
 * @param bs the object
 * @param data buffer to read data from
 * @param len amount of data. Must be >=0.
 * @param addr remote address. Must be valid.
 * @return non-negative value for amount of data sent,
 *         -1 for failure, where the error code can be:
 *             - BSOCKET_ERROR_LATER no data can be sent at the moment
 *             - BSOCKET_ERROR_CONNECTION_REFUSED the remote host refused to allow the network connection.
 *                   For UDP sockets, this means the remote sent an ICMP Port Unreachable packet.
 *             - BSOCKET_ERROR_CONNECTION_RESET connection was reset by the remote peer
 *             - BSOCKET_ERROR_UNKNOWN unhandled error
 */
int BSocket_SendTo (BSocket *bs, uint8_t *data, int len, BAddr *addr) WARN_UNUSED;

/**
 * Receives a datagram on a datagram socket and returns the sender address.
 *
 * @param bs the object
 * @param data buffer to write data to
 * @param len maximum amount of data to read. Must be >=0.
 * @param addr the sender address will be stored here on success. Must not be NULL.
 *             The returned address may be an invalid address.
 * @return - non-negative value for amount of data read; on stream sockets the value 0
 *           means that the peer has shutdown the connection gracefully
 *         - -1 for failure, where the error code can be:
 *             - BSOCKET_ERROR_LATER no data can be read at the moment
 *             - BSOCKET_ERROR_CONNECTION_REFUSED a remote host refused to allow the network connection.
 *                   For UDP sockets, this means the remote sent an ICMP Port Unreachable packet.
 *             - BSOCKET_ERROR_CONNECTION_RESET connection was reset by the remote peer
 *             - BSOCKET_ERROR_UNKNOWN unhandled error
 */
int BSocket_RecvFrom (BSocket *bs, uint8_t *data, int len, BAddr *addr) WARN_UNUSED;

/**
 * Sends a datagram on a datagram socket to the specified address
 * from the specified local source address.
 *
 * @param bs the object
 * @param data buffer to read data from
 * @param len amount of data. Must be >=0.
 * @param addr remote address. Must be valid.
 * @param local_addr source address. Must not be NULL, but may be invalid.
 * @return non-negative value for amount of data sent,
 *         -1 for failure, where the error code can be:
 *             - BSOCKET_ERROR_LATER no data can be sent at the moment
 *             - BSOCKET_ERROR_CONNECTION_REFUSED the remote host refused to allow the network connection.
 *                   For UDP sockets, this means the remote sent an ICMP Port Unreachable packet.
 *             - BSOCKET_ERROR_CONNECTION_RESET connection was reset by the remote peer
 *             - BSOCKET_ERROR_UNKNOWN unhandled error
 */
int BSocket_SendToFrom (BSocket *bs, uint8_t *data, int len, BAddr *addr, BIPAddr *local_addr) WARN_UNUSED;

/**
 * Receives a datagram on a datagram socket and returns the sender address
 * and the local destination address.
 *
 * @param bs the object
 * @param data buffer to write data to
 * @param len maximum amount of data to read. Must be >=0.
 * @param addr the sender address will be stored here on success. Must not be NULL.
 *             The returned address may be an invalid address.
 * @param local_addr the destination address will be stored here on success. Must not be NULL.
 *                   Returned address will be invalid if it could not be determined.
 * @return - non-negative value for amount of data read; on stream sockets the value 0
 *           means that the peer has shutdown the connection gracefully
 *         - -1 for failure, where the error code can be:
 *             - BSOCKET_ERROR_LATER no data can be read at the moment
 *             - BSOCKET_ERROR_CONNECTION_REFUSED a remote host refused to allow the network connection.
 *                   For UDP sockets, this means the remote sent an ICMP Port Unreachable packet.
 *             - BSOCKET_ERROR_CONNECTION_RESET connection was reset by the remote peer
 *             - BSOCKET_ERROR_UNKNOWN unhandled error
 */
int BSocket_RecvFromTo (BSocket *bs, uint8_t *data, int len, BAddr *addr, BIPAddr *local_addr) WARN_UNUSED;

/**
 * Returns the address of the remote peer.
 *
 * @param bs the object
 * @param addr where to store address. Must not be NULL.
 *             The returned address may be an invalid address.
 * @return 0 for success, -1 for failure
 */
int BSocket_GetPeerName (BSocket *bs, BAddr *addr) WARN_UNUSED;

#ifndef BADVPN_USE_WINAPI

/**
 * Binds the unix socket to the specified path.
 *
 * @param bs the object
 * @param path path to bind to
 * @return 0 for success, -1 for failure
 */
int BSocket_BindUnix (BSocket *bs, const char *path) WARN_UNUSED;

/**
 * Connects the unix socket to the specified path.
 *
 * @param bs the object
 * @param path path to connect to
 * @return 0 for success, -1 for failure
 */
int BSocket_ConnectUnix (BSocket *bs, const char *path) WARN_UNUSED;

#endif

#endif
