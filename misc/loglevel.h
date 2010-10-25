/**
 * @file loglevel.h
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
 * Log level specification parsing function.
 */

#ifndef BADVPN_MISC_LOGLEVEL_H
#define BADVPN_MISC_LOGLEVEL_H

#include <string.h>

#include <system/BLog.h>

/**
 * Parses the log level string.
 * 
 * @param str log level string. Recognizes none, error, warning, notice,
 *            info, debug.
 * @return 0 for none, one of BLOG_* for some log level, -1 for unrecognized
 */
static int parse_loglevel (char *str);

int parse_loglevel (char *str)
{
    if (!strcmp(str, "none")) {
        return 0;
    }
    if (!strcmp(str, "error")) {
        return BLOG_ERROR;
    }
    if (!strcmp(str, "warning")) {
        return BLOG_WARNING;
    }
    if (!strcmp(str, "notice")) {
        return BLOG_NOTICE;
    }
    if (!strcmp(str, "info")) {
        return BLOG_INFO;
    }
    if (!strcmp(str, "debug")) {
        return BLOG_DEBUG;
    }
    
    char *endptr;
    int res = strtol(str, &endptr, 10);
    if (*str && !*endptr && res >= 0 && res <= BLOG_DEBUG) {
        return res;
    }
    
    return -1;
}

#endif
