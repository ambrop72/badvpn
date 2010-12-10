/**
 * @file read_file.h
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
 * Function for reading a file into memory using stdio.
 */

#ifndef BADVPN_MISC_READ_FILE_H
#define BADVPN_MISC_READ_FILE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static int read_file (const char *file, uint8_t **out_data, size_t *out_len)
{
    FILE *f = fopen(file, "r");
    if (!f) {
        goto fail0;
    }
    
    size_t buf_len = 0;
    size_t buf_size = 128;
    
    uint8_t *buf = malloc(buf_size);
    if (!buf) {
        goto fail1;
    }
    
    while (1) {
        if (buf_len == buf_size) {
            if (2 > SIZE_MAX / buf_size) {
                goto fail;
            }
            size_t newsize = 2 * buf_size;
            
            uint8_t *newbuf = realloc(buf, newsize);
            if (!newbuf) {
                goto fail;
            }
            
            buf = newbuf;
            buf_size = newsize;
        }
        
        size_t bytes = fread(buf + buf_len, 1, buf_size - buf_len, f);
        if (bytes == 0) {
            if (feof(f)) {
                break;
            }
            goto fail;
        }
        
        buf_len += bytes;
    }
    
    fclose(f);
    
    *out_data = buf;
    *out_len = buf_len;
    return 1;
    
fail:
    free(buf);
fail1:
    fclose(f);
fail0:
    return 0;
}

#endif
