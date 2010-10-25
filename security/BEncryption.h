/**
 * @file BEncryption.h
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
 * Block cipher encryption abstraction.
 */

#ifndef BADVPN_SECURITY_BENCRYPTION_H
#define BADVPN_SECURITY_BENCRYPTION_H

#include <stdint.h>
#include <string.h>

#ifdef BADVPN_USE_CRYPTODEV
#include <crypto/cryptodev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#endif

#include <openssl/blowfish.h>
#include <openssl/aes.h>

#include <misc/debug.h>
#include <system/DebugObject.h>

#define BENCRYPTION_MODE_ENCRYPT 1
#define BENCRYPTION_MODE_DECRYPT 2

#define BENCRYPTION_CIPHER_BLOWFISH 1
#define BENCRYPTION_CIPHER_BLOWFISH_BLOCK_SIZE 8
#define BENCRYPTION_CIPHER_BLOWFISH_KEY_SIZE 16

#define BENCRYPTION_CIPHER_AES 2
#define BENCRYPTION_CIPHER_AES_BLOCK_SIZE 16
#define BENCRYPTION_CIPHER_AES_KEY_SIZE 16

/**
 * Block cipher encryption abstraction.
 */
typedef struct {
    DebugObject d_obj;
    int mode;
    int cipher;
    #ifdef BADVPN_USE_CRYPTODEV
    int use_cryptodev;
    #endif
    union {
        BF_KEY blowfish;
        struct {
            AES_KEY encrypt;
            AES_KEY decrypt;
        } aes;
        #ifdef BADVPN_USE_CRYPTODEV
        struct {
            int fd;
            int cfd;
            int cipher;
            uint32_t ses;
        } cryptodev;
        #endif
    };
} BEncryption;

/**
 * Checks if the given cipher number is valid.
 * 
 * @param cipher cipher number
 * @return 1 if valid, 0 if not
 */
static int BEncryption_cipher_valid (int cipher);

/**
 * Returns the block size of a cipher.
 * 
 * @param cipher cipher number. Must be valid.
 * @return block size in bytes
 */
static int BEncryption_cipher_block_size (int cipher);

/**
 * Returns the key size of a cipher.
 * 
 * @param cipher cipher number. Must be valid.
 * @return key size in bytes
 */
static int BEncryption_cipher_key_size (int cipher);

/**
 * Initializes the object.
 * 
 * @param enc the object
 * @param mode whether encryption or decryption is to be done, or both.
 *             Must be a bitwise-OR of at least one of BENCRYPTION_MODE_ENCRYPT
 *             and BENCRYPTION_MODE_DECRYPT.
 * @param cipher cipher number. Must be valid.
 * @param key encryption key
 */
static void BEncryption_Init (BEncryption *enc, int mode, int cipher, uint8_t *key);

/**
 * Frees the object.
 * 
 * @param enc the object
 */
static void BEncryption_Free (BEncryption *enc);

/**
 * Encrypts data.
 * The object must have been initialized with mode including
 * BENCRYPTION_MODE_ENCRYPT.
 * 
 * @param enc the object
 * @param in data to encrypt
 * @param out ciphertext output
 * @param len number of bytes to encrypt. Must be >=0 and a multiple of
 *            block size.
 * @param iv initialization vector. Updated such that continuing a previous encryption
 *           starting with the updated IV is equivalent to performing just one encryption.
 */
static void BEncryption_Encrypt (BEncryption *enc, uint8_t *in, uint8_t *out, int len, uint8_t *iv);

/**
 * Decrypts data.
 * The object must have been initialized with mode including
 * BENCRYPTION_MODE_DECRYPT.
 * 
 * @param enc the object
 * @param in data to decrypt
 * @param out plaintext output
 * @param len number of bytes to decrypt. Must be >=0 and a multiple of
 *            block size.
 * @param iv initialization vector. Updated such that continuing a previous decryption
 *           starting with the updated IV is equivalent to performing just one decryption.
 */
static void BEncryption_Decrypt (BEncryption *enc, uint8_t *in, uint8_t *out, int len, uint8_t *iv);

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

#endif
