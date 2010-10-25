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

#include <stdlib.h>

/**
 * Aligns the integer x up to n bytes.
 */
#define BALIGN_UP_N(x,n) \
    ({\
        typeof (x) _x = (x);\
        typeof (n) _n = (n);\
        typeof (x) _r = _x % _n;\
        _r ? _x + (_n - _r) : _x;\
    })

/**
 * Aligns the integer x down to n bytes.
 */
#define BALIGN_DOWN_N(x,n) \
    ({\
        typeof (x) _x = (x);\
        typeof (n) _n = (n);\
        _x - (_x % _n);\
    })

/**
 * Calculates the quotient of integers a and b, rounded up.
 */
#define BDIVIDE_UP(a,b) \
    ({\
        typeof (a) _a = (a);\
        typeof (b) _b = (b);\
        typeof (a) _r = _a % _b;\
        _r > 0 ? _a / _b + 1 : _a / _b;\
    })

#endif
