/**
 * @file if.c
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
 * Conditional module.
 * 
 * Synopsis: if(string cond)
 * Description: on initialization, transitions to UP state if cond equals "true", else
 *      remains in the DOWN state indefinitely.
 * 
 * Synopsis: ifnot(string cond)
 * Description: on initialization, transitions to UP state if cond does not equal "true", else
 *      remains in the DOWN state indefinitely.
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_if.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

static void new_templ (NCDModuleInst *i, int not)
{
    // check arguments
    NCDValue *arg;
    if (!NCDValue_ListRead(i->args, 1, &arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (NCDValue_Type(arg) != NCDVALUE_STRING) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    // compute logical value of argument
    int c = NCDValue_StringEquals(arg, "true");
    
    // signal up if needed
    if ((not && !c) || (!not && c)) {
        NCDModuleInst_Backend_Up(i);
    }
    
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new (NCDModuleInst *i)
{
    new_templ(i, 0);
}

static void func_new_not (NCDModuleInst *i)
{
    new_templ(i, 1);
}

static const struct NCDModule modules[] = {
    {
        .type = "if",
        .func_new = func_new
    }, {
        .type = "ifnot",
        .func_new = func_new_not
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_if = {
    .modules = modules
};
