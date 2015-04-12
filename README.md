# badvpn

## NCD programming language
NCD (Network Configuration Daemon) is a daemon and programming/scripting language for configuration of network interfaces and other aspects of the operating system. It implements various functionalities as built-in modules, which may be used from an NCD program wherever and for whatever purpose the user needs them. This modularity makes NCD extremely flexible and extensible. It does a very good job with hotplugging in various forms, like USB network interfaces and link detection for wired devices. New features can be added by implementing statements as C-language modules using a straightforward interface.

## Tun2socks network-layer proxifier
tun2socks is a program that "socksifes" TCP connections at the network layer. It implements a TUN device which accepts all incoming TCP connections (regardless of destination IP), and forwards the connections through a SOCKS server. This allows you to forward all connections through SOCKS, without any need for application support. It can be used, for example, to forward connections through a remote SSH server.

## Peer-to-peer VPN
The VPN part of this project implements a Layer 2 (Ethernet) network between the peers (VPN nodes). The peers connect to a central server which acts as a chat server for them to establish direct connections between each other (data connections). These connections are used for transferring network data (Ethernet frames), and can be secured with a multitude of mechanisms. Notable features are:

## UDP and TCP transport
Converges very quickly after a new peer joins
IGMP snooping to deliver multicasts efficiently (e.g. for IPTV)
Double SSL: if SSL is enabled, not only do peers connect to the server with SSL, but they use an additional layer of SSL when exchanging messages through the server
Features related to the NAT problem:
Can work with multiple layers of NAT (needs configuration)
Local peers inside a NAT can communicate directly
Relaying as a fallback (needs configuration)

# Notes
In this project I host some of my open-source networking software. All of the software is written in C and utilizes a custom-developed framework for event-driven programming. The extensive code sharing is the reason all the software is packaged together. However, it is possible to compile only the required components to avoid extra dependencies.

## Documentation is available on the [wiki] (https://github.com/ambrop72/badvpn/wiki):

-   badvpn - General description of BadVPN and its features.
-   Examples - Quick guide to setting up a working VPN.
-   badvpn_server, badvpn_client - Documentation of individual programs.

## Requirements
NCD only works on Linux. Tun2socks works on Linux and Windows. The P2P VPN works on Linux, Windows and FreeBSD (not tested often).

## Installation
Packages are available for Gentoo Linux (net-misc/badvpn), Arch Linux (AUR package badvpn) and Ubuntu (see Installation). A Windows build is available too. If you want to build BadVPN from source, see INSTALL.

## License
Copyright (c) 2009, Ambroz Bizjak <ambrop7@gmail.com> All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
Neither the name of the author nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
