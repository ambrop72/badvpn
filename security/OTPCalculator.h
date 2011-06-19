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
#include <security/BRandom.h>
#include <security/BEncryption.h>
#include <base/DebugObject.h>

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
    size_t num_blocks;
    otp_t *data;
} OTPCalculator;

/**
 * Initializes the calculator.
 * {@link BSecurity_GlobalInitThreadSafe} must have been done if this object
 * will be used from a non-main thread.
 *
 * @param calc the object
 * @param num_otps number of OTPs to generate from a seed. Must be >=0.
 * @param cipher encryption cipher for calculating the OTPs. Must be valid
 *               according to {@link BEncryption_cipher_valid}.
 * @return 1 on success, 0 on failure
 */
int OTPCalculator_Init (OTPCalculator *calc, int num_otps, int cipher) WARN_UNUSED;

/**
 * Frees the calculator.
 *
 * @param calc the object
 */
void OTPCalculator_Free (OTPCalculator *calc);

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
otp_t * OTPCalculator_Generate (OTPCalculator *calc, uint8_t *key, uint8_t *iv, int shuffle);

#endif
