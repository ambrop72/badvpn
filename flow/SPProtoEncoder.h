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
 * Event context handler called when the remaining number of
 * OTPs equals the warning number after having encoded a packet.
 * 
 * @param user as in {@link SPProtoEncoder_Init}
 */
typedef void (*SPProtoEncoder_handler) (void *user);

/**
 * Object which encodes packets according to SPProto.
 *
 * Input is with {@link PacketRecvInterface}.
 * Output is with {@link PacketRecvInterface}.
 */
typedef struct {
    PacketRecvInterface *input;
    struct spproto_security_params sp_params;
    int otp_warning_count;
    SPProtoEncoder_handler handler;
    void *user;
    int hash_size;
    int enc_block_size;
    int enc_key_size;
    OTPGenerator otpgen;
    uint16_t otpgen_seed_id;
    int have_encryption_key;
    BEncryption encryptor;
    int input_mtu;
    int output_mtu;
    int in_len;
    PacketRecvInterface output;
    int out_have;
    uint8_t *out;
    uint8_t *buf;
    BPending handler_job;
    DebugObject d_obj;
} SPProtoEncoder;

/**
 * Initializes the object.
 * The object is initialized in blocked state.
 *
 * @param o the object
 * @param input input interface. Its MTU must not be too large, i.e. this must hold:
 *              spproto_carrier_mtu_for_payload_mtu(sp_params, input MTU) >= 0
 * @param sp_params SPProto security parameters
 * @param otp_warning_count If using OTPs, after how many encoded packets to call the handler.
 *                          In this case, must be >0 and <=sp_params.otp_num.
 * @param handler OTP warning handler
 * @param user value to pass to handler
 * @param pg pending group
 * @return 1 on success, 0 on failure
 */
int SPProtoEncoder_Init (SPProtoEncoder *o, PacketRecvInterface *input, struct spproto_security_params sp_params, int otp_warning_count, SPProtoEncoder_handler handler, void *user, BPendingGroup *pg) WARN_UNUSED;

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

#endif
