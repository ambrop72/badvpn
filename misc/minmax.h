/**
 * @file minmax.h
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
 * Minimum and maximum macros.
 */

#ifndef BADVPN_MISC_MINMAX_H
#define BADVPN_MISC_MINMAX_H

#define BMIN(a,b) \
    ({\
        typeof (a) _a = (a);\
        typeof (b) _b = (b);\
        (_a < _b ? _a : _b);\
    })

#define BMAX(a,b) \
    ({\
        typeof (a) _a = (a);\
        typeof (b) _b = (b);\
        (_a > _b ? _a : _b);\
    })

#endif
