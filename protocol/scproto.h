/**
 * @file scproto.h
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
 * Definitions for SCProto, the protocol that the clients communicate in
 * with the server.
 * 
 * All multi-byte integers in structs are little-endian, unless stated otherwise.
 * 
 * A SCProto packet consists of:
 *   - a header (struct {@link sc_header}) which contains the type of the
 *     packet
 *   - the payload
 * 
 * It goes roughly like that:
 * 
 * When the client connects to the server, it sends a "clienthello" packet
 * to the server. The packet contains the protocol version the client is using.
 * When the server receives the "clienthello" packet, it checks the version.
 * If it doesn't match, it disconnects the client. Otherwise the server sends
 * the client a "serverhello" packet to the client. That packet contains
 * the ID of the client and possibly its IPv4 address as the server sees it
 * (zero if not applicable).
 * 
 * The server than proceeds to synchronize the peers' knowledge of each other.
 * It does that by sending a "newclient" messages to a client to inform it of
 * another peer, and "endclient" messages to inform it that a peer is gone.
 * Each client, upon receiving a "newclient" message, MUST sent a corresponding
 * "acceptpeer" message, before sending any messages to the new peer.
 * The server forwards messages between synchronized peers to allow them to
 * communicate. A peer sends a message to another peer by sending the "outmsg"
 * packet to the server, and the server delivers a message to a peer by sending
 * it the "inmsg" packet.
 * 
 * The message service is reliable; messages from one client to another are
 * expected to arrive unmodified and in the same order. There is, however,
 * no flow control. This means that messages can not be used for bulk transfers
 * between the clients (and they are not). If the server runs out of buffer for
 * messages from one client to another, it will stop forwarding messages, and
 * will reset knowledge of the two clients after some delay. Similarly, if one
 * of the clients runs out of buffer locally, it will send the "resetpeer"
 * packet to make the server reset knowledge.
 * 
 * The messages transport either:
 * 
 * - If the relevant "newclient" packets do not contain the
 *   SCID_NEWCLIENT_FLAG_SSL flag, then plaintext MsgProto messages.
 * 
 * - If the relevant "newclient" packets do contain the SCID_NEWCLIENT_FLAG_SSL
 *   flag, then SSL, broken down into packets, PacketProto inside SSL, and finally
 *   MsgProto inside PacketProto. The master peer (one with higher ID) acts as an
 *   SSL server, and the other acts as an SSL client. The peers must identify with
 *   the same certificate they used when connecting to the server, and each peer
 *   must byte-compare the other's certificate agains the one provided to it by
 *   by the server in the relevent "newclient" message.
 */

#ifndef BADVPN_PROTOCOL_SCPROTO_H
#define BADVPN_PROTOCOL_SCPROTO_H

#include <stdint.h>

#define SC_VERSION 29
#define SC_OLDVERSION_NOSSL 27
#define SC_OLDVERSION_BROKENCERT 26

#define SC_KEEPALIVE_INTERVAL 10000

/**
 * SCProto packet header.
 * Follows up to SC_MAX_PAYLOAD bytes of payload.
 */
struct sc_header {
    /**
     * Message type.
     */
    uint8_t type;
} __attribute__((packed));

#define SC_MAX_PAYLOAD 2000
#define SC_MAX_ENC (sizeof(struct sc_header) + SC_MAX_PAYLOAD)

typedef uint16_t peerid_t;

#define SCID_KEEPALIVE 0
#define SCID_CLIENTHELLO 1
#define SCID_SERVERHELLO 2
#define SCID_NEWCLIENT 3
#define SCID_ENDCLIENT 4
#define SCID_OUTMSG 5
#define SCID_INMSG 6
#define SCID_RESETPEER 7
#define SCID_ACCEPTPEER 8

/**
 * "clienthello" client packet payload.
 * Packet type is SCID_CLIENTHELLO.
 */
struct sc_client_hello {
    /**
     * Protocol version the client is using.
     */
    uint16_t version;
} __attribute__((packed));

/**
 * "serverhello" server packet payload.
 * Packet type is SCID_SERVERHELLO.
 */
struct sc_server_hello {
    /**
     * Flags. Not used yet.
     */
    uint16_t flags;
    
    /**
     * Peer ID of the client.
     */
    peerid_t id;
    
    /**
     * IPv4 address of the client as seen by the server
     * (network byte order). Zero if not applicable.
     */
    uint32_t clientAddr;
} __attribute__((packed));

/**
 * "newclient" server packet payload.
 * Packet type is SCID_NEWCLIENT.
 * If the server is using TLS, follows up to SCID_NEWCLIENT_MAX_CERT_LEN
 * bytes of the new client's certificate (encoded in DER).
 */
struct sc_server_newclient {
    /**
     * ID of the new peer.
     */
    peerid_t id;
    
    /**
     * Flags. Possible flags:
     *   - SCID_NEWCLIENT_FLAG_RELAY_SERVER
     *     You can relay frames to other peers through this peer.
     *   - SCID_NEWCLIENT_FLAG_RELAY_CLIENT
     *     You must allow this peer to relay frames to other peers through you.
     *   - SCID_NEWCLIENT_FLAG_SSL
     *     SSL must be used to talk to this peer through messages.
     */
    uint16_t flags;
} __attribute__((packed));

#define SCID_NEWCLIENT_FLAG_RELAY_SERVER 1
#define SCID_NEWCLIENT_FLAG_RELAY_CLIENT 2
#define SCID_NEWCLIENT_FLAG_SSL 4

#define SCID_NEWCLIENT_MAX_CERT_LEN (SC_MAX_PAYLOAD - sizeof(struct sc_server_newclient))

/**
 * "endclient" server packet payload.
 * Packet type is SCID_ENDCLIENT.
 */
struct sc_server_endclient {
    /**
     * ID of the removed peer.
     */
    peerid_t id;
} __attribute__((packed));

/**
 * "outmsg" client packet header.
 * Packet type is SCID_OUTMSG.
 * Follows up to SC_MAX_MSGLEN bytes of message payload.
 */
struct sc_client_outmsg {
    /**
     * ID of the destionation peer.
     */
    peerid_t clientid;
} __attribute__((packed));

/**
 * "inmsg" server packet payload.
 * Packet type is SCID_INMSG.
 * Follows up to SC_MAX_MSGLEN bytes of message payload.
 */
struct sc_server_inmsg {
    /**
     * ID of the source peer.
     */
    peerid_t clientid;
} __attribute__((packed));

#define _SC_MAX_OUTMSGLEN (SC_MAX_PAYLOAD - sizeof(struct sc_client_outmsg))
#define _SC_MAX_INMSGLEN (SC_MAX_PAYLOAD - sizeof(struct sc_server_inmsg))

#define SC_MAX_MSGLEN (_SC_MAX_OUTMSGLEN < _SC_MAX_INMSGLEN ? _SC_MAX_OUTMSGLEN : _SC_MAX_INMSGLEN)

/**
 * "resetpeer" client packet header.
 * Packet type is SCID_RESETPEER.
 */
struct sc_client_resetpeer {
    /**
     * ID of the peer to reset.
     */
    peerid_t clientid;
} __attribute__((packed));

/**
 * "acceptpeer" client packet payload.
 * Packet type is SCID_ACCEPTPEER.
 */
struct sc_client_acceptpeer {
    /**
     * ID of the peer to accept.
     */
    peerid_t clientid;
} __attribute__((packed));

#endif
