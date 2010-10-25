/**
 * @file jenkins_hash.h
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
 * Jenkins hash functions for use in hash tables.
 * The resulting hashes depend on the endianness of the machine.
 */

#ifndef BADVPN_MISC_JENKINS_HASH_H
#define BADVPN_MISC_JENKINS_HASH_H

#include <stdint.h>

static uint32_t jenkins_one_at_a_time_hash (uint8_t *key, int key_len);
static uint32_t jenkins_lookup2_hash (uint8_t *k, uint32_t length, uint32_t initval);

uint32_t jenkins_one_at_a_time_hash (uint8_t *key, int key_len)
{
    uint32_t hash = 0;
    int i;
    
    for (i = 0; i < key_len; i++) {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    
    return hash;
}

#define lookup2_hashsize(n) ((uint32_t)1<<(n))
#define lookup2_hashmask(n) (hashsize(n)-1)
#define lookup2_mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

uint32_t jenkins_lookup2_hash (uint8_t *k, uint32_t length, uint32_t initval)
{
   uint32_t a,b,c,len;

   /* Set up the internal state */
   len = length;
   a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
   c = initval;         /* the previous hash value */

   /*---------------------------------------- handle most of the key */
   while (len >= 12)
   {
      a += (k[0] +((uint32_t)k[1]<<8) +((uint32_t)k[2]<<16) +((uint32_t)k[3]<<24));
      b += (k[4] +((uint32_t)k[5]<<8) +((uint32_t)k[6]<<16) +((uint32_t)k[7]<<24));
      c += (k[8] +((uint32_t)k[9]<<8) +((uint32_t)k[10]<<16)+((uint32_t)k[11]<<24));
      lookup2_mix(a,b,c);
      k += 12; len -= 12;
   }

   /*------------------------------------- handle the last 11 bytes */
   c += length;
   switch(len)              /* all the case statements fall through */
   {
   case 11: c+=((uint32_t)k[10]<<24);
   case 10: c+=((uint32_t)k[9]<<16);
   case 9 : c+=((uint32_t)k[8]<<8);
      /* the first byte of c is reserved for the length */
   case 8 : b+=((uint32_t)k[7]<<24);
   case 7 : b+=((uint32_t)k[6]<<16);
   case 6 : b+=((uint32_t)k[5]<<8);
   case 5 : b+=k[4];
   case 4 : a+=((uint32_t)k[3]<<24);
   case 3 : a+=((uint32_t)k[2]<<16);
   case 2 : a+=((uint32_t)k[1]<<8);
   case 1 : a+=k[0];
     /* case 0: nothing left to add */
   }
   lookup2_mix(a,b,c);
   /*-------------------------------------------- report the result */
   return c;
}

#endif
