/**
 * @file offset.h
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
 * Macros for determining offsets of members in structs.
 */

#ifndef BADVPN_MISC_OFFSET_H
#define BADVPN_MISC_OFFSET_H

#include <stddef.h>
#include <stdint.h>

/**
 * Returns a pointer to a struct, given a pointer to its member.
 */
#define UPPER_OBJECT(_ptr, _object_type, _field_name) ((_object_type *)((uint8_t *)(_ptr) - offsetof(_object_type, _field_name)))

/**
 * Returns the offset of one struct member from another.
 * Expands to an int.
 */
#define OFFSET_DIFF(_object_type, _field1, _field2) ((int)offsetof(_object_type, _field1) - (int)offsetof(_object_type, _field2))

#endif
