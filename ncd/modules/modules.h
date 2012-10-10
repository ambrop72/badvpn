/**
 * @file modules.h
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BADVPN_NCD_MODULES_MODULES_H
#define BADVPN_NCD_MODULES_MODULES_H

#include <stddef.h>

#include <ncd/NCDModule.h>

extern struct NCDModuleGroup ncdmodule_var;
extern struct NCDModuleGroup ncdmodule_list;
extern struct NCDModuleGroup ncdmodule_depend;
extern struct NCDModuleGroup ncdmodule_multidepend;
extern struct NCDModuleGroup ncdmodule_dynamic_depend;
extern struct NCDModuleGroup ncdmodule_concat;
extern struct NCDModuleGroup ncdmodule_concatv;
extern struct NCDModuleGroup ncdmodule_if;
extern struct NCDModuleGroup ncdmodule_strcmp;
extern struct NCDModuleGroup ncdmodule_regex_match;
extern struct NCDModuleGroup ncdmodule_logical;
extern struct NCDModuleGroup ncdmodule_sleep;
extern struct NCDModuleGroup ncdmodule_print;
extern struct NCDModuleGroup ncdmodule_blocker;
extern struct NCDModuleGroup ncdmodule_run;
extern struct NCDModuleGroup ncdmodule_runonce;
extern struct NCDModuleGroup ncdmodule_daemon;
extern struct NCDModuleGroup ncdmodule_spawn;
extern struct NCDModuleGroup ncdmodule_call;
extern struct NCDModuleGroup ncdmodule_imperative;
extern struct NCDModuleGroup ncdmodule_ref;
extern struct NCDModuleGroup ncdmodule_index;
extern struct NCDModuleGroup ncdmodule_alias;
extern struct NCDModuleGroup ncdmodule_process_manager;
extern struct NCDModuleGroup ncdmodule_ondemand;
extern struct NCDModuleGroup ncdmodule_foreach;
extern struct NCDModuleGroup ncdmodule_choose;
extern struct NCDModuleGroup ncdmodule_from_string;
extern struct NCDModuleGroup ncdmodule_to_string;
extern struct NCDModuleGroup ncdmodule_value;
extern struct NCDModuleGroup ncdmodule_try;
extern struct NCDModuleGroup ncdmodule_net_backend_waitdevice;
extern struct NCDModuleGroup ncdmodule_net_backend_waitlink;
extern struct NCDModuleGroup ncdmodule_net_backend_badvpn;
extern struct NCDModuleGroup ncdmodule_net_backend_wpa_supplicant;
#ifdef BADVPN_USE_LINUX_RFKILL
extern struct NCDModuleGroup ncdmodule_net_backend_rfkill;
#endif
extern struct NCDModuleGroup ncdmodule_net_up;
extern struct NCDModuleGroup ncdmodule_net_dns;
extern struct NCDModuleGroup ncdmodule_net_iptables;
extern struct NCDModuleGroup ncdmodule_net_ipv4_addr;
extern struct NCDModuleGroup ncdmodule_net_ipv4_route;
extern struct NCDModuleGroup ncdmodule_net_ipv4_dhcp;
extern struct NCDModuleGroup ncdmodule_net_ipv4_arp_probe;
extern struct NCDModuleGroup ncdmodule_net_watch_interfaces;
extern struct NCDModuleGroup ncdmodule_sys_watch_input;
extern struct NCDModuleGroup ncdmodule_sys_watch_usb;
#ifdef BADVPN_USE_LINUX_INPUT
extern struct NCDModuleGroup ncdmodule_sys_evdev;
#endif
#ifdef BADVPN_USE_INOTIFY
extern struct NCDModuleGroup ncdmodule_sys_watch_directory;
#endif
extern struct NCDModuleGroup ncdmodule_sys_request_server;
extern struct NCDModuleGroup ncdmodule_net_ipv6_wait_dynamic_addr;
extern struct NCDModuleGroup ncdmodule_sys_request_client;
extern struct NCDModuleGroup ncdmodule_exit;
extern struct NCDModuleGroup ncdmodule_getargs;
extern struct NCDModuleGroup ncdmodule_arithmetic;
extern struct NCDModuleGroup ncdmodule_parse;
extern struct NCDModuleGroup ncdmodule_valuemetic;
extern struct NCDModuleGroup ncdmodule_file;
extern struct NCDModuleGroup ncdmodule_netmask;
extern struct NCDModuleGroup ncdmodule_implode;
extern struct NCDModuleGroup ncdmodule_call2;
extern struct NCDModuleGroup ncdmodule_assert;
extern struct NCDModuleGroup ncdmodule_reboot;
extern struct NCDModuleGroup ncdmodule_explode;
extern struct NCDModuleGroup ncdmodule_net_ipv6_addr;
extern struct NCDModuleGroup ncdmodule_net_ipv6_route;
extern struct NCDModuleGroup ncdmodule_net_ipv4_addr_in_network;
extern struct NCDModuleGroup ncdmodule_net_ipv6_addr_in_network;

static struct NCDModuleGroup * const ncd_modules[] = {
    &ncdmodule_var,
    &ncdmodule_list,
    &ncdmodule_depend,
    &ncdmodule_multidepend,
    &ncdmodule_dynamic_depend,
    &ncdmodule_concat,
    &ncdmodule_concatv,
    &ncdmodule_if,
    &ncdmodule_strcmp,
    &ncdmodule_regex_match,
    &ncdmodule_logical,
    &ncdmodule_sleep,
    &ncdmodule_print,
    &ncdmodule_blocker,
    &ncdmodule_run,
    &ncdmodule_runonce,
    &ncdmodule_daemon,
    &ncdmodule_spawn,
    &ncdmodule_call,
    &ncdmodule_imperative,
    &ncdmodule_ref,
    &ncdmodule_index,
    &ncdmodule_alias,
    &ncdmodule_process_manager,
    &ncdmodule_ondemand,
    &ncdmodule_foreach,
    &ncdmodule_choose,
    &ncdmodule_from_string,
    &ncdmodule_to_string,
    &ncdmodule_value,
    &ncdmodule_try,
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
    &ncdmodule_net_ipv4_arp_probe,
    &ncdmodule_net_watch_interfaces,
    &ncdmodule_sys_watch_input,
    &ncdmodule_sys_watch_usb,
#ifdef BADVPN_USE_LINUX_INPUT
    &ncdmodule_sys_evdev,
#endif
#ifdef BADVPN_USE_INOTIFY
    &ncdmodule_sys_watch_directory,
#endif
    &ncdmodule_sys_request_server,
    &ncdmodule_net_ipv6_wait_dynamic_addr,
    &ncdmodule_sys_request_client,
    &ncdmodule_exit,
    &ncdmodule_getargs,
    &ncdmodule_arithmetic,
    &ncdmodule_parse,
    &ncdmodule_valuemetic,
    &ncdmodule_file,
    &ncdmodule_netmask,
    &ncdmodule_implode,
    &ncdmodule_call2,
    &ncdmodule_assert,
    &ncdmodule_reboot,
    &ncdmodule_explode,
    &ncdmodule_net_ipv6_addr,
    &ncdmodule_net_ipv6_route,
    &ncdmodule_net_ipv4_addr_in_network,
    &ncdmodule_net_ipv6_addr_in_network,
    NULL
};

#endif
