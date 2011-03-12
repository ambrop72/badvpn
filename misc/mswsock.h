/**
 * @file mswsock.h
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
 * WinSock MS extensions definitions.
 * 
 * This file exists because MinGW's headers are unpredictable and don't
 * contain everything we need.
 */

#ifndef BADVPN_MISC_MSWSOCK_H
#define BADVPN_MISC_MSWSOCK_H

#include <windows.h>
#include <winsock2.h>

#ifdef _WIN64
#define BADVPN_MAX_NATURAL_ALIGNMENT sizeof(ULONGLONG)
#define BADVPN_MEMORY_ALLOCATION_ALIGNMENT 16
#else
#define BADVPN_MAX_NATURAL_ALIGNMENT sizeof(DWORD)
#define BADVPN_MEMORY_ALLOCATION_ALIGNMENT 8
#endif

#ifdef __cplusplus
#define BADVPN_TYPE_ALIGNMENT(t) __alignof__ (t)
#else
#define BADVPN_TYPE_ALIGNMENT(t) FIELD_OFFSET(struct { char x; t test; },test)
#endif

typedef struct _WSAMSG {
LPSOCKADDR name;
INT namelen;
LPWSABUF lpBuffers;
DWORD dwBufferCount;
WSABUF Control;
DWORD dwFlags;
} WSAMSG,*PWSAMSG,*LPWSAMSG;

typedef struct _WSACMSGHDR {
SIZE_T cmsg_len;
INT cmsg_level;
INT cmsg_type;
} WSACMSGHDR,*PWSACMSGHDR,*LPWSACMSGHDR;

#define WSA_CMSGHDR_ALIGN(length) (((length) + BADVPN_TYPE_ALIGNMENT(WSACMSGHDR)-1) & (~(BADVPN_TYPE_ALIGNMENT(WSACMSGHDR)-1)))
#define WSA_CMSGDATA_ALIGN(length) (((length) + BADVPN_MAX_NATURAL_ALIGNMENT-1) & (~(BADVPN_MAX_NATURAL_ALIGNMENT-1)))
#define WSA_CMSG_FIRSTHDR(msg) (((msg)->Control.len >= sizeof(WSACMSGHDR)) ? (LPWSACMSGHDR)(msg)->Control.buf : (LPWSACMSGHDR)NULL)
#define WSA_CMSG_NXTHDR(msg,cmsg) ((!(cmsg)) ? WSA_CMSG_FIRSTHDR(msg) : ((((u_char *)(cmsg) + WSA_CMSGHDR_ALIGN((cmsg)->cmsg_len) + sizeof(WSACMSGHDR)) > (u_char *)((msg)->Control.buf) + (msg)->Control.len) ? (LPWSACMSGHDR)NULL : (LPWSACMSGHDR)((u_char *)(cmsg) + WSA_CMSGHDR_ALIGN((cmsg)->cmsg_len))))
#define WSA_CMSG_DATA(cmsg) ((u_char *)(cmsg) + WSA_CMSGDATA_ALIGN(sizeof(WSACMSGHDR)))
#define WSA_CMSG_SPACE(length) (WSA_CMSGDATA_ALIGN(sizeof(WSACMSGHDR) + WSA_CMSGHDR_ALIGN(length)))
#define WSA_CMSG_LEN(length) (WSA_CMSGDATA_ALIGN(sizeof(WSACMSGHDR)) + length)

#define WSAID_WSARECVMSG {0xf689d7c8,0x6f1f,0x436b,{0x8a,0x53,0xe5,0x4f,0xe3,0x51,0xc3,0x22}}

typedef INT (WINAPI *LPFN_WSARECVMSG)(SOCKET s, LPWSAMSG lpMsg, LPDWORD lpdwNumberOfBytesRecvd,
    LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

#define WSAID_WSASENDMSG {0xa441e712,0x754f,0x43ca,{0x84,0xa7,0x0d,0xee,0x44,0xcf,0x60,0x6d}}

typedef INT (WINAPI *LPFN_WSASENDMSG)(SOCKET s, LPWSAMSG lpMsg, DWORD dwFlags, LPDWORD lpNumberOfBytesSent,
    LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

#endif
