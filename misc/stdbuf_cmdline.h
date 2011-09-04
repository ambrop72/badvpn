/**
 * @file stdbuf_cmdline.h
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
 * Builds command line for running a program via stdbuf.
 */

#ifndef BADVPN_STDBUF_CMDLINE_H
#define BADVPN_STDBUF_CMDLINE_H

#include <misc/debug.h>
#include <misc/cmdline.h>
#include <misc/concat_strings.h>

#define STDBUF_EXEC "/usr/bin/stdbuf"

/**
 * Builds the initial part of command line for calling a program via stdbuf
 * with standard output buffering set to line-buffered.
 * 
 * @param out {@link CmdLine} to append the result to. Note than on failure, only
 *            some part of the cmdline may have been appended.
 * @param exec path to the executable
 * @return 1 on success, 0 on failure
 */
static int build_stdbuf_cmdline (CmdLine *out, const char *exec) WARN_UNUSED;

int build_stdbuf_cmdline (CmdLine *out, const char *exec)
{
    if (!CmdLine_AppendMulti(out, 3, STDBUF_EXEC, "-o", "L")) {
        goto fail1;
    }
    
    if (exec[0] == '/') {
        if (!CmdLine_Append(out, exec)) {
            goto fail1;
        }
    } else {
        char *real_exec = concat_strings(2, "./", exec);
        if (!real_exec) {
            goto fail1;
        }
        int res = CmdLine_Append(out, real_exec);
        free(real_exec);
        if (!res) {
            goto fail1;
        }
    }
    
    return 1;
    
fail1:
    return 0;
}

#endif
