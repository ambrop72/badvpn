/**
 * @file NCDObject.h
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
 */

#ifndef BADVPN_NCDOBJECT_H
#define BADVPN_NCDOBJECT_H

#include <misc/debug.h>
#include <ncd/NCDValue.h>

typedef struct NCDObject_s NCDObject;

typedef int (*NCDObject_func_getvar) (void *user, const char *name, NCDValue *out_value);
typedef int (*NCDObject_func_getobj) (void *user, const char *name, NCDObject *out_object);
typedef int (*NCDObject_func_getvar2) (void *user, void *user2, const char *name, NCDValue *out_value);
typedef int (*NCDObject_func_getobj2) (void *user, void *user2, const char *name, NCDObject *out_object);

struct NCDObject_s {
    const char *type;
    void *user;
    void *user2;
    union {
        NCDObject_func_getvar func_getvar;
        NCDObject_func_getvar2 func_getvar2;
    } uv;
    union {
        NCDObject_func_getobj func_getobj;
        NCDObject_func_getobj2 func_getobj2;
    } uo;
};

NCDObject NCDObject_Build (const char *type, void *user, NCDObject_func_getvar func_getvar, NCDObject_func_getobj func_getobj);
NCDObject NCDObject_Build2 (const char *type, void *user, void *user2, NCDObject_func_getvar2 func_getvar2, NCDObject_func_getobj2 func_getobj2);
const char * NCDObject_Type (NCDObject *o);
int NCDObject_GetObj (NCDObject *o, const char *name, NCDObject *out_object) WARN_UNUSED;
int NCDObject_GetVar (NCDObject *o, const char *name, NCDValue *out_value) WARN_UNUSED;
int NCDObject_ResolveObjExpr (NCDObject *o, char **names, NCDObject *out_object) WARN_UNUSED;
int NCDObject_ResolveObjExprCompact (NCDObject *o, const char *names, int num_names, NCDObject *out_object) WARN_UNUSED;
int NCDObject_ResolveVarExpr (NCDObject *o, char **names, NCDValue *out_value) WARN_UNUSED;

#endif
