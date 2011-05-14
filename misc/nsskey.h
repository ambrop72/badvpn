/**
 * @file nsskey.h
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
 * Function for opening a NSS certificate and its private key.
 */

#ifndef BADVPN_MISC_NSSKEY_H
#define BADVPN_MISC_NSSKEY_H

#include <stdlib.h>

#include <prerror.h>
#include <cert.h>
#include <keyhi.h>
#include <pk11func.h>

#include <base/BLog.h>

#include <generated/blog_channel_nsskey.h>

/**
 * Opens a NSS certificate and its private key.
 * 
 * @param name name of the certificate
 * @param out_cert on success, the certificate will be returned here. Should be
 *                 released with CERT_DestroyCertificate.
 * @param out_key on success, the private key will be returned here. Should be
 *                released with SECKEY_DestroyPrivateKey.
 * @return 1 on success, 0 on failure
 */
static int open_nss_cert_and_key (char *name, CERTCertificate **out_cert, SECKEYPrivateKey **out_key) WARN_UNUSED;

static SECKEYPrivateKey * find_nss_private_key (char *name)
{
    SECKEYPrivateKey *key = NULL;

    PK11SlotList *slot_list = PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_FALSE, PR_FALSE, NULL);
    if (!slot_list) {
        return NULL;
    }
    
    PK11SlotListElement *slot_entry;
    for (slot_entry = slot_list->head; !key && slot_entry; slot_entry = slot_entry->next) {
        SECKEYPrivateKeyList *key_list = PK11_ListPrivKeysInSlot(slot_entry->slot, name, NULL);
        if (!key_list) {
            BLog(BLOG_ERROR, "PK11_ListPrivKeysInSlot failed");
            continue;
        }
        
        SECKEYPrivateKeyListNode *key_node;
        for (key_node = PRIVKEY_LIST_HEAD(key_list); !key && !PRIVKEY_LIST_END(key_node, key_list); key_node = PRIVKEY_LIST_NEXT(key_node)) {
            char *key_name = PK11_GetPrivateKeyNickname(key_node->key);
            if (!key_name || strcmp(key_name, name)) {
                PORT_Free((void *)key_name);
                continue;
            }
            PORT_Free((void *)key_name);
            
            key = SECKEY_CopyPrivateKey(key_node->key);
        }
        
        SECKEY_DestroyPrivateKeyList(key_list);
    }
    
    PK11_FreeSlotList(slot_list);
    
    return key;
}

int open_nss_cert_and_key (char *name, CERTCertificate **out_cert, SECKEYPrivateKey **out_key)
{
    CERTCertificate *cert;
    cert = CERT_FindCertByNicknameOrEmailAddr(CERT_GetDefaultCertDB(), name);
    if (!cert) {
        BLog(BLOG_ERROR, "CERT_FindCertByName failed (%d)", (int)PR_GetError());
        return 0;
    }
    
    SECKEYPrivateKey *key = find_nss_private_key(name);
    if (!key) {
        BLog(BLOG_ERROR, "Failed to find private key");
        CERT_DestroyCertificate(cert);
        return 0;
    }
    
    *out_cert = cert;
    *out_key = key;
    return 1;
}

#endif
