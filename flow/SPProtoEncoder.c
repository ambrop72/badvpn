/**
 * @file SPProtoEncoder.c
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

#include <string.h>
#include <stdlib.h>

#include <misc/debug.h>
#include <misc/balign.h>
#include <misc/offset.h>
#include <misc/byteorder.h>
#include <security/BRandom.h>
#include <security/BHash.h>

#include <flow/SPProtoEncoder.h>

static int can_encode (SPProtoEncoder *o)
{
    ASSERT(o->in_len >= 0)
    ASSERT(o->out_have)
    
    return (
        (!SPPROTO_HAVE_OTP(o->sp_params) || OTPGenerator_GetPosition(&o->otpgen) < o->sp_params.otp_num) &&
        (!SPPROTO_HAVE_ENCRYPTION(o->sp_params) || o->have_encryption_key)
    );
}

static int encode_packet (SPProtoEncoder *o)
{
    ASSERT(o->in_len >= 0)
    ASSERT(o->out_have)
    ASSERT(can_encode(o))
    
    ASSERT(o->in_len <= o->input_mtu)
    
    // plaintext is either output packet or our buffer
    uint8_t *plaintext = (!SPPROTO_HAVE_ENCRYPTION(o->sp_params) ? o->out : o->buf);
    
    // plaintext begins with header
    uint8_t *header = plaintext;
    
    // plaintext is header + payload
    int plaintext_len = SPPROTO_HEADER_LEN(o->sp_params) + o->in_len;
    
    // write OTP
    if (SPPROTO_HAVE_OTP(o->sp_params)) {
        struct spproto_otpdata *header_otpd = (struct spproto_otpdata *)(header + SPPROTO_HEADER_OTPDATA_OFF(o->group->sp_params));
        header_otpd->seed_id = htol16(o->otpgen_seed_id);
        header_otpd->otp = OTPGenerator_GetOTP(&o->otpgen);
    }
    
    // write hash
    if (SPPROTO_HAVE_HASH(o->sp_params)) {
        uint8_t *header_hash = header + SPPROTO_HEADER_HASH_OFF(o->sp_params);
        // zero hash field
        memset(header_hash, 0, o->hash_size);
        // calculate hash
        uint8_t hash[o->hash_size];
        BHash_calculate(o->sp_params.hash_mode, plaintext, plaintext_len, hash);
        // set hash field
        memcpy(header_hash, hash, o->hash_size);
    }
    
    int out_len;
    
    if (SPPROTO_HAVE_ENCRYPTION(o->sp_params)) {
        // encrypting pad(header + payload)
        int cyphertext_len = BALIGN_UP_N((plaintext_len + 1), o->enc_block_size);
        // write padding
        plaintext[plaintext_len] = 1;
        for (int i = plaintext_len + 1; i < cyphertext_len; i++) {
            plaintext[i] = 0;
        }
        // generate IV
        BRandom_randomize(o->out, o->enc_block_size);
        // copy IV because BEncryption_Encrypt changes the IV
        uint8_t iv[o->enc_block_size];
        memcpy(iv, o->out, o->enc_block_size);
        // encrypt
        BEncryption_Encrypt(&o->encryptor, plaintext, o->out + o->enc_block_size, cyphertext_len, iv);
        out_len = o->enc_block_size + cyphertext_len;
    } else {
        out_len = plaintext_len;
    }
    
    return out_len;
}

static void do_encode (SPProtoEncoder *o)
{
    ASSERT(o->in_len >= 0)
    ASSERT(o->out_have)
    ASSERT(can_encode(o))
    
    // encode
    int out_len = encode_packet(o);
    
    // finish packet
    o->in_len = -1;
    o->out_have = 0;
    PacketRecvInterface_Done(&o->output, out_len);
}

static void maybe_encode (SPProtoEncoder *o)
{
    if (o->in_len >= 0 && o->out_have && can_encode(o)) {
        do_encode(o);
    }
}

static void output_handler_recv (SPProtoEncoder *o, uint8_t *data)
{
    ASSERT(o->in_len == -1)
    ASSERT(!o->out_have)
    DebugObject_Access(&o->d_obj);
    
    // remember output packet
    o->out_have = 1;
    o->out = data;
    
    // determine plaintext location
    uint8_t *plaintext = (!SPPROTO_HAVE_ENCRYPTION(o->sp_params) ? o->out : o->buf);
    
    // schedule receive
    PacketRecvInterface_Receiver_Recv(o->input, plaintext + SPPROTO_HEADER_LEN(o->sp_params));
}

static void input_handler_done (SPProtoEncoder *o, int in_len)
{
    ASSERT(in_len >= 0)
    ASSERT(in_len <= o->input_mtu)
    ASSERT(o->in_len == -1)
    ASSERT(o->out_have)
    DebugObject_Access(&o->d_obj);
    
    // remember input packet
    o->in_len = in_len;
    
    // encode if possible
    if (can_encode(o)) {
        do_encode(o);
    }
}

int SPProtoEncoder_Init (SPProtoEncoder *o, struct spproto_security_params sp_params, PacketRecvInterface *input, BPendingGroup *pg)
{
    ASSERT(spproto_validate_security_params(sp_params))
    
    // init parameters
    o->sp_params = sp_params;
    o->input = input;
    
    // calculate hash size
    if (SPPROTO_HAVE_HASH(o->sp_params)) {
        o->hash_size = BHash_size(o->sp_params.hash_mode);
    }
    
    // calculate encryption block and key sizes
    if (SPPROTO_HAVE_ENCRYPTION(o->sp_params)) {
        o->enc_block_size = BEncryption_cipher_block_size(o->sp_params.encryption_mode);
        o->enc_key_size = BEncryption_cipher_key_size(o->sp_params.encryption_mode);
    }
    
    // init otp generator
    if (SPPROTO_HAVE_OTP(o->sp_params)) {
        if (!OTPGenerator_Init(&o->otpgen, o->sp_params.otp_num, o->sp_params.otp_mode)) {
            goto fail0;
        }
    }
    
    // have no encryption key
    if (SPPROTO_HAVE_ENCRYPTION(o->sp_params)) { 
        o->have_encryption_key = 0;
    }
    
    // remember input MTU
    o->input_mtu = PacketRecvInterface_GetMTU(o->input);
    
    // calculate output MTU
    o->output_mtu = spproto_carrier_mtu_for_payload_mtu(o->sp_params, o->input_mtu);
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    // have no input in buffer
    o->in_len = -1;
    
    // init output
    PacketRecvInterface_Init(&o->output, o->output_mtu, (PacketRecvInterface_handler_recv)output_handler_recv, o, pg);
    
    // have no output available
    o->out_have = 0;
    
    // allocate plaintext buffer
    if (SPPROTO_HAVE_ENCRYPTION(o->sp_params)) {
        int buf_size = BALIGN_UP_N((SPPROTO_HEADER_LEN(o->sp_params) + o->input_mtu + 1), o->enc_block_size);
        if (!(o->buf = malloc(buf_size))) {
            goto fail1;
        }
    }
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    PacketRecvInterface_Free(&o->output);
    if (SPPROTO_HAVE_OTP(o->sp_params)) {
        OTPGenerator_Free(&o->otpgen);
    }
fail0:
    return 0;
}

void SPProtoEncoder_Free (SPProtoEncoder *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free plaintext buffer
    if (SPPROTO_HAVE_ENCRYPTION(o->sp_params)) {
        free(o->buf);
    }
    
    // free output
    PacketRecvInterface_Free(&o->output);
    
    // free encryptor
    if (SPPROTO_HAVE_ENCRYPTION(o->sp_params) && o->have_encryption_key) {
        BEncryption_Free(&o->encryptor);
    }
    
    // free otp generator
    if (SPPROTO_HAVE_OTP(o->sp_params)) {
        OTPGenerator_Free(&o->otpgen);
    }
}

PacketRecvInterface * SPProtoEncoder_GetOutput (SPProtoEncoder *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}

void SPProtoEncoder_SetEncryptionKey (SPProtoEncoder *o, uint8_t *encryption_key)
{
    ASSERT(SPPROTO_HAVE_ENCRYPTION(o->sp_params))
    DebugObject_Access(&o->d_obj);
    
    // free encryptor
    if (o->have_encryption_key) {
        BEncryption_Free(&o->encryptor);
    }
    
    // init encryptor
    BEncryption_Init(&o->encryptor, BENCRYPTION_MODE_ENCRYPT, o->sp_params.encryption_mode, encryption_key);
    
    // have encryption key
    o->have_encryption_key = 1;
    
    // possibly continue I/O
    maybe_encode(o);
}

void SPProtoEncoder_RemoveEncryptionKey (SPProtoEncoder *o)
{
    ASSERT(SPPROTO_HAVE_ENCRYPTION(o->sp_params))
    DebugObject_Access(&o->d_obj);
    
    if (o->have_encryption_key) {
        // free encryptor
        BEncryption_Free(&o->encryptor);
        
        // have no encryption key
        o->have_encryption_key = 0;
    }
}

void SPProtoEncoder_SetOTPSeed (SPProtoEncoder *o, uint16_t seed_id, uint8_t *key, uint8_t *iv)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    DebugObject_Access(&o->d_obj);
    
    // give seed to OTP generator
    OTPGenerator_SetSeed(&o->otpgen, key, iv);
    
    // remember seed ID
    o->otpgen_seed_id = seed_id;
    
    // possibly continue I/O
    maybe_encode(o);
}

void SPProtoEncoder_RemoveOTPSeed (SPProtoEncoder *o)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    DebugObject_Access(&o->d_obj);
    
    // reset OTP generator
    OTPGenerator_Reset(&o->otpgen);
}

int SPProtoEncoder_GetOTPPosition (SPProtoEncoder *o)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    DebugObject_Access(&o->d_obj);
    
    return OTPGenerator_GetPosition(&o->otpgen);
}
