/**
 * @file BLog.c
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

#include <stdio.h>

#include <system/BLog.h>

struct _BLog_channel blog_channel_list[] = {
#include <generated/blog_channels_list.h>
};

struct _BLog_global blog_global = {
    #ifndef NDEBUG
    .initialized = 0,
    #endif
};

static char *level_names[] = {
    [BLOG_ERROR] = "ERROR",
    [BLOG_WARNING] = "WARNING",
    [BLOG_NOTICE] = "NOTICE",
    [BLOG_INFO] = "INFO",
    [BLOG_DEBUG] = "DEBUG",
};

static void stdout_log (int channel, int level, const char *msg)
{
    printf("%s(%s): %s\n", level_names[level], blog_global.channels[channel].name, msg);
}

static void stdout_free (void)
{
}

void BLog_InitStdout (void)
{
    BLog_Init(stdout_log, stdout_free);
}
