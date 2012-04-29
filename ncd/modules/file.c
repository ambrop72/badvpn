/**
 * @file file.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * @section DESCRIPTION
 * 
 * File I/O module.
 * 
 * Synopsis:
 *   file_read(string filename)
 * 
 * Variables:
 *   string (empty) - file contents
 * 
 * Description:
 *   Reads the contents of a file. Reports an error if something goes wrong.
 *   WARNING: this uses fopen/fread/fclose, blocking the entire interpreter while
 *            the file is being read. For this reason, you should only use this
 *            to read small local files which will be read quickly, and especially
 *            not files on network mounts.
 * 
 * Synopsis:
 *   file_write(string filename, string contents)
 * 
 * Description:
 *   Writes a file, possibly overwriting an existing one. Reports an error if something
 *   goes wrong.
 *   WARNING: this is not an atomic operation; other programs may see the file in an
 *            inconsistent state while it is being written. Similarly, if writing
 *            fails, the file may remain in an inconsistent state indefinitely.
 *            If this is a problem, you should write the new contents to a temporary
 *            file and rename this temporary file to the live file.
 *   WARNING: this uses fopen/fwrite/fclose, blocking the entire interpreter while
 *            the file is being written. For this reason, you should only use this
 *            to write small local files which will be written quickly, and especially
 *            not files on network mounts.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <misc/read_file.h>
#include <misc/write_file.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_file.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct read_instance {
    NCDModuleInst *i;
    uint8_t *file_data;
    size_t file_len;
};

static void read_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct read_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // read arguments
    NCDValue *filename_arg;
    if (!NCDValue_ListRead(i->args, 1, &filename_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDValue_IsStringNoNulls(filename_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    // read file
    if (!read_file(NCDValue_StringValue(filename_arg), &o->file_data, &o->file_len)) {
        ModuleLog(i, BLOG_ERROR, "failed to read file");
        goto fail1;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void read_func_die (void *vo)
{
    struct read_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free data
    free(o->file_data);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int read_func_getvar (void *vo, const char *name, NCDValue *out_value)
{
    struct read_instance *o = vo;
    
    if (!strcmp(name, "")) {
        if (!NCDValue_InitStringBin(out_value, o->file_data, o->file_len)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitStringBin failed");
            return 0;
        }
        return 1;
    }
    
    return 0;
}

static void write_func_new (NCDModuleInst *i)
{
    // read arguments
    NCDValue *filename_arg;
    NCDValue *contents_arg;
    if (!NCDValue_ListRead(i->args, 2, &filename_arg, &contents_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDValue_IsStringNoNulls(filename_arg) || !NCDValue_IsString(contents_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    // write file
    if (!write_file(NCDValue_StringValue(filename_arg), NCDValue_StringValue(contents_arg), NCDValue_StringLength(contents_arg))) {
        ModuleLog(i, BLOG_ERROR, "failed to write file");
        goto fail0;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static const struct NCDModule modules[] = {
    {
        .type = "file_read",
        .func_new = read_func_new,
        .func_die = read_func_die,
        .func_getvar = read_func_getvar
    }, {
        .type = "file_write",
        .func_new = write_func_new
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_file = {
    .modules = modules
};
