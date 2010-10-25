/**
 * @file BLog_syslog.c
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
#include <syslog.h>

#include <misc/debug.h>

#include <system/BLog_syslog.h>

static int resolve_facility (char *str, int *out)
{
    if (!strcmp(str, "authpriv")) {
        *out = LOG_AUTHPRIV;
    }
    else if (!strcmp(str, "cron")) {
        *out = LOG_CRON;
    }
    else if (!strcmp(str, "daemon")) {
        *out = LOG_DAEMON;
    }
    else if (!strcmp(str, "ftp")) {
        *out = LOG_FTP;
    }
    else if (!strcmp(str, "local0")) {
        *out = LOG_LOCAL0;
    }
    else if (!strcmp(str, "local1")) {
        *out = LOG_LOCAL1;
    }
    else if (!strcmp(str, "local2")) {
        *out = LOG_LOCAL2;
    }
    else if (!strcmp(str, "local3")) {
        *out = LOG_LOCAL3;
    }
    else if (!strcmp(str, "local4")) {
        *out = LOG_LOCAL4;
    }
    else if (!strcmp(str, "local5")) {
        *out = LOG_LOCAL5;
    }
    else if (!strcmp(str, "local6")) {
        *out = LOG_LOCAL6;
    }
    else if (!strcmp(str, "local7")) {
        *out = LOG_LOCAL7;
    }
    else if (!strcmp(str, "lpr")) {
        *out = LOG_LPR;
    }
    else if (!strcmp(str, "mail")) {
        *out = LOG_MAIL;
    }
    else if (!strcmp(str, "news")) {
        *out = LOG_NEWS;
    }
    else if (!strcmp(str, "syslog")) {
        *out = LOG_SYSLOG;
    }
    else if (!strcmp(str, "user")) {
        *out = LOG_USER;
    }
    else if (!strcmp(str, "uucp")) {
        *out = LOG_UUCP;
    }
    else {
        return 0;
    }
    
    return 1;
}

static int convert_level (int level)
{
    ASSERT(level >= BLOG_ERROR && level <= BLOG_DEBUG)
    
    switch (level) {
        case BLOG_ERROR:
            return LOG_ERR;
        case BLOG_WARNING:
            return LOG_WARNING;
        case BLOG_NOTICE:
            return LOG_NOTICE;
        case BLOG_INFO:
            return LOG_INFO;
        case BLOG_DEBUG:
            return LOG_DEBUG;
        default:
            ASSERT(0)
            return 0;
    }
}

struct {
    char ident[200];
} syslog_global;

static void syslog_log (int channel, int level, const char *msg)
{
    syslog(convert_level(level), "%s: %s", blog_global.channels[channel].name, msg);
}

static void syslog_free (void)
{
    closelog();
}

int BLog_InitSyslog (char *ident, char *facility_str)
{
    int facility;
    if (!resolve_facility(facility_str, &facility)) {
        return 0;
    }
    
    snprintf(syslog_global.ident, sizeof(syslog_global.ident), "%s", ident);
    
    openlog(syslog_global.ident, 0, facility);
    
    BLog_Init(syslog_log, syslog_free);
    
    return 1;
}
