/**
 * @file NCDObject.c
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

#include <stddef.h>

#include "NCDObject.h"

NCDObject NCDObject_Build (const char *type, void *user, NCDObject_func_getvar func_getvar, NCDObject_func_getobj func_getobj)
{
    NCDObject obj;
    obj.type = type;
    obj.user = user;
    obj.user2 = NULL;
    obj.uv.func_getvar = func_getvar;
    obj.uo.func_getobj = func_getobj;
    
    return obj;
}

NCDObject NCDObject_Build2 (const char *type, void *user, void *user2, NCDObject_func_getvar2 func_getvar2, NCDObject_func_getobj2 func_getobj2)
{
    ASSERT(user2)
    
    NCDObject obj;
    obj.type = type;
    obj.user = user;
    obj.user2 = user2;
    obj.uv.func_getvar2 = func_getvar2;
    obj.uo.func_getobj2 = func_getobj2;
    
    return obj;
}

const char * NCDObject_Type (NCDObject *o)
{
    return o->type;
}

int NCDObject_GetObj (NCDObject *o, const char *name, NCDObject *out_object)
{
    ASSERT(name)
    ASSERT(out_object)
    
    int res;
    if (o->user2) {
        res = (!o->uo.func_getobj2 ? 0 : o->uo.func_getobj2(o->user, o->user2, name, out_object));
    } else {
        res = (!o->uo.func_getobj ? 0 : o->uo.func_getobj(o->user, name, out_object));
    }
    
    ASSERT(res == 0 || res == 1)
    
    return res;
}

int NCDObject_GetVar (NCDObject *o, const char *name, NCDValue *out_value)
{
    ASSERT(name)
    ASSERT(out_value)
    
    int res;
    if (o->user2) {
        res = (!o->uv.func_getvar2 ? 0 : o->uv.func_getvar2(o->user, o->user2, name, out_value));
    } else {
        res = (!o->uv.func_getvar ? 0 : o->uv.func_getvar(o->user, name, out_value));
    }
    
    ASSERT(res == 0 || res == 1)
#ifndef NDEBUG
    if (res) {
        NCDValue_Type(out_value);
    }
#endif
    
    return res;
}

static NCDObject dig_into_object (NCDObject object)
{
    NCDObject obj2;
    while (NCDObject_GetObj(&object, "", &obj2)) {
        object = obj2;
    }
    
    return object;
}

int NCDObject_ResolveObjExpr (NCDObject *o, char **names, NCDObject *out_object)
{
    ASSERT(names)
    ASSERT(out_object)
    
    NCDObject object = dig_into_object(*o);
    
    for (size_t i = 0; names[i]; i++) {
        NCDObject obj2;
        if (!NCDObject_GetObj(&object, names[i], &obj2)) {
            return 0;
        }
        
        object = dig_into_object(obj2);
    }
    
    *out_object = object;
    return 1;
}

int NCDObject_ResolveVarExpr (NCDObject *o, char **names, NCDValue *out_value)
{
    ASSERT(names)
    ASSERT(out_value)
    
    NCDObject object = dig_into_object(*o);
    
    for (size_t i = 0; names[i]; i++) {
        NCDObject obj2;
        if (!NCDObject_GetObj(&object, names[i], &obj2)) {
            if (!names[i + 1] && NCDObject_GetVar(&object, names[i], out_value)) {
                return 1;
            }
            
            return 0;
        }
        
        object = dig_into_object(obj2);
    }
    
    return NCDObject_GetVar(&object, "", out_value);
}
