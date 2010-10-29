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
#include <security/BRandom.h>
#include <security/BHash.h>

#include <flow/SPProtoEncoder.h>

static int can_encode (SPProtoEncoder *o)
{
    ASSERT(o->in_len >= 0)
    ASSERT(o->out_have)
    
    return (
        (!SPPROTO_HAVE_OTP(o->group->sp_params) || OTPGenerator_GetPosition(&o->group->otpgen) < o->group->sp_params.otp_num) &&
        (!SPPROTO_HAVE_ENCRYPTION(o->group->sp_params) || o->group->have_encryption_key)
    );
}

static int encode_packet (SPProtoEncoder *o)
{
    ASSERT(o->in_len >= 0)
    ASSERT(o->in_len <= o->input_mtu)
    ASSERT(o->out_have)
    ASSERT(can_encode(o))
    
    // plaintext is either output packet or our buffer
    uint8_t *plaintext = (!SPPROTO_HAVE_ENCRYPTION(o->group->sp_params) ? o->out : o->buf);
    
    // plaintext begins with header
    uint8_t *header = plaintext;
    
    // plaintext is header + payload
    int plaintext_len = SPPROTO_HEADER_LEN(o->group->sp_params) + o->in_len;
    
    // write OTP
    if (SPPROTO_HAVE_OTP(o->group->sp_params)) {
        struct spproto_otpdata *header_otpd = (struct spproto_otpdata *)(header + SPPROTO_HEADER_OTPDATA_OFF(o->group->sp_params));
        header_otpd->seed_id = o->group->otpgen_seed_id;
        header_otpd->otp = OTPGenerator_GetOTP(&o->group->otpgen);
    }
    
    // write hash
    if (SPPROTO_HAVE_HASH(o->group->sp_params)) {
        uint8_t *header_hash = header + SPPROTO_HEADER_HASH_OFF(o->group->sp_params);
        // zero hash field
        memset(header_hash, 0, o->group->hash_size);
        // calculate hash
        uint8_t hash[o->group->hash_size];
        BHash_calculate(o->group->sp_params.hash_mode, plaintext, plaintext_len, hash);
        // set hash field
        memcpy(header_hash, hash, o->group->hash_size);
    }
    
    int out_len;
    
    if (SPPROTO_HAVE_ENCRYPTION(o->group->sp_params)) {
        // encrypting pad(header + payload)
        int cyphertext_len = BALIGN_UP_N((plaintext_len + 1), o->group->enc_block_size);
        // write padding
        plaintext[plaintext_len] = 1;
        for (int i = plaintext_len + 1; i < cyphertext_len; i++) {
            plaintext[i] = 0;
        }
        // generate IV
        BRandom_randomize(o->out, o->group->enc_block_size);
        // copy IV because BEncryption_Encrypt changes the IV
        uint8_t iv[o->group->enc_block_size];
        memcpy(iv, o->out, o->group->enc_block_size);
        // encrypt
        BEncryption_Encrypt(&o->group->encryptor, plaintext, o->out + o->group->enc_block_size, cyphertext_len, iv);
        out_len = o->group->enc_block_size + cyphertext_len;
    } else {
        out_len = plaintext_len;
    }
    
    o->in_len = -1;
    o->out_have = 0;
    
    return out_len;
}

static int output_handler_recv (SPProtoEncoder *o, uint8_t *data, int *data_len)
{
    ASSERT(o->in_len == -1)
    ASSERT(!o->out_have)
    
    // remember output packet
    o->out_have = 1;
    o->out = data;
    
    // determine plaintext location
    uint8_t *plaintext = (!SPPROTO_HAVE_ENCRYPTION(o->group->sp_params) ? o->out : o->buf);
    
    // try to receive input packet
    int in_len;
    DEAD_ENTER(o->dead)
    int res = PacketRecvInterface_Receiver_Recv(o->input, plaintext + SPPROTO_HEADER_LEN(o->group->sp_params), &in_len);
    if (DEAD_LEAVE(o->dead)) {
        return -1;
    }
    
    ASSERT(res == 0 || res == 1)
    
    if (!res) {
        return 0;
    }
    
    ASSERT(in_len >= 0 && in_len <= o->input_mtu)
    
    // remember input packet
    o->in_len = in_len;
    
    // check if we can encode
    if (!can_encode(o)) {
        return 0;
    }
    
    // encode
    *data_len = encode_packet(o);
    
    return 1;
}

static void input_handler_done (SPProtoEncoder *o, int in_len)
{
    ASSERT(o->in_len == -1)
    ASSERT(o->out_have)
    ASSERT(in_len >= 0 && in_len <= o->input_mtu)
    
    // remember input packet
    o->in_len = in_len;
    
    // check if we can encode
    if (!can_encode(o)) {
        return;
    }
    
    // encode
    int out_len = encode_packet(o);
    
    // inform output
    PacketRecvInterface_Done(&o->output, out_len);
    return;
}

static void job_handler (SPProtoEncoder *o)
{
    if (o->in_len >= 0 && o->out_have && can_encode(o)) {
        // encode
        int out_len = encode_packet(o);
        
        // inform output
        PacketRecvInterface_Done(&o->output, out_len);
        return;
    }
}

static void schedule_jobs (SPProtoEncoderGroup *o)
{
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &o->encoders_list);
    LinkedList2Node *node;
    while (node = LinkedList2Iterator_Next(&it)) {
        SPProtoEncoder *enc = UPPER_OBJECT(node, SPProtoEncoder, group_list_node);
        BPending_Set(&enc->continue_job);
    }
}

int SPProtoEncoderGroup_Init (SPProtoEncoderGroup *o, struct spproto_security_params sp_params)
{
    ASSERT(spproto_validate_security_params(sp_params))
    
    // init parameters
    o->sp_params = sp_params;
    
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
            goto fail1;
        }
    }
    
    // have no encryption key
    if (SPPROTO_HAVE_ENCRYPTION(o->sp_params)) { 
        o->have_encryption_key = 0;
    }
    
    // init encoders list
    LinkedList2_Init(&o->encoders_list);
    
    // init debug object
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    return 0;
}

void SPProtoEncoderGroup_Free (SPProtoEncoderGroup *o)
{
    ASSERT(LinkedList2_IsEmpty(&o->encoders_list))
    DebugObject_Free(&o->d_obj);
    
    // free encryptor
    if (SPPROTO_HAVE_ENCRYPTION(o->sp_params) && o->have_encryption_key) {
        BEncryption_Free(&o->encryptor);
    }
    
    // free otp generator
    if (SPPROTO_HAVE_OTP(o->sp_params)) {
        OTPGenerator_Free(&o->otpgen);
    }
}

void SPProtoEncoderGroup_SetEncryptionKey (SPProtoEncoderGroup *o, uint8_t *encryption_key)
{
    ASSERT(SPPROTO_HAVE_ENCRYPTION(o->sp_params))
    
    // free encryptor
    if (o->have_encryption_key) {
        BEncryption_Free(&o->encryptor);
    }
    
    // init encryptor
    BEncryption_Init(&o->encryptor, BENCRYPTION_MODE_ENCRYPT, o->sp_params.encryption_mode, encryption_key);
    
    // have encryption key
    o->have_encryption_key = 1;
    
    // set jobs
    schedule_jobs(o);
}

void SPProtoEncoderGroup_RemoveEncryptionKey (SPProtoEncoderGroup *o)
{
    ASSERT(SPPROTO_HAVE_ENCRYPTION(o->sp_params))
    
    if (o->have_encryption_key) {
        // free encryptor
        BEncryption_Free(&o->encryptor);
        
        // have no encryption key
        o->have_encryption_key = 0;
    }
}

void SPProtoEncoderGroup_SetOTPSeed (SPProtoEncoderGroup *o, uint16_t seed_id, uint8_t *key, uint8_t *iv)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    
    // give seed to OTP generator
    OTPGenerator_SetSeed(&o->otpgen, key, iv);
    
    // remember seed ID
    o->otpgen_seed_id = seed_id;
    
    // set jobs
    schedule_jobs(o);
}

void SPProtoEncoderGroup_RemoveOTPSeed (SPProtoEncoderGroup *o)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    
    // reset OTP generator
    OTPGenerator_Reset(&o->otpgen);
}

int SPProtoEncoderGroup_GetOTPPosition (SPProtoEncoderGroup *o)
{
    ASSERT(SPPROTO_HAVE_OTP(o->sp_params))
    
    return OTPGenerator_GetPosition(&o->otpgen);
}

int SPProtoEncoder_Init (SPProtoEncoder *o, SPProtoEncoderGroup *group, PacketRecvInterface *input, BPendingGroup *pg)
{
    // init parameters
    o->group = group;
    o->input = input;
    
    // init dead var
    DEAD_INIT(o->dead);
    
    // remember input MTU
    o->input_mtu = PacketRecvInterface_GetMTU(o->input);
    
    // calculate output MTU
    o->output_mtu = spproto_carrier_mtu_for_payload_mtu(o->group->sp_params, o->input_mtu);
    
    // init input
    PacketRecvInterface_Receiver_Init(o->input, (PacketRecvInterface_handler_done)input_handler_done, o);
    
    // have no input in buffer
    o->in_len = -1;
    
    // init output
    PacketRecvInterface_Init(&o->output, o->output_mtu, (PacketRecvInterface_handler_recv)output_handler_recv, o);
    
    // have no output available
    o->out_have = 0;
    
    // allocate plaintext buffer
    if (SPPROTO_HAVE_ENCRYPTION(o->group->sp_params)) {
        int buf_size = BALIGN_UP_N((SPPROTO_HEADER_LEN(o->group->sp_params) + o->input_mtu + 1), o->group->enc_block_size);
        if (!(o->buf = malloc(buf_size))) {
            goto fail1;
        }
    }
    
    // insert to group list
    LinkedList2_Append(&o->group->encoders_list, &o->group_list_node);
    
    // init pending job
    BPending_Init(&o->continue_job, pg, (BPending_handler)job_handler, o);
    
    // init debug object
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    PacketRecvInterface_Free(&o->output);
    return 0;
}

void SPProtoEncoder_Free (SPProtoEncoder *o)
{
    // free debug object
    DebugObject_Free(&o->d_obj);
    
    // free pending job
    BPending_Free(&o->continue_job);
    
    // remove from group list
    LinkedList2_Remove(&o->group->encoders_list, &o->group_list_node);
    
    // free plaintext buffer
    if (SPPROTO_HAVE_ENCRYPTION(o->group->sp_params)) {
        free(o->buf);
    }
    
    // free output
    PacketRecvInterface_Free(&o->output);
    
    // kill dead var
    DEAD_KILL(o->dead);
}

PacketRecvInterface * SPProtoEncoder_GetOutput (SPProtoEncoder *o)
{
    return &o->output;
}
