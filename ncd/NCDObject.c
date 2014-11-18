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

#include "NCDObject.h"

int NCDObject_no_getvar (const NCDObject *obj, NCD_string_id_t name, NCDValMem *mem, NCDValRef *out_value)
{
    return 0;
}

int NCDObject_no_getobj (const NCDObject *obj, NCD_string_id_t name, NCDObject *out_object)
{
    return 0;
}

NCDObject NCDObject_Build (NCD_string_id_t type, void *data_ptr, NCDObject_func_getvar func_getvar, NCDObject_func_getobj func_getobj)
{
    ASSERT(type >= -1)
    ASSERT(func_getvar)
    ASSERT(func_getobj)
    
    NCDObject obj;
    obj.type = type;
    obj.data_int = 0;
    obj.data_ptr = data_ptr;
    obj.method_user = data_ptr;
    obj.func_getvar = func_getvar;
    obj.func_getobj = func_getobj;
    
    return obj;
}

NCDObject NCDObject_BuildFull (NCD_string_id_t type, void *data_ptr, int data_int, void *method_user, NCDObject_func_getvar func_getvar, NCDObject_func_getobj func_getobj)
{
    ASSERT(type >= -1)
    ASSERT(func_getvar)
    ASSERT(func_getobj)
    
    NCDObject obj;
    obj.type = type;
    obj.data_int = data_int;
    obj.data_ptr = data_ptr;
    obj.method_user = method_user;
    obj.func_getvar = func_getvar;
    obj.func_getobj = func_getobj;
    
    return obj;
}

NCD_string_id_t NCDObject_Type (const NCDObject *o)
{
    return o->type;
}

void * NCDObject_DataPtr (const NCDObject *o)
{
    return o->data_ptr;
}

int NCDObject_DataInt (const NCDObject *o)
{
    return o->data_int;
}

void * NCDObject_MethodUser (const NCDObject *o)
{
    return o->method_user;
}

int NCDObject_GetVar (const NCDObject *o, NCD_string_id_t name, NCDValMem *mem, NCDValRef *out_value)
{
    ASSERT(name >= 0)
    ASSERT(mem)
    ASSERT(out_value)
    
    int res = o->func_getvar(o, name, mem, out_value);
    
    ASSERT(res == 0 || res == 1)
    ASSERT(res == 0 || (NCDVal_Assert(*out_value), 1))
    
    return res;
}

int NCDObject_GetObj (const NCDObject *o, NCD_string_id_t name, NCDObject *out_object)
{
    ASSERT(name >= 0)
    ASSERT(out_object)
    
    int res = o->func_getobj(o, name, out_object);
    
    ASSERT(res == 0 || res == 1)
    
    return res;
}

static NCDObject NCDObject__dig_into_object (NCDObject object)
{
    NCDObject obj2;
    while (NCDObject_GetObj(&object, NCD_STRING_EMPTY, &obj2)) {
        object = obj2;
    }
    
    return object;
}

int NCDObject_ResolveVarExprCompact (const NCDObject *o, const NCD_string_id_t *names, size_t num_names, NCDValMem *mem, NCDValRef *out_value)
{
    ASSERT(num_names == 0 || names)
    ASSERT(mem)
    ASSERT(out_value)
    
    NCDObject object = NCDObject__dig_into_object(*o);
    
    while (num_names > 0) {
        NCDObject obj2;
        if (!NCDObject_GetObj(&object, *names, &obj2)) {
            if (num_names == 1 && NCDObject_GetVar(&object, *names, mem, out_value)) {
                return 1;
            }
            
            return 0;
        }
        
        object = NCDObject__dig_into_object(obj2);
        
        names++;
        num_names--;
    }
    
    return NCDObject_GetVar(&object, NCD_STRING_EMPTY, mem, out_value);
}

int NCDObject_ResolveObjExprCompact (const NCDObject *o, const NCD_string_id_t *names, size_t num_names, NCDObject *out_object)
{
    ASSERT(num_names == 0 || names)
    ASSERT(out_object)
    
    NCDObject object = NCDObject__dig_into_object(*o);
    
    while (num_names > 0) {
        NCDObject obj2;
        if (!NCDObject_GetObj(&object, *names, &obj2)) {
            return 0;
        }
        
        object = NCDObject__dig_into_object(obj2);
        
        names++;
        num_names--;
    }
    
    *out_object = object;
    return 1;
}
