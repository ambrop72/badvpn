/**
 * @file depend.c
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
 * Dependencies module.
 * 
 * Synopsis: provide(string name)
 * 
 * Synopsis: depend(string name)
 * Variables: Provides variables available from the corresponding provide,
 *     ("modname.varname" or "modname").
 */

#include <stdlib.h>

#include <misc/offset.h>
#include <structure/LinkedList2.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_depend.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct provide {
    NCDModuleInst *i;
    char *name;
    LinkedList2Node provides_node;
    LinkedList2 depends;
    int dying;
};

struct depend {
    NCDModuleInst *i;
    char *name;
    struct provide *p;
    LinkedList2Node node;
};

LinkedList2 provides;
LinkedList2 free_depends;

static struct provide * find_provide (const char *name)
{
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &provides);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        struct provide *p = UPPER_OBJECT(n, struct provide, provides_node);
        if (!strcmp(p->name, name)) {
            LinkedList2Iterator_Free(&it);
            return p;
        }
    }
    
    return NULL;
}

static int func_globalinit (struct NCDModuleInitParams params)
{
    // init provides list
    LinkedList2_Init(&provides);
    
    // init free depends list
    LinkedList2_Init(&free_depends);
    
    return 1;
}

static void * provide_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct provide *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    
    // init arguments
    o->i = i;
    
    // read arguments
    NCDValue *name_arg;
    if (!NCDValue_ListRead(o->i->args, 1, &name_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(name_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->name = NCDValue_StringValue(name_arg);
    
    // check for existing provide with this name
    if (find_provide(o->name)) {
        ModuleLog(o->i, BLOG_ERROR, "a provide with this name already exists");
        goto fail1;
    }
    
    // insert to provides list
    LinkedList2_Append(&provides, &o->provides_node);
    
    // init depends list
    LinkedList2_Init(&o->depends);
    
    // set not dying
    o->dying = 0;
    
    // attach free depends with this name
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &free_depends);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        struct depend *d = UPPER_OBJECT(n, struct depend, node);
        ASSERT(!d->p)
        
        if (strcmp(d->name, o->name)) {
            continue;
        }
        
        // remove from free depends list
        LinkedList2_Remove(&free_depends, &d->node);
        
        // set provide
        d->p = o;
        
        // insert to provide's list
        LinkedList2_Append(&o->depends, &d->node);
        
        // signal up
        NCDModuleInst_Backend_Event(d->i, NCDMODULE_EVENT_UP);
    }
    
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    return o;
    
fail1:
    free(o);
fail0:
    return NULL;
}

static void provide_func_free (void *vo)
{
    struct provide *o = vo;
    ASSERT(LinkedList2_IsEmpty(&o->depends))
    
    // remove from provides list
    LinkedList2_Remove(&provides, &o->provides_node);
    
    // free instance
    free(o);
}

static void provide_func_die (void *vo)
{
    struct provide *o = vo;
    ASSERT(!o->dying)
    
    // if we have no depends, die immediately
    if (LinkedList2_IsEmpty(&o->depends)) {
        NCDModuleInst_Backend_Died(o->i, 0);
        return;
    }
    
    // set dying
    o->dying = 1;
    
    // signal our depends down
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &o->depends);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        struct depend *d = UPPER_OBJECT(n, struct depend, node);
        ASSERT(d->p == o)
        
        // signal down
        NCDModuleInst_Backend_Event(d->i, NCDMODULE_EVENT_DOWN);
    }
}

static void * depend_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct depend *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    
    // init arguments
    o->i = i;
    
    // read arguments
    NCDValue *name_arg;
    if (!NCDValue_ListRead(o->i->args, 1, &name_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(name_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->name = NCDValue_StringValue(name_arg);
    
    // find a provide with our name
    o->p = find_provide(o->name);
    
    // do not attach to a dying provide
    if (o->p && o->p->dying) {
        o->p = NULL;
    }
    
    if (o->p) {
        // insert to provide's list
        LinkedList2_Append(&o->p->depends, &o->node);
        
        // signal up
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    } else {
        // insert to free depends list
        LinkedList2_Append(&free_depends, &o->node);
    }
    
    return o;
    
fail1:
    free(o);
fail0:
    return NULL;
}

static void depend_func_free (void *vo)
{
    struct depend *o = vo;
    
    if (o->p) {
        // remove from provide's list
        LinkedList2_Remove(&o->p->depends, &o->node);
        
        // if provide is dying and is empty, let it die
        if (o->p->dying && LinkedList2_IsEmpty(&o->p->depends)) {
            NCDModuleInst_Backend_Died(o->p->i, 0);
        }
    } else {
        // remove free depends list
        LinkedList2_Remove(&free_depends, &o->node);
    }
    
    // free instance
    free(o);
}

static void depend_func_clean (void *vo)
{
    struct depend *o = vo;
    
    if (!(o->p && o->p->dying)) {
        return;
    }
    
    struct provide *p = o->p;
    
    // remove from provide's list
    LinkedList2_Remove(&o->p->depends, &o->node);
    
    // set no provide
    o->p = NULL;
    
    // insert to free depends list
    LinkedList2_Append(&free_depends, &o->node);
    
    // if provide is empty, let it die
    if (LinkedList2_IsEmpty(&p->depends)) {
        NCDModuleInst_Backend_Died(p->i, 0);
    }
}

static int depend_func_getvar (void *vo, const char *name_orig, NCDValue *out)
{
    struct depend *o = vo;
    ASSERT(o->p)
    
    int ret = 0;
    
    char *name = strdup(name_orig);
    if (!name) {
        ModuleLog(o->i, BLOG_ERROR, "strdup failed");
        goto fail0;
    }
    
    const char *modname;
    const char *varname;
    
    char *dot = strstr(name, ".");
    if (!dot) {
        modname = name;
        varname = "";
    } else {
        *dot = '\0';
        modname = name;
        varname = dot + 1;
    }
    
    ret = NCDModuleInst_Backend_GetVar(o->p->i, modname, varname, out);
    
    free(name);
fail0:
    return ret;
}

static const struct NCDModule modules[] = {
    {
        .type = "provide",
        .func_new = provide_func_new,
        .func_free = provide_func_free,
        .func_die = provide_func_die
    }, {
        .type = "depend",
        .func_new = depend_func_new,
        .func_free = depend_func_free,
        .func_clean = depend_func_clean,
        .func_getvar = depend_func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_depend = {
    .func_globalinit = func_globalinit,
    .modules = modules
};
