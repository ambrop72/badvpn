/**
 * @file BDatagram.h
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

#ifndef BADVPN_SYSTEM_BDATAGRAM
#define BADVPN_SYSTEM_BDATAGRAM

#include <misc/debug.h>
#include <flow/PacketPassInterface.h>
#include <flow/PacketRecvInterface.h>
#include <system/BAddr.h>
#include <system/BReactor.h>
#include <system/BNetwork.h>

struct BDatagram_s;
typedef struct BDatagram_s BDatagram;

#define BDATAGRAM_EVENT_ERROR 1

typedef void (*BDatagram_handler) (void *user, int event);

int BDatagram_AddressFamilySupported (int family);

int BDatagram_Init (BDatagram *o, int family, BReactor *reactor, void *user,
                    BDatagram_handler handler) WARN_UNUSED;
void BDatagram_Free (BDatagram *o);
int BDatagram_Bind (BDatagram *o, BAddr addr) WARN_UNUSED;
void BDatagram_SetSendAddrs (BDatagram *o, BAddr remote_addr, BIPAddr local_addr);
int BDatagram_GetLastReceiveAddrs (BDatagram *o, BAddr *remote_addr, BIPAddr *local_addr) WARN_UNUSED;
#ifndef BADVPN_USE_WINAPI
int BDatagram_GetFd (BDatagram *o);
#endif

void BDatagram_SendAsync_Init (BDatagram *o, int mtu);
void BDatagram_SendAsync_Free (BDatagram *o);
PacketPassInterface * BDatagram_SendAsync_GetIf (BDatagram *o);

void BDatagram_RecvAsync_Init (BDatagram *o, int mtu);
void BDatagram_RecvAsync_Free (BDatagram *o);
PacketRecvInterface * BDatagram_RecvAsync_GetIf (BDatagram *o);

#ifdef BADVPN_USE_WINAPI
#include "BDatagram_win.h"
#else
#include "BDatagram_unix.h"
#endif

#endif
