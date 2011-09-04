/**
 * @file open_standard_streams.h
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

#ifndef BADVPN_OPEN_STANDARD_STREAMS_H
#define BADVPN_OPEN_STANDARD_STREAMS_H

#ifndef BADVPN_USE_WINAPI
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

static void open_standard_streams (void)
{
#ifndef BADVPN_USE_WINAPI
    int fd;
    
    do {
        fd = open("/dev/null", O_RDWR);
        if (fd > 2) {
            close(fd);
        }
    } while (fd >= 0 && fd <= 2);
#endif
}

#endif
