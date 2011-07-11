/**
 * @file bsort.h
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
 * Sorting functions.
 */

#ifndef BADVPN_MISC_BSORT_H
#define BADVPN_MISC_BSORT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <misc/debug.h>
#include <misc/balloc.h>

typedef int (*BSort_comparator) (const void *e1, const void *e2);

static void BInsertionSort (void *arr, size_t count, size_t esize, BSort_comparator compatator, void *temp);

void BInsertionSort (void *arr, size_t count, size_t esize, BSort_comparator compatator, void *temp)
{
    ASSERT(esize > 0)
    
    for (size_t i = 0; i < count; i++) {
        size_t j = i;
        while (j > 0) {
            uint8_t *x = (uint8_t *)arr + (j - 1) * esize;
            uint8_t *y = (uint8_t *)arr + j * esize;
            int c = compatator(x, y);
            if (c <= 0) {
                break;
            }
            memcpy(temp, x, esize);
            memcpy(x, y, esize);
            memcpy(y, temp, esize);
            j--;
        }
    }
}

#endif
