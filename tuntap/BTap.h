/**
 * @file BTap.h
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
 * TAP device abstraction.
 */

#ifndef BADVPN_TUNTAP_BTAP_H
#define BADVPN_TUNTAP_BTAP_H

#if (defined(BADVPN_USE_WINAPI) + defined(BADVPN_LINUX) + defined(BADVPN_FREEBSD)) != 1
#error Unknown TAP backend or too many TAP backends
#endif

#include <stdint.h>

#ifdef BADVPN_USE_WINAPI
#else
#include <net/if.h>
#endif

#include <misc/debug.h>
#include <misc/debugerror.h>
#include <system/DebugObject.h>
#include <system/BReactor.h>
#include <flow/PacketRecvInterface.h>
#include <flow/PacketPassInterface.h>

#define BTAP_ETHERNET_HEADER_LENGTH 14

typedef void (*BTap_handler_error) (void *used);

/**
 * TAP device abstraction.
 * 
 * Frames are written to the device using {@link PacketPassInterface}
 * and read from the device using {@link PacketRecvInterface}.
 */
typedef struct {
    BReactor *reactor;
    BTap_handler_error handler_error;
    void *handler_error_user;
    int frame_mtu;
    PacketPassInterface input;
    PacketRecvInterface output;
    uint8_t *input_packet;
    int input_packet_len;
    uint8_t *output_packet;
    
    #ifdef BADVPN_USE_WINAPI
    HANDLE device;
    HANDLE input_event;
    HANDLE output_event;
    BHandle input_bhandle;
    BHandle output_bhandle;
    OVERLAPPED input_ol;
    OVERLAPPED output_ol;
    #else
    int fd;
    BFileDescriptor bfd;
    char devname[IFNAMSIZ];
    int poll_events;
    #endif
    
    DebugObject d_obj;
    DebugError d_err;
} BTap;

/**
 * Initializes the TAP device.
 *
 * @param o the object
 * @param BReactor {@link BReactor} we live in
 * @param devname name of the devece to open.
 *                On Linux: a network interface name. If it is NULL, no
 *                specific device will be requested, and the operating system
 *                may create a new device.
 *                On Windows: a string "component_id:device_name", where
 *                component_id is a string identifying the driver, and device_name
 *                is the name of the network interface. If component_id is empty,
 *                a hardcoded default will be used instead. If device_name is empty,
 *                the first device found with a matching component_id will be used.
 *                Specifying a NULL devname is equivalent to specifying ":".
 * @param handler_error error handler function
 * @param handler_error_user value passed to error handler
 * @param tun whether to create a TUN (IP) device or a TAP (Ethernet) device. Must be 0 or 1.
 * @return 1 on success, 0 on failure
 */
int BTap_Init (BTap *o, BReactor *bsys, char *devname, BTap_handler_error handler_error, void *handler_error_user, int tun) WARN_UNUSED;

/**
 * Frees the TAP device.
 *
 * @param o the object
 */
void BTap_Free (BTap *o);

/**
 * Returns the device's maximum transmission unit (including any protocol headers).
 *
 * @param o the object
 * @return device's MTU
 */
int BTap_GetMTU (BTap *o);

/**
 * Returns a {@link PacketPassInterface} for writing packets to the device.
 * The MTU of the interface will be {@link BTap_GetMTU}.
 *
 * @param o the object
 * @return input interface
 */
PacketPassInterface * BTap_GetInput (BTap *o);

/**
 * Returns a {@link PacketRecvInterface} for reading packets from the device.
 * The MTU of the interface will be {@link BTap_GetMTU}.
 * The interface will support cancel functionality.
 * 
 * @param o the object
 * @return output interface
 */
PacketRecvInterface * BTap_GetOutput (BTap *o);

#endif
