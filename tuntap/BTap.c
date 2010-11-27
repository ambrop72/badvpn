/**
 * @file BTap.c
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

#include <string.h>
#include <stdio.h>

#ifdef BADVPN_USE_WINAPI
#include <windows.h>
#include <winioctl.h>
#include <objbase.h>
#include <wtypes.h>
#include "wintap-common.h"
#include <tuntap/tapwin32-funcs.h>
#else
#include <linux/if_tun.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

#include <tuntap/BTap.h>

static void report_error (BTap *o);
static void input_handler_send (BTap *o, uint8_t *data, int data_len);
static void input_handler_cancel (BTap *o);
static void output_handler_recv (BTap *o, uint8_t *data);

#ifdef BADVPN_USE_WINAPI

static int try_send (BTap *o, uint8_t *data, int data_len)
{
    // setup overlapped
    memset(&o->input_ol, 0, sizeof(o->input_ol));
    o->input_ol.hEvent = o->input_event;
    
    // attempt write
    if (!WriteFile(o->device, data, data_len, NULL, &o->input_ol)) {
        DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            // write pending
            return 0;
        }
        DEBUG("WARNING: WriteFile failed (%u)", error);
        return 1;
    }
    
    // read result
    DWORD bytes;
    if (!GetOverlappedResult(o->device, &o->input_ol, &bytes, FALSE)) {
        DEBUG("WARNING: GetOverlappedResult failed (%u)", GetLastError());
    }
    else if (bytes != data_len) {
        DEBUG("WARNING: written %d expected %d", (int)bytes, data_len);
    }
    
    // reset event
    ASSERT_FORCE(ResetEvent(o->input_event))
    
    return 1;
}

static int try_recv (BTap *o, uint8_t *data, int *data_len)
{
    // setup overlapped
    memset(&o->output_ol, 0, sizeof(o->output_ol));
    o->output_ol.hEvent = o->output_event;
    
    // attempt read
    if (!ReadFile(o->device, data, o->frame_mtu, NULL, &o->output_ol)) {
        DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            // read pending
            return 0;
        }
        
        DEBUG("ReadFile failed (%u)", error);
        
        // fatal error
        return -1;
    }
    
    // read result
    DWORD bytes;
    if (!GetOverlappedResult(o->device, &o->output_ol, &bytes, FALSE)) {
        DEBUG("GetOverlappedResult (output) failed (%u)", GetLastError());
        
        // fatal error
        return -1;
    }
    
    ASSERT_FORCE(bytes <= o->frame_mtu)
    
    // reset event
    ASSERT_FORCE(ResetEvent(o->output_event))
    
    *data_len = bytes;
    return 1;
}

static void write_handle_handler (BTap *o)
{
    ASSERT(o->input_packet_len >= 0)
    DebugObject_Access(&o->d_obj);
    
    // disable handle event
    BReactor_DisableHandle(o->reactor, &o->input_bhandle);
    
    // read result
    DWORD bytes;
    if (!GetOverlappedResult(o->device, &o->input_ol, &bytes, FALSE)) {
        DEBUG("WARNING: GetOverlappedResult (input) failed (%u)", GetLastError());
    } else if (bytes != o->input_packet_len) {
        DEBUG("WARNING: written %d expected %d", (int)bytes, o->input_packet_len);
    }
    
    // set no input packet
    o->input_packet_len = -1;
    
    // reset event
    ASSERT_FORCE(ResetEvent(o->input_event))
    
    // inform sender we finished the packet
    PacketPassInterface_Done(&o->input);
}

static void read_handle_handler (BTap *o)
{
    ASSERT(o->output_packet)
    DebugObject_Access(&o->d_obj);
    
    int bytes;
    
    // read result
    DWORD dbytes;
    if (!GetOverlappedResult(o->device, &o->output_ol, &dbytes, FALSE)) {
        DWORD error = GetLastError();
        
        DEBUG("GetOverlappedResult (output) failed (%u)", error);
        
        // handle accidental cancelation from input_handler_cancel
        if (error == ERROR_OPERATION_ABORTED) {
            DEBUG("retrying read");
            
            // reset event
            ASSERT_FORCE(ResetEvent(o->output_event))
            
            // try receiving
            int res;
            if ((res = try_recv(o, o->output_packet, &bytes)) < 0) {
                goto fatal_error;
            }
            if (!res) {
                // keep waiting
                return;
            }
            
            goto done;
        }
        
    fatal_error:
        
        // set no output packet (so that BTap_Free doesn't try getting the result again)
        o->output_packet = NULL;
        
        // report fatal error
        report_error(o);
        return;
    }
    
    // reset event
    ASSERT_FORCE(ResetEvent(o->output_event))
    
    bytes = dbytes;
    
done:
    ASSERT_FORCE(bytes <= o->frame_mtu)
    
    // disable handle event
    BReactor_DisableHandle(o->reactor, &o->output_bhandle);
    
    // set no output packet
    o->output_packet = NULL;
    
    // inform receiver we finished the packet
    PacketRecvInterface_Done(&o->output, bytes);
}

#else

static void fd_handler (BTap *o, int events)
{
    DebugObject_Access(&o->d_obj);
    
    DEAD_DECLARE
    
    if (events&BREACTOR_ERROR) {
        DEBUG("WARNING: device fd reports error?");
    }
    
    if (events&BREACTOR_WRITE) do {
        ASSERT(o->input_packet_len >= 0)
        
        int bytes = write(o->fd, o->input_packet, o->input_packet_len);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // retry later
                break;
            }
            // malformed packets will cause errors, ignore them and act like
            // the packet was accepeted
        } else {
            if (bytes != o->input_packet_len) {
                DEBUG("WARNING: written %d expected %d", bytes, o->input_packet_len);
            }
        }
        
        // set no input packet
        o->input_packet_len = -1;
        
        // update events
        o->poll_events &= ~BREACTOR_WRITE;
        BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->poll_events);
        
        // inform sender we finished the packet
        PacketPassInterface_Done(&o->input);
    } while (0);
    
    if (events&BREACTOR_READ) do {
        ASSERT(o->output_packet)
        
        // try reading into the buffer
        int bytes = read(o->fd, o->output_packet, o->frame_mtu);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // retry later
                break;
            }
            // report fatal error
            report_error(o);
            return;
        }
        
        ASSERT_FORCE(bytes <= o->frame_mtu)
        
        // set no output packet
        o->output_packet = NULL;
        
        // update events
        o->poll_events &= ~BREACTOR_READ;
        BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->poll_events);
        
        // inform receiver we finished the packet
        PacketRecvInterface_Done(&o->output, bytes);
    } while (0);
}

#endif

void report_error (BTap *o)
{
    #ifndef NDEBUG
    DEAD_ENTER(o->dead)
    #endif
    
    o->handler_error(o->handler_error_user);
    
    #ifndef NDEBUG
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(o->dead);
    #endif
}

void input_handler_send (BTap *o, uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= o->frame_mtu)
    ASSERT(o->input_packet_len == -1)
    DebugObject_Access(&o->d_obj);
    
    #ifdef BADVPN_USE_WINAPI
    
    if (!try_send(o, data, data_len)) {
        // write pending
        o->input_packet = data;
        o->input_packet_len = data_len;
        BReactor_EnableHandle(o->reactor, &o->input_bhandle);
        return;
    }
    
    #else
    
    int bytes = write(o->fd, data, data_len);
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // retry later in fd_handler
            // remember packet
            o->input_packet = data;
            o->input_packet_len = data_len;
            // update events
            o->poll_events |= BREACTOR_WRITE;
            BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->poll_events);
            return;
        }
        // malformed packets will cause errors, ignore them and act like
        // the packet was accepeted
    } else {
        if (bytes != data_len) {
            DEBUG("WARNING: written %d expected %d", bytes, data_len);
        }
    }
    
    #endif
    
    PacketPassInterface_Done(&o->input);
}

void input_handler_cancel (BTap *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->input_packet_len >= 0)
    
    #ifdef BADVPN_USE_WINAPI
    
    // disable handle event
    BReactor_DisableHandle(o->reactor, &o->input_bhandle);
    
    // cancel I/O on the device
    // this also cancels reading, so handle the aborted error code in read_handle_handler
    ASSERT_FORCE(CancelIo(o->device))
    
    // wait for it
    DWORD bytes;
    if (!GetOverlappedResult(o->device, &o->input_ol, &bytes, TRUE)) {
        DWORD error = GetLastError();
        if (error != ERROR_OPERATION_ABORTED) {
            DEBUG("WARNING: GetOverlappedResult (input) failed (%u)", error);
        }
    } else if (bytes != o->input_packet_len) {
        DEBUG("WARNING: written %d expected %d", (int)bytes, o->input_packet_len);
    }
    
    // reset event
    ASSERT_FORCE(ResetEvent(o->input_event))
    
    #else
    
    // update events
    o->poll_events &= ~BREACTOR_WRITE;
    BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->poll_events);
    
    #endif
    
    // set no input packet
    o->input_packet_len = -1;
}

void output_handler_recv (BTap *o, uint8_t *data)
{
    ASSERT(data)
    ASSERT(!o->output_packet)
    DebugObject_Access(&o->d_obj);
    
    #ifdef BADVPN_USE_WINAPI
    
    int bytes;
    int res;
    if ((res = try_recv(o, data, &bytes)) < 0) {
        // report fatal error
        report_error(o);
        return;
    }
    if (!res) {
        // read pending
        o->output_packet = data;
        BReactor_EnableHandle(o->reactor, &o->output_bhandle);
        return;
    }
    
    #else
    
    // attempt read
    int bytes = read(o->fd, data, o->frame_mtu);
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // retry later in fd_handler
            // remember packet
            o->output_packet = data;
            // update events
            o->poll_events |= BREACTOR_READ;
            BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->poll_events);
            return;
        }
        // report fatal error
        report_error(o);
        return;
    }
    
    #endif
    
    ASSERT_FORCE(bytes <= o->frame_mtu)
    
    PacketRecvInterface_Done(&o->output, bytes);
}

int BTap_Init (BTap *o, BReactor *reactor, char *devname, BTap_handler_error handler_error, void *handler_error_user, int tun)
{
    ASSERT(tun == 0 || tun == 1)
    
    // init arguments
    o->reactor = reactor;
    o->handler_error = handler_error;
    o->handler_error_user = handler_error_user;
    
    #ifdef BADVPN_USE_WINAPI
    
    // parse device specification
    
    if (!devname) {
        DEBUG("no device specification provided");
        return 0;
    }
    
    int devname_len = strlen(devname);
    
    char device_component_id[devname_len + 1];
    char device_name[devname_len + 1];
    uint32_t tun_addrs[3];
    
    if (tun) {
        if (!tapwin32_parse_tun_spec(devname, device_component_id, device_name, tun_addrs)) {
            DEBUG("failed to parse TUN device specification");
            return 0;
        }
    } else {
        if (!tapwin32_parse_tap_spec(devname, device_component_id, device_name)) {
            DEBUG("failed to parse TAP device specification");
            return 0;
        }
    }
    
    // locate device path
    
    char device_path[TAPWIN32_MAX_REG_SIZE];
    
    DEBUG("Looking for TAP-Win32 with component ID %s, name %s", device_component_id, device_name);
    
    if (!tapwin32_find_device(device_component_id, device_name, &device_path)) {
        DEBUG("Could not find device");
        goto fail0;
    }
    
    // open device
    
    DEBUG("Opening device %s", device_path);
    
    o->device = CreateFile(device_path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM|FILE_FLAG_OVERLAPPED, 0);
    if (o->device == INVALID_HANDLE_VALUE) {
        DEBUG("CreateFile failed");
        goto fail0;
    }
    
    // set TUN if needed
    
    DWORD len;
    
    if (tun) {
        if (!DeviceIoControl(o->device, TAP_IOCTL_CONFIG_TUN, tun_addrs, sizeof(tun_addrs), tun_addrs, sizeof(tun_addrs), &len, NULL)) {
            DEBUG("DeviceIoControl(TAP_IOCTL_CONFIG_TUN) failed");
            goto fail1;
        }
    }
    
    // get MTU
    
    if (tun) {
        o->frame_mtu = 65535;
    } else {
        ULONG umtu;
        
        if (!DeviceIoControl(o->device, TAP_IOCTL_GET_MTU, NULL, 0, &umtu, sizeof(umtu), &len, NULL)) {
            DEBUG("DeviceIoControl(TAP_IOCTL_GET_MTU) failed");
            goto fail1;
        }
        
        o->frame_mtu = umtu + BTAP_ETHERNET_HEADER_LENGTH;
    }
    
    // set connected
    
    ULONG upstatus = TRUE;
    if (!DeviceIoControl(o->device, TAP_IOCTL_SET_MEDIA_STATUS, &upstatus, sizeof(upstatus), &upstatus, sizeof(upstatus), &len, NULL)) {
        DEBUG("DeviceIoControl(TAP_IOCTL_SET_MEDIA_STATUS) failed");
        goto fail1;
    }
    
    DEBUG("Device opened");
    
    // init input/output
    
    if (!(o->input_event = CreateEvent(NULL, TRUE, FALSE, NULL))) {
        DEBUG("CreateEvent failed");
        goto fail1;
    }
    
    if (!(o->output_event = CreateEvent(NULL, TRUE, FALSE, NULL))) {
        DEBUG("CreateEvent failed");
        goto fail2;
    }
    
    BHandle_Init(&o->input_bhandle, o->input_event, (BHandle_handler)write_handle_handler, o);
    BHandle_Init(&o->output_bhandle, o->output_event, (BHandle_handler)read_handle_handler, o);
    
    if (!BReactor_AddHandle(o->reactor, &o->input_bhandle)) {
        goto fail3;
    }
    if (!BReactor_AddHandle(o->reactor, &o->output_bhandle)) {
        goto fail4;
    }
    
    goto success;
    
fail4:
    BReactor_RemoveHandle(o->reactor, &o->input_bhandle);
fail3:
    ASSERT_FORCE(CloseHandle(o->output_event))
fail2:
    ASSERT_FORCE(CloseHandle(o->input_event))
fail1:
    ASSERT_FORCE(CloseHandle(o->device))
fail0:
    return 0;
    
    #else
    
    // open device
    
    if ((o->fd = open("/dev/net/tun", O_RDWR)) < 0) {
        DEBUG("error opening device");
        goto fail0;
    }
    
    // configure device
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags |= IFF_NO_PI;
    if (tun) {
        ifr.ifr_flags |= IFF_TUN;
    } else {
        ifr.ifr_flags |= IFF_TAP;
    }
    if (devname) {
        snprintf(ifr.ifr_name, IFNAMSIZ, "%s", devname);
    }
    
    if (ioctl(o->fd, TUNSETIFF, (void *)&ifr) < 0) {
        DEBUG("error configuring device");
        goto fail1;
    }
    
    strcpy(o->devname, ifr.ifr_name);
    
    // get MTU
    if (tun) {
        o->frame_mtu = 65535;
    } else {
        // open dummy socket for ioctls
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            DEBUG("socket failed");
            goto fail1;
        }
        
        memset(&ifr, 0, sizeof(ifr));
        strcpy(ifr.ifr_name, o->devname);
        
        if (ioctl(sock, SIOCGIFMTU, (void *)&ifr) < 0) {
            DEBUG("error getting MTU");
            close(sock);
            goto fail1;
        }
        
        o->frame_mtu = ifr.ifr_mtu + BTAP_ETHERNET_HEADER_LENGTH;
        
        close(sock);
    }
    
    // set non-blocking
    if (fcntl(o->fd, F_SETFL, O_NONBLOCK) < 0) {
        DEBUG("cannot set non-blocking");
        goto fail1;
    }
    
    // init file descriptor object
    BFileDescriptor_Init(&o->bfd, o->fd, (BFileDescriptor_handler)fd_handler, o);
    if (!BReactor_AddFileDescriptor(o->reactor, &o->bfd)) {
        DEBUG("BReactor_AddFileDescriptor failed");
        goto fail1;
    }
    o->poll_events = 0;
    
    goto success;
    
fail1:
    ASSERT_FORCE(close(o->fd) == 0)
fail0:
    return 0;
    
    #endif
    
success:
    // init dead var
    DEAD_INIT(o->dead);
    
    // init input
    PacketPassInterface_Init(&o->input, o->frame_mtu, (PacketPassInterface_handler_send)input_handler_send, o, BReactor_PendingGroup(o->reactor));
    PacketPassInterface_EnableCancel(&o->input, (PacketPassInterface_handler_cancel)input_handler_cancel);
    
    // init output
    PacketRecvInterface_Init(&o->output, o->frame_mtu, (PacketRecvInterface_handler_recv)output_handler_recv, o, BReactor_PendingGroup(o->reactor));
    
    // set no input packet
    o->input_packet_len = -1;
    
    // set no output packet
    o->output_packet = NULL;
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
}

void BTap_Free (BTap *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free output
    PacketRecvInterface_Free(&o->output);
    
    // free input
    PacketPassInterface_Free(&o->input);
    
    // kill dead variable
    DEAD_KILL(o->dead);
    
    #ifdef BADVPN_USE_WINAPI
    
    // wait for pending i/o
    ASSERT_FORCE(CancelIo(o->device))
    DWORD bytes;
    DWORD error;
    if (o->input_packet_len >= 0) {
        if (!GetOverlappedResult(o->device, &o->input_ol, &bytes, TRUE)) {
            error = GetLastError();
            if (error != ERROR_OPERATION_ABORTED) {
                DEBUG("WARNING: GetOverlappedResult (input) failed (%u)", error);
            }
        }
    }
    if (o->output_packet) {
        if (!GetOverlappedResult(o->device, &o->output_ol, &bytes, TRUE)) {
            error = GetLastError();
            if (error != ERROR_OPERATION_ABORTED) {
                DEBUG("WARNING: GetOverlappedResult (output) failed (%u)", error);
            }
        }
    }
    
    // free stuff
    BReactor_RemoveHandle(o->reactor, &o->input_bhandle);
    BReactor_RemoveHandle(o->reactor, &o->output_bhandle);
    ASSERT_FORCE(CloseHandle(o->output_event))
    ASSERT_FORCE(CloseHandle(o->input_event))
    ASSERT_FORCE(CloseHandle(o->device))
    
    #else
    
    // free BFileDescriptor
    BReactor_RemoveFileDescriptor(o->reactor, &o->bfd);
    
    // close file descriptor
    ASSERT_FORCE(close(o->fd) == 0)
    
    #endif
}

int BTap_GetMTU (BTap *o)
{
    DebugObject_Access(&o->d_obj);
    
    return o->frame_mtu;
}

PacketPassInterface * BTap_GetInput (BTap *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->input;
}

PacketRecvInterface * BTap_GetOutput (BTap *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}
