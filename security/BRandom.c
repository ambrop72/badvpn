/**
 * @file BRandom.c
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

#include <openssl/rand.h>

#include <misc/debug.h>

#include <security/BRandom.h>

void BRandom_randomize (uint8_t *buf, int len)
{
    ASSERT(len >= 0)
    
    DEBUG_ZERO_MEMORY(buf, len)
    ASSERT_FORCE(RAND_bytes(buf, len) == 1)
}
