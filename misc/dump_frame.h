/**
 * @file dump_frame.h
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
 * Function for dumping an Ethernet frame to a file in pcap format, used
 * for debugging (e.g. for analyzing with Wireshark).
 */

#ifndef BADVPN_MISC_DUMP_FRAME_H
#define BADVPN_MISC_DUMP_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

struct pcap_hdr {
    uint32_t magic_number;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t  thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network;
} __attribute__((packed));

struct pcaprec_hdr {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
} __attribute__((packed));

static int write_to_file (uint8_t *data, size_t data_len, FILE *f)
{
    while (data_len > 0) {
        size_t bytes = fwrite(data, 1, data_len, f);
        if (bytes == 0) {
            return 0;
        }
        data += bytes;
        data_len -= bytes;
    }
    
    return 1;
}

static int dump_frame (uint8_t *data, size_t data_len, const char *file)
{
    FILE *f = fopen(file, "w");
    if (!f) {
        goto fail0;
    }
    
    struct pcap_hdr gh;
    gh.magic_number = 0xa1b2c3d4;
    gh.version_major = 2;
    gh.version_minor = 4;
    gh.thiszone = 0;
    gh.sigfigs = 0;
    gh.snaplen = 65535;
    gh.network = 1;
    
    if (!write_to_file((uint8_t *)&gh, sizeof(gh), f)) {
        goto fail1;
    }
    
    struct pcaprec_hdr ph;
    ph.ts_sec = 0;
    ph.ts_usec = 0;
    ph.incl_len = data_len;
    ph.orig_len = data_len;
    
    if (!write_to_file((uint8_t *)&ph, sizeof(ph), f)) {
        goto fail1;
    }
    
    if (!write_to_file(data, data_len, f)) {
        goto fail1;
    }
    
    if (fclose(f) != 0) {
        return 0;
    }
    
    return 1;
    
fail1:
    fclose(f);
fail0:
    return 0;
}

#endif
