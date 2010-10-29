/**
 * @file BHash.c
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

#include <security/BHash.h>

int BHash_type_valid (int type)
{
    switch (type) {
        case BHASH_TYPE_MD5:
        case BHASH_TYPE_SHA1:
            return 1;
        default:
            return 0;
    }
}

int BHash_size (int type)
{
    switch (type) {
        case BHASH_TYPE_MD5:
            return BHASH_TYPE_MD5_SIZE;
        case BHASH_TYPE_SHA1:
            return BHASH_TYPE_SHA1_SIZE;
        default:
            ASSERT(0)
            return 0;
    }
}

void BHash_calculate (int type, uint8_t *data, int data_len, uint8_t *out)
{
    switch (type) {
        case BHASH_TYPE_MD5:
            MD5(data, data_len, out);
            break;
        case BHASH_TYPE_SHA1:
            SHA1(data, data_len, out);
            break;
        default:
            ASSERT(0)
            ;
    }
}
