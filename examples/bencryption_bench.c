/**
 * @file bencryption_bench.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#include <misc/balloc.h>
#include <security/BRandom.h>
#include <security/BEncryption.h>
#include <base/DebugObject.h>

static void usage (char *name)
{
    printf(
        "Usage: %s <enc/dec> <ciper> <num_blocks> <num_ops>\n"
        "    <cipher> is one of (blowfish, aes).\n",
        name
    );
    
    exit(1);
}

int main (int argc, char **argv)
{
    if (argc <= 0) {
        return 1;
    }
    
    if (argc != 5) {
        usage(argv[0]);
    }
    
    char *mode_str = argv[1];
    char *cipher_str = argv[2];
    
    int mode;
    int cipher;
    int num_blocks = atoi(argv[3]);
    int num_ops = atoi(argv[4]);
    
    if (!strcmp(mode_str, "enc")) {
        mode = BENCRYPTION_MODE_ENCRYPT;
    }
    else if (!strcmp(mode_str, "dec")) {
        mode = BENCRYPTION_MODE_DECRYPT;
    }
    else {
        usage(argv[0]);
    }
    
    if (!strcmp(cipher_str, "blowfish")) {
        cipher = BENCRYPTION_CIPHER_BLOWFISH;
    }
    else if (!strcmp(cipher_str, "aes")) {
        cipher = BENCRYPTION_CIPHER_AES;
    }
    else {
        usage(argv[0]);
    }
    
    if (num_blocks < 0 || num_ops < 0) {
        usage(argv[0]);
    }
    
    int key_size = BEncryption_cipher_key_size(cipher);
    int block_size = BEncryption_cipher_block_size(cipher);
    
    uint8_t key[key_size];
    BRandom_randomize(key, sizeof(key));
    
    uint8_t iv[block_size];
    BRandom_randomize(iv, sizeof(iv));
    
    if (num_blocks > INT_MAX / block_size) {
        printf("too much");
        goto fail0;
    }
    int unit_size = num_blocks * block_size;
    
    printf("unit size %d\n", unit_size);
    
    uint8_t *buf1 = BAlloc(unit_size);
    if (!buf1) {
        printf("BAlloc failed");
        goto fail0;
    }
    
    uint8_t *buf2 = BAlloc(unit_size);
    if (!buf2) {
        printf("BAlloc failed");
        goto fail1;
    }
    
    BEncryption enc;
    BEncryption_Init(&enc, mode, cipher, key);
    
    uint8_t *in = buf1;
    uint8_t *out = buf2;
    BRandom_randomize(in, unit_size);
    
    for (int i = 0; i < num_ops; i++) {
        BEncryption_Encrypt(&enc, in, out, unit_size, iv);
        
        uint8_t *t = in;
        in = out;
        out = t;
    }
    
    BEncryption_Free(&enc);
    BFree(buf2);
fail1:
    BFree(buf1);
fail0:
    DebugObjectGlobal_Finish();
    
    return 0;
}
