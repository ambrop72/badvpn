/**
 * @file udpgw.h
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

// name of the program
#define PROGRAM_NAME "udpgw"

// maxiumum listen addresses
#define MAX_LISTEN_ADDRS 16

// maximum datagram size
#define DEFAULT_UDP_MTU 65520

// connection buffer size for sending to client, in packets
#define CONNECTION_CLIENT_BUFFER_SIZE 1

// connection buffer size for sending to UDP, in packets
#define CONNECTION_UDP_BUFFER_SIZE 1

// maximum number of clients
#define DEFAULT_MAX_CLIENTS 4

// maximum connections for client
#define DEFAULT_MAX_CONNECTIONS_FOR_CLIENT 512

// how long after nothing has been received to disconnect a client
#define CLIENT_DISCONNECT_TIMEOUT 20000

// SO_SNDBFUF socket option for clients
#define CLIENT_SOCKET_SEND_BUFFER 4096
