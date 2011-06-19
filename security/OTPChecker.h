/**
 * @file OTPChecker.h
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
 * Object that checks OTPs agains known seeds.
 */

#ifndef BADVPN_SECURITY_OTPCHECKER_H
#define BADVPN_SECURITY_OTPCHECKER_H

#include <stdint.h>

#include <misc/balign.h>
#include <misc/debug.h>
#include <misc/modadd.h>
#include <security/OTPCalculator.h>
#include <base/DebugObject.h>
#include <threadwork/BThreadWork.h>

struct OTPChecker_entry {
    otp_t otp;
    int avail;
};

struct OTPChecker_table {
    uint16_t id;
    struct OTPChecker_entry *entries;
};

/**
 * Handler called when OTP generation for a seed is finished and new OTPs
 * can be recognized.
 * 
 * @param user as in {@link OTPChecker_Init}
 */
typedef void (*OTPChecker_handler) (void *user);

/**
 * Object that checks OTPs agains known seeds.
 */
typedef struct {
    BThreadWorkDispatcher *twd;
    OTPChecker_handler handler;
    void *user;
    int num_otps;
    int cipher;
    int num_entries;
    int num_tables;
    int tables_used;
    int next_table;
    OTPCalculator calc;
    struct OTPChecker_table *tables;
    struct OTPChecker_entry *entries;
    int tw_have;
    BThreadWork tw;
    uint8_t tw_key[BENCRYPTION_MAX_KEY_SIZE];
    uint8_t tw_iv[BENCRYPTION_MAX_BLOCK_SIZE];
    DebugObject d_obj;
} OTPChecker;

/**
 * Initializes the checker.
 * {@link BSecurity_GlobalInitThreadSafe} must have been done if
 * {@link BThreadWorkDispatcher_UsingThreads}(twd) = 1.
 *
 * @param mc the object
 * @param num_otps number of OTPs to generate from a seed. Must be >0.
 * @param cipher encryption cipher for calculating the OTPs. Must be valid
 *               according to {@link BEncryption_cipher_valid}.
 * @param num_tables number of tables to keep, each for one seed. Must be >0.
 * @param twd thread work dispatcher
 * @return 1 on success, 0 on failure
 */
int OTPChecker_Init (OTPChecker *mc, int num_otps, int cipher, int num_tables, BThreadWorkDispatcher *twd) WARN_UNUSED;

/**
 * Frees the checker.
 *
 * @param mc the object
 */
void OTPChecker_Free (OTPChecker *mc);

/**
 * Starts generating OTPs to recognize for a seed.
 * OTPs for this seed will not be recognized until the {@link OTPChecker_handler} handler is called.
 * If OTPs are still being generated for a previous seed, it will be forgotten.
 *
 * @param mc the object
 * @param seed_id seed identifier
 * @param key encryption key
 * @param iv initialization vector
 */
void OTPChecker_AddSeed (OTPChecker *mc, uint16_t seed_id, uint8_t *key, uint8_t *iv);

/**
 * Removes all active seeds.
 *
 * @param mc the object
 */
void OTPChecker_RemoveSeeds (OTPChecker *mc);

/**
 * Checks an OTP.
 *
 * @param mc the object
 * @param seed_id identifer of seed whom the OTP is claimed to belong to
 * @param otp OTP to check
 * @return 1 if the OTP is valid, 0 if not
 */
int OTPChecker_CheckOTP (OTPChecker *mc, uint16_t seed_id, otp_t otp);

/**
 * Sets handlers.
 *
 * @param mc the object
 * @param handler handler to call when generation of new OTPs is complete,
 *                after {@link OTPChecker_AddSeed} was called.
 * @param user argument to handler
 */
void OTPChecker_SetHandlers (OTPChecker *mc, OTPChecker_handler handler, void *user);

#endif
