/**
 * @file SPProtoEncoder.h
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
 * Object which encodes packets according to SPProto.
 */

#ifndef BADVPN_FLOW_SPPROTOENCODER_H
#define BADVPN_FLOW_SPPROTOENCODER_H

#include <stdint.h>

#include <misc/debug.h>
#include <protocol/spproto.h>
#include <system/DebugObject.h>
#include <security/BEncryption.h>
#include <security/OTPGenerator.h>
#include <flow/PacketRecvInterface.h>

/**
 * Object which encodes packets according to SPProto.
 *
 * Input is with {@link PacketRecvInterface}.
 * Output is with {@link PacketRecvInterface}.
 */
typedef struct {
    struct spproto_security_params sp_params;
    int hash_size;
    int enc_block_size;
    int enc_key_size;
    OTPGenerator otpgen;
    uint16_t otpgen_seed_id;
    int have_encryption_key;
    BEncryption encryptor;
    
    int input_mtu;
    int output_mtu;
    PacketRecvInterface *input;
    int in_len;
    PacketRecvInterface output;
    int out_have;
    uint8_t *out;
    uint8_t *buf;
    LinkedList2Node group_list_node;
    BPending continue_job;
    DebugObject d_obj;
} SPProtoEncoder;

/**
 * Initializes the object.
 * The object is initialized in blocked state.
 *
 * @param o the object
 * @param sp_params SPProto security parameters. Must be valid according to {@link spproto_validate_security_params}.
 * @param input input interface
 * @param pg pending group
 * @return 1 on success, 0 on failure
 */
int SPProtoEncoder_Init (SPProtoEncoder *o, struct spproto_security_params sp_params, PacketRecvInterface *input, BPendingGroup *pg) WARN_UNUSED;

/**
 * Frees the object.
 *
 * @param o the object
 */
void SPProtoEncoder_Free (SPProtoEncoder *o);

/**
 * Returns the output interface.
 * The MTU of the output interface will depend on the input MTU and security parameters,
 * that is spproto_carrier_mtu_for_payload_mtu(sp_params, input MTU).
 *
 * @param o the object
 * @return output interface
 */
PacketRecvInterface * SPProtoEncoder_GetOutput (SPProtoEncoder *o);

/**
 * Sets an encryption key to use.
 * Encryption must be enabled.
 *
 * @param o the object
 * @param encryption_key key to use
 */
void SPProtoEncoder_SetEncryptionKey (SPProtoEncoder *o, uint8_t *encryption_key);

/**
 * Removes an encryption key if one is configured.
 * Encryption must be enabled.
 *
 * @param o the object
 */
void SPProtoEncoder_RemoveEncryptionKey (SPProtoEncoder *o);

/**
 * Sets an OTP seed to use.
 * OTPs must be enabled.
 *
 * @param o the object
 * @param seed_id seed identifier
 * @param key OTP encryption key
 * @param iv OTP initialization vector
 */
void SPProtoEncoder_SetOTPSeed (SPProtoEncoder *o, uint16_t seed_id, uint8_t *key, uint8_t *iv);

/**
 * Removes the OTP seed if one is configured.
 * OTPs must be enabled.
 *
 * @param o the object
 */
void SPProtoEncoder_RemoveOTPSeed (SPProtoEncoder *o);

/**
 * Returns the number of OTPs used so far, or total number if
 * no seed has been set yet.
 * OTPs must be enabled.
 *
 * @param o the object
 * @return OTP position
 */
int SPProtoEncoder_GetOTPPosition (SPProtoEncoder *o);

#endif
