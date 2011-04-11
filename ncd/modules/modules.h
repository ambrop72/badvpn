/**
 * @file modules.h
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

#ifndef BADVPN_NCD_MODULES_MODULES_H
#define BADVPN_NCD_MODULES_MODULES_H

#include <stddef.h>

#include <ncd/NCDModule.h>

extern const struct NCDModuleGroup ncdmodule_var;
extern const struct NCDModuleGroup ncdmodule_list;
extern const struct NCDModuleGroup ncdmodule_depend;
extern const struct NCDModuleGroup ncdmodule_multidepend;
extern const struct NCDModuleGroup ncdmodule_concat;
extern const struct NCDModuleGroup ncdmodule_concatv;
extern const struct NCDModuleGroup ncdmodule_concatlist;
extern const struct NCDModuleGroup ncdmodule_if;
extern const struct NCDModuleGroup ncdmodule_strcmp;
extern const struct NCDModuleGroup ncdmodule_regex_match;
extern const struct NCDModuleGroup ncdmodule_logical;
extern const struct NCDModuleGroup ncdmodule_sleep;
extern const struct NCDModuleGroup ncdmodule_print;
extern const struct NCDModuleGroup ncdmodule_blocker;
extern const struct NCDModuleGroup ncdmodule_ip_in_network;
extern const struct NCDModuleGroup ncdmodule_run;
extern const struct NCDModuleGroup ncdmodule_synchronous_process;
extern const struct NCDModuleGroup ncdmodule_process_manager;
extern const struct NCDModuleGroup ncdmodule_net_backend_waitdevice;
extern const struct NCDModuleGroup ncdmodule_net_backend_waitlink;
extern const struct NCDModuleGroup ncdmodule_net_backend_badvpn;
extern const struct NCDModuleGroup ncdmodule_net_backend_wpa_supplicant;
#ifdef BADVPN_USE_LINUX_RFKILL
extern const struct NCDModuleGroup ncdmodule_net_backend_rfkill;
#endif
extern const struct NCDModuleGroup ncdmodule_net_up;
extern const struct NCDModuleGroup ncdmodule_net_dns;
extern const struct NCDModuleGroup ncdmodule_net_iptables;
extern const struct NCDModuleGroup ncdmodule_net_ipv4_addr;
extern const struct NCDModuleGroup ncdmodule_net_ipv4_route;
extern const struct NCDModuleGroup ncdmodule_net_ipv4_dhcp;
#ifdef BADVPN_USE_LINUX_INPUT
extern const struct NCDModuleGroup ncdmodule_sys_evdev;
#endif

static const struct NCDModuleGroup *ncd_modules[] = {
    &ncdmodule_var,
    &ncdmodule_list,
    &ncdmodule_depend,
    &ncdmodule_multidepend,
    &ncdmodule_concat,
    &ncdmodule_concatv,
    &ncdmodule_concatlist,
    &ncdmodule_if,
    &ncdmodule_strcmp,
    &ncdmodule_regex_match,
    &ncdmodule_logical,
    &ncdmodule_sleep,
    &ncdmodule_print,
    &ncdmodule_blocker,
    &ncdmodule_ip_in_network,
    &ncdmodule_run,
    &ncdmodule_synchronous_process,
    &ncdmodule_process_manager,
    &ncdmodule_net_backend_waitdevice,
    &ncdmodule_net_backend_waitlink,
    &ncdmodule_net_backend_badvpn,
    &ncdmodule_net_backend_wpa_supplicant,
#ifdef BADVPN_USE_LINUX_RFKILL
    &ncdmodule_net_backend_rfkill,
#endif
    &ncdmodule_net_up,
    &ncdmodule_net_dns,
    &ncdmodule_net_iptables,
    &ncdmodule_net_ipv4_addr,
    &ncdmodule_net_ipv4_route,
    &ncdmodule_net_ipv4_dhcp,
#ifdef BADVPN_USE_LINUX_INPUT
    &ncdmodule_sys_evdev,
#endif
    NULL
};

#endif
