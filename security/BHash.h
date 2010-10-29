/**
 * @file BHash.h
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
 * Cryptographic hash funtions abstraction.
 */

#ifndef BADVPN_SECURITY_BHASH_H
#define BADVPN_SECURITY_BHASH_H

#include <stdint.h>

#include <openssl/md5.h>
#include <openssl/sha.h>

#include <misc/debug.h>

#define BHASH_TYPE_MD5 1
#define BHASH_TYPE_MD5_SIZE 16

#define BHASH_TYPE_SHA1 2
#define BHASH_TYPE_SHA1_SIZE 20

/**
 * Checks if the given hash type number is valid.
 * 
 * @param type hash type number
 * @return 1 if valid, 0 if not
 */
int BHash_type_valid (int type);

/**
 * Returns the size of a hash.
 * 
 * @param cipher hash type number. Must be valid.
 * @return hash size in bytes
 */
int BHash_size (int type);

/**
 * Calculates a hash.
 * 
 * @param type hash type number. Must be valid.
 * @param data data to calculate the hash of
 * @param data_len length of data
 * @param out the hash will be written here. Must not overlap with data.
 */
void BHash_calculate (int type, uint8_t *data, int data_len, uint8_t *out);

#endif
