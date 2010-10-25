/**
 * @file OTPCalculator.h
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
 * Object that calculates OTPs.
 */

#ifndef BADVPN_SECURITY_OTPCALCULATOR_H
#define BADVPN_SECURITY_OTPCALCULATOR_H

#include <stdlib.h>
#include <string.h>

#include <misc/balign.h>
#include <misc/debug.h>
#include <misc/brandom.h>
#include <security/BEncryption.h>
#include <system/DebugObject.h>

/**
 * Type for an OTP.
 */
typedef uint32_t otp_t;

/**
 * Object that calculates OTPs.
 */
typedef struct {
    DebugObject d_obj;
    int num_otps;
    int cipher;
    int block_size;
    int num_blocks;
    otp_t *data;
} OTPCalculator;

/**
 * Initializes the calculator.
 *
 * @param calc the object
 * @param num_otps number of OTPs to generate from a seed. Must be >=0.
 * @param cipher encryption cipher for calculating the OTPs. Must be valid
 *               according to {@link BEncryption_cipher_valid}.
 * @return 1 on success, 0 on failure
 */
static int OTPCalculator_Init (OTPCalculator *calc, int num_otps, int cipher) WARN_UNUSED;

/**
 * Frees the calculator.
 *
 * @param calc the object
 */
static void OTPCalculator_Free (OTPCalculator *calc);

/**
 * Generates OTPs from the given key and IV.
 *
 * @param calc the object
 * @param key encryption key
 * @param iv initialization vector
 * @param shuffle whether to shuffle the OTPs. Must be 1 or 0.
 * @return pointer to an array of 32-bit OPTs. Constains as many OTPs as was specified
 *         in {@link OTPCalculator_Init}. Valid until the next generation or
 *         until the object is freed.
 */
static otp_t * OTPCalculator_Generate (OTPCalculator *calc, uint8_t *key, uint8_t *iv, int shuffle);

int OTPCalculator_Init (OTPCalculator *calc, int num_otps, int cipher)
{
    ASSERT(num_otps >= 0)
    ASSERT(BEncryption_cipher_valid(cipher))
    
    // init arguments
    calc->num_otps = num_otps;
    calc->cipher = cipher;
    
    // remember block size
    calc->block_size = BEncryption_cipher_block_size(calc->cipher);
    
    // calculate number of blocks
    calc->num_blocks = BDIVIDE_UP(calc->num_otps * sizeof(otp_t), calc->block_size);
    
    // allocate buffer
    calc->data = malloc(calc->num_blocks * calc->block_size);
    if (!calc->data) {
        goto fail0;
    }
    
    // init debug object
    DebugObject_Init(&calc->d_obj);
    
    return 1;
    
fail0:
    return 0;
}

void OTPCalculator_Free (OTPCalculator *calc)
{
    // free debug object
    DebugObject_Free(&calc->d_obj);
    
    // free buffer
    free(calc->data);
}

otp_t * OTPCalculator_Generate (OTPCalculator *calc, uint8_t *key, uint8_t *iv, int shuffle)
{
    ASSERT(shuffle == 0 || shuffle == 1)
    
    // copy IV so it can be updated
    uint8_t iv_work[calc->block_size];
    memcpy(iv_work, iv, calc->block_size);
    
    // create zero block
    uint8_t zero[calc->block_size];
    memset(zero, 0, calc->block_size);
    
    // init encryptor
    BEncryption encryptor;
    BEncryption_Init(&encryptor, BENCRYPTION_MODE_ENCRYPT, calc->cipher, key);
    
    // encrypt zero blocks
    for (int i = 0; i < calc->num_blocks; i++) {
        BEncryption_Encrypt(&encryptor, zero, (uint8_t *)calc->data + i * calc->block_size, calc->block_size, iv_work);
    }
    
    // free encryptor
    BEncryption_Free(&encryptor);
    
    // shuffle if requested
    if (shuffle) {
        int i = 0;
        while (i < calc->num_otps) {
            uint16_t ints[256];
            brandom_randomize((uint8_t *)ints, sizeof(ints));
            for (int j = 0; j < 256 && i < calc->num_otps; j++) {
                int newIndex = i + (ints[j] % (calc->num_otps - i));
                otp_t temp = calc->data[i];
                calc->data[i] = calc->data[newIndex];
                calc->data[newIndex] = temp;
                i++;
            }
        }
    }
    
    return calc->data;
}

#endif
