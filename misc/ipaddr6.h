/**
 * @file ipaddr6.h
 * @author Ambroz Bizjak <ambrop7@gmail.com>
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
 * IPv6 address parsing functions.
 */

#ifndef BADVPN_MISC_IPADDR6_H
#define BADVPN_MISC_IPADDR6_H

#include <stdint.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <misc/debug.h>

// from /etc/iproute2/rt_scopes
#define IPADDR6_SCOPE_GLOBAL 0
#define IPADDR6_SCOPE_HOST 254
#define IPADDR6_SCOPE_LINK 253
#define IPADDR6_SCOPE_SITE 200

struct ipv6_ifaddr {
    uint8_t addr[16];
    int prefix;
    int scope;
};

#define IPADDR6_PRINT_MAX INET6_ADDRSTRLEN

static int ipaddr6_print_addr (const uint8_t *addr, char *out_buf) WARN_UNUSED;


int ipaddr6_print_addr (const uint8_t *addr, char *out_buf)
{
    struct sockaddr_in6 a;
    memset(&a, 0, sizeof(a));
    a.sin6_family = AF_INET6;
    a.sin6_port = 0;
    a.sin6_flowinfo= 0;
    memcpy(a.sin6_addr.s6_addr, addr, 16);
    a.sin6_scope_id = 0;
    
    if (getnameinfo((struct sockaddr *)&a, sizeof(a), out_buf, IPADDR6_PRINT_MAX, NULL, 0, NI_NUMERICHOST) < 0) {
        return 0;
    }
    
    return 1;
}

#endif
