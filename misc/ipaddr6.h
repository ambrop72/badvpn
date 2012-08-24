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

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

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

#define IPADDR6_PRINT_MAX 46

static void ipaddr6_print_addr (const uint8_t *addr, char *out_buf);

void ipaddr6_print_addr (const uint8_t *addr, char *out_buf)
{
    int largest_start = 0;
    int largest_len = 0;
    int current_start = 0;
    int current_len = 0;
    
    for (int i = 0; i < 8; i++) {
        if (addr[2 * i] == 0 && addr[2 * i + 1] == 0) {
            current_len++;
            if (current_len > largest_len) {
                largest_start = current_start;
                largest_len = current_len;
            }
        } else {
            current_start = i + 1;
            current_len = 0;
        }
    }
    
    if (largest_len > 1) {
        for (int i = 0; i < largest_start; i++) {
            uint16_t block = ((uint16_t)addr[2 * i] << 8) | addr[2 * i + 1];
            out_buf += sprintf(out_buf, "%"PRIx16":", block);
        }
        if (largest_start == 0) {
            out_buf += sprintf(out_buf, ":");
        }
        
        for (int i = largest_start + largest_len; i < 8; i++) {
            uint16_t block = ((uint16_t)addr[2 * i] << 8) | addr[2 * i + 1];
            out_buf += sprintf(out_buf, ":%"PRIx16, block);
        }
        if (largest_start + largest_len == 8) {
            out_buf += sprintf(out_buf, ":");
        }
    } else {
        const char *prefix = "";
        for (int i = 0; i < 8; i++) {
            uint16_t block = ((uint16_t)addr[2 * i] << 8) | addr[2 * i + 1];
            out_buf += sprintf(out_buf, "%s%"PRIx16, prefix, block);
            prefix = ":";
        }
    }
}

#endif
