/**
 * @file SPProtoDecoder.h
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
 * Object which decodes packets according to SPProto.
 */

#ifndef BADVPN_FLOW_SPPROTODECODER_H
#define BADVPN_FLOW_SPPROTODECODER_H

#include <stdint.h>

#include <misc/debug.h>
#include <system/DebugObject.h>
#include <protocol/spproto.h>
#include <security/BEncryption.h>
#include <security/OTPChecker.h>
#include <flow/PacketPassInterface.h>

/**
 * Object which decodes packets according to SPProto.
 * Input is with {@link PacketPassInterface}.
 * Output is with {@link PacketPassInterface}.
 */
typedef struct {
    PacketPassInterface *output;
    int output_mtu;
    struct spproto_security_params sp_params;
    int hash_size;
    int enc_block_size;
    int enc_key_size;
    int input_mtu;
    uint8_t *buf;
    PacketPassInterface input;
    OTPChecker otpchecker;
    int have_encryption_key;
    BEncryption encryptor;
    DebugObject d_obj;
} SPProtoDecoder;

/**
 * Initializes the object.
 *
 * @param o the object
 * @param output output interface. Its MTU must not be too large, i.e. this must hold:
 *               spproto_carrier_mtu_for_payload_mtu(sp_params, output MTU) >= 0
 * @param sp_params SPProto parameters
 * @param encryption_key if using encryption, the encryption key
 * @param num_otp_seeds if using OTPs, how many OTP seeds to keep for checking
 *                      receiving packets. Must be >=2 if using OTPs.
 * @param pg pending group
 * @return 1 on success, 0 on failure
 */
int SPProtoDecoder_Init (SPProtoDecoder *o, PacketPassInterface *output, struct spproto_security_params sp_params, int num_otp_seeds, BPendingGroup *pg) WARN_UNUSED;

/**
 * Frees the object.
 *
 * @param o the object
 */
void SPProtoDecoder_Free (SPProtoDecoder *o);

/**
 * Returns the input interface.
 * The MTU of the input interface will depend on the output MTU and security parameters,
 * that is spproto_carrier_mtu_for_payload_mtu(sp_params, output MTU).
 *
 * @param o the object
 * @return input interface
 */
PacketPassInterface * SPProtoDecoder_GetInput (SPProtoDecoder *o);

/**
 * Sets an encryption key for decrypting packets.
 * Encryption must be enabled.
 *
 * @param o the object
 * @param encryption_key key to use
 */
void SPProtoDecoder_SetEncryptionKey (SPProtoDecoder *o, uint8_t *encryption_key);

/**
 * Removes an encryption key if one is configured.
 * Encryption must be enabled.
 *
 * @param o the object
 */
void SPProtoDecoder_RemoveEncryptionKey (SPProtoDecoder *o);

/**
 * Adds a new OTP seed to check received packets against.
 * OTPs must be enabled.
 *
 * @param o the object
 * @param seed_id seed identifier
 * @param key OTP encryption key
 * @param iv OTP initialization vector
 */
void SPProtoDecoder_AddOTPSeed (SPProtoDecoder *o, uint16_t seed_id, uint8_t *key, uint8_t *iv);

/**
 * Removes all OTP seeds for checking received packets against.
 * OTPs must be enabled.
 *
 * @param o the object
 */
void SPProtoDecoder_RemoveOTPSeeds (SPProtoDecoder *o);

#endif
