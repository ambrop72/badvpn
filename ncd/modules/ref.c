/**
 * @file ref.c
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
 * References module.
 * 
 * Synopsis:
 *   refhere()
 * Variables:
 *   Exposes variables and objects as seen from this refhere() statement.
 * 
 * Synopsis:
 *   ref refhere::ref()
 *   ref ref::ref()
 * Variables:
 *   Exposes variables and objects as seen from the corresponding refhere()
 *   statement.
 */

#include <stdlib.h>
#include <string.h>

#include <misc/offset.h>
#include <structure/LinkedList0.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_ref.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct refhere_instance {
    NCDModuleInst *i;
    LinkedList0 refs_list;
};

struct ref_instance {
    NCDModuleInst *i;
    struct refhere_instance *rh;
    LinkedList0Node refs_list_node;
};

static void ref_instance_free (struct ref_instance *o);

static void refhere_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct refhere_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    if (!NCDValue_ListRead(i->args, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // init refs list
    LinkedList0_Init(&o->refs_list);
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void refhere_func_die (void *vo)
{
    struct refhere_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // die refs
    while (!LinkedList0_IsEmpty(&o->refs_list)) {
        struct ref_instance *ref = UPPER_OBJECT(LinkedList0_GetFirst(&o->refs_list), struct ref_instance, refs_list_node);
        ASSERT(ref->rh == o)
        ref_instance_free(ref);
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static int refhere_func_getvar (void *vo, const char *varname, NCDValue *out)
{
    struct refhere_instance *o = vo;
    
    return NCDModuleInst_Backend_GetVar(o->i, varname, out);
}

static NCDModuleInst * refhere_func_getobj (void *vo, const char *objname)
{
    struct refhere_instance *o = vo;
    
    return NCDModuleInst_Backend_GetObj(o->i, objname);
}

static void ref_func_new_templ (NCDModuleInst *i, struct refhere_instance *rh)
{
    // allocate instance
    struct ref_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    if (!NCDValue_ListRead(i->args, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // set refhere
    o->rh = rh;
    
    // add to refhere's refs list
    LinkedList0_Prepend(&o->rh->refs_list, &o->refs_list_node);
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void ref_func_new_from_refhere (NCDModuleInst *i)
{
    struct refhere_instance *rh = i->method_object->inst_user;
    
    return ref_func_new_templ(i, rh);
}

static void ref_func_new_from_ref (NCDModuleInst *i)
{
    struct ref_instance *ref = i->method_object->inst_user;
    
    return ref_func_new_templ(i, ref->rh);
}

static void ref_instance_free (struct ref_instance *o)
{
    NCDModuleInst *i = o->i;
    
    // remove from refhere's reft list
    LinkedList0_Remove(&o->rh->refs_list, &o->refs_list_node);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void ref_func_die (void *vo)
{
    struct ref_instance *o = vo;
    
    ref_instance_free(o);
}

static int ref_func_getvar (void *vo, const char *varname, NCDValue *out)
{
    struct ref_instance *o = vo;
    
    return NCDModuleInst_Backend_GetVar(o->rh->i, varname, out);
}

static NCDModuleInst * ref_func_getobj (void *vo, const char *objname)
{
    struct ref_instance *o = vo;
    
    return NCDModuleInst_Backend_GetObj(o->rh->i, objname);
}

static const struct NCDModule modules[] = {
    {
        .type = "refhere",
        .func_new = refhere_func_new,
        .func_die = refhere_func_die,
        .func_getvar = refhere_func_getvar,
        .func_getobj = refhere_func_getobj
    }, {
        .type = "refhere::ref",
        .base_type = "ref",
        .func_new = ref_func_new_from_refhere,
        .func_die = ref_func_die,
        .func_getvar = ref_func_getvar,
        .func_getobj = ref_func_getobj
    }, {
        .type = "ref::ref",
        .base_type = "ref",
        .func_new = ref_func_new_from_ref,
        .func_die = ref_func_die,
        .func_getvar = ref_func_getvar,
        .func_getobj = ref_func_getobj
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_ref = {
    .modules = modules
};
