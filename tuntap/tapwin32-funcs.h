/**
 * @file tapwin32-funcs.h
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

#ifndef BADVPN_TUNTAP_TAPWIN32_FUNCS_H
#define BADVPN_TUNTAP_TAPWIN32_FUNCS_H

#include <stdint.h>
#include <windows.h>

#define TAPWIN32_MAX_REG_SIZE 256

int tapwin32_parse_tap_spec (char *name, char *out_component_id, char *out_human_name);
int tapwin32_parse_tun_spec (char *name, char *out_component_id, char *out_human_name, uint32_t out_addrs[3]);
int tapwin32_find_device (char *device_component_id, char *device_name, char (*device_path)[TAPWIN32_MAX_REG_SIZE]);

#endif
