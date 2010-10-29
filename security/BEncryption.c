/**
 * @file BEncryption.c
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

#include <security/BEncryption.h>

int BEncryption_cipher_valid (int cipher)
{
    switch (cipher) {
        case BENCRYPTION_CIPHER_BLOWFISH:
        case BENCRYPTION_CIPHER_AES:
            return 1;
        default:
            return 0;
    }
}

int BEncryption_cipher_block_size (int cipher)
{
    switch (cipher) {
        case BENCRYPTION_CIPHER_BLOWFISH:
            return BENCRYPTION_CIPHER_BLOWFISH_BLOCK_SIZE;
        case BENCRYPTION_CIPHER_AES:
            return BENCRYPTION_CIPHER_AES_BLOCK_SIZE;
        default:
            ASSERT(0)
            return 0;
    }
}

int BEncryption_cipher_key_size (int cipher)
{
    switch (cipher) {
        case BENCRYPTION_CIPHER_BLOWFISH:
            return BENCRYPTION_CIPHER_BLOWFISH_KEY_SIZE;
        case BENCRYPTION_CIPHER_AES:
            return BENCRYPTION_CIPHER_AES_KEY_SIZE;
        default:
            ASSERT(0)
            return 0;
    }
}

void BEncryption_Init (BEncryption *enc, int mode, int cipher, uint8_t *key)
{
    ASSERT(!(mode&~(BENCRYPTION_MODE_ENCRYPT|BENCRYPTION_MODE_DECRYPT)))
    ASSERT((mode&BENCRYPTION_MODE_ENCRYPT) || (mode&BENCRYPTION_MODE_DECRYPT))
    
    enc->mode = mode;
    enc->cipher = cipher;
    
    #ifdef BADVPN_USE_CRYPTODEV
    
    switch (enc->cipher) {
        case BENCRYPTION_CIPHER_AES:
            enc->cryptodev.cipher = CRYPTO_AES_CBC;
            break;
        default:
            goto fail1;
    }
    
    if ((enc->cryptodev.fd = open("/dev/crypto", O_RDWR, 0)) < 0) {
        DEBUG("failed to open /dev/crypto");
        goto fail1;
    }
    
    if (ioctl(enc->cryptodev.fd, CRIOGET, &enc->cryptodev.cfd)) {
        DEBUG("failed ioctl(CRIOGET)");
        goto fail2;
    }
    
    struct session_op sess;
    memset(&sess, 0, sizeof(sess));
    sess.cipher = enc->cryptodev.cipher;
    sess.keylen = BEncryption_cipher_key_size(enc->cipher);
    sess.key = key;
    if (ioctl(enc->cryptodev.cfd, CIOCGSESSION, &sess)) {
        DEBUG("failed ioctl(CIOCGSESSION)");
        goto fail3;
    }
    
    enc->cryptodev.ses = sess.ses;
    enc->use_cryptodev = 1;
    
    goto success;
    
fail3:
    ASSERT_FORCE(close(enc->cryptodev.cfd) == 0)
fail2:
    ASSERT_FORCE(close(enc->cryptodev.fd) == 0)
fail1:
    
    enc->use_cryptodev = 0;
    
    #endif
    
    int res;
    
    switch (enc->cipher) {
        case BENCRYPTION_CIPHER_BLOWFISH:
            BF_set_key(&enc->blowfish, BENCRYPTION_CIPHER_BLOWFISH_KEY_SIZE, key);
            break;
        case BENCRYPTION_CIPHER_AES:
            if (enc->mode&BENCRYPTION_MODE_ENCRYPT) {
                res = AES_set_encrypt_key(key, 128, &enc->aes.encrypt);
                ASSERT(res >= 0)
            }
            if (enc->mode&BENCRYPTION_MODE_DECRYPT) {
                res = AES_set_decrypt_key(key, 128, &enc->aes.decrypt);
                ASSERT(res >= 0)
            }
            break;
        default:
            ASSERT(0)
            ;
    }
    
success:
    // init debug object
    DebugObject_Init(&enc->d_obj);
}

void BEncryption_Free (BEncryption *enc)
{
    // free debug object
    DebugObject_Free(&enc->d_obj);
    
    #ifdef BADVPN_USE_CRYPTODEV
    
    if (enc->use_cryptodev) {
        ASSERT_FORCE(ioctl(enc->cryptodev.cfd, CIOCFSESSION, &enc->cryptodev.ses) == 0)
        ASSERT_FORCE(close(enc->cryptodev.cfd) == 0)
        ASSERT_FORCE(close(enc->cryptodev.fd) == 0)
    }
    
    #endif
}

void BEncryption_Encrypt (BEncryption *enc, uint8_t *in, uint8_t *out, int len, uint8_t *iv)
{
    ASSERT(enc->mode&BENCRYPTION_MODE_ENCRYPT)
    ASSERT(len >= 0)
    ASSERT(len % BEncryption_cipher_block_size(enc->cipher) == 0)
    
    #ifdef BADVPN_USE_CRYPTODEV
    
    if (enc->use_cryptodev) {
        struct crypt_op cryp;
        memset(&cryp, 0, sizeof(cryp));
        cryp.ses = enc->cryptodev.ses;
        cryp.len = len;
        cryp.src = in;
        cryp.dst = out;
        cryp.iv = iv;
        cryp.op = COP_ENCRYPT;
        ASSERT_FORCE(ioctl(enc->cryptodev.cfd, CIOCCRYPT, &cryp) == 0)
        
        return;
    }
    
    #endif
    
    switch (enc->cipher) {
        case BENCRYPTION_CIPHER_BLOWFISH:
            BF_cbc_encrypt(in, out, len, &enc->blowfish, iv, BF_ENCRYPT);
            break;
        case BENCRYPTION_CIPHER_AES:
            AES_cbc_encrypt(in, out, len, &enc->aes.encrypt, iv, AES_ENCRYPT);
            break;
        default:
            ASSERT(0);
    }
}

void BEncryption_Decrypt (BEncryption *enc, uint8_t *in, uint8_t *out, int len, uint8_t *iv)
{
    ASSERT(enc->mode&BENCRYPTION_MODE_DECRYPT)
    ASSERT(len >= 0)
    ASSERT(len % BEncryption_cipher_block_size(enc->cipher) == 0)
    
    #ifdef BADVPN_USE_CRYPTODEV
    
    if (enc->use_cryptodev) {
        struct crypt_op cryp;
        memset(&cryp, 0, sizeof(cryp));
        cryp.ses = enc->cryptodev.ses;
        cryp.len = len;
        cryp.src = in;
        cryp.dst = out;
        cryp.iv = iv;
        cryp.op = COP_DECRYPT;
        ASSERT_FORCE(ioctl(enc->cryptodev.cfd, CIOCCRYPT, &cryp) == 0)
        
        return;
    }
    
    #endif
    
    switch (enc->cipher) {
        case BENCRYPTION_CIPHER_BLOWFISH:
            BF_cbc_encrypt(in, out, len, &enc->blowfish, iv, BF_DECRYPT);
            break;
        case BENCRYPTION_CIPHER_AES:
            AES_cbc_encrypt(in, out, len, &enc->aes.decrypt, iv, AES_DECRYPT);
            break;
        default:
            ASSERT(0);
    }
}
