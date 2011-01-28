/**
 * @file balign.h
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
 * Integer alignment macros.
 */

#ifndef BADVPN_MISC_BALIGN_H
#define BADVPN_MISC_BALIGN_H

#include <stddef.h>

/**
 * Aligns x up to n.
 */
static size_t balign_up (size_t x, size_t n)
{
    size_t r = x % n;
    return (r ? x + (n - r) : x);
}

/**
 * Aligns x down to n.
 */
static size_t balign_down (size_t x, size_t n)
{
    return (x - (x % n));
}

/**
 * Calculates the quotient of a and b, rounded up.
 */
static size_t bdivide_up (size_t a, size_t b)
{
    size_t r = a % b;
    return (r > 0 ? a / b + 1 : a / b);
}

#endif
