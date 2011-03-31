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
#include <string.h>

#include <misc/offset.h>
#include <misc/debug.h>
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

static void provide_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct provide *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
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
    
    // signal up.
    // This comes above the loop which follows, so that effects on related depend statements are
    // computed before this process advances, avoiding problems like failed variable resolutions.
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
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
        
        // insert to provide's list
        LinkedList2_Append(&o->depends, &d->node);
        
        // set provide
        d->p = o;
        
        // signal up
        NCDModuleInst_Backend_Event(d->i, NCDMODULE_EVENT_UP);
    }
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void provide_free (struct provide *o)
{
    ASSERT(LinkedList2_IsEmpty(&o->depends))
    NCDModuleInst *i = o->i;
    
    // remove from provides list
    LinkedList2_Remove(&provides, &o->provides_node);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void provide_func_die (void *vo)
{
    struct provide *o = vo;
    ASSERT(!o->dying)
    
    // if we have no depends, die immediately
    if (LinkedList2_IsEmpty(&o->depends)) {
        provide_free(o);
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

static void depend_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct depend *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
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
    struct provide *p = find_provide(o->name);
    
    if (p && !p->dying) {
        // insert to provide's list
        LinkedList2_Append(&p->depends, &o->node);
        
        // set provide
        o->p = p;
        
        // signal up
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    } else {
        // insert to free depends list
        LinkedList2_Append(&free_depends, &o->node);
        
        // set no provide
        o->p = NULL;
    }
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void depend_free (struct depend *o)
{
    NCDModuleInst *i = o->i;
    
    if (o->p) {
        // remove from provide's list
        LinkedList2_Remove(&o->p->depends, &o->node);
        
        // if provide is dying and is empty, let it die
        if (o->p->dying && LinkedList2_IsEmpty(&o->p->depends)) {
            provide_free(o->p);
        }
    } else {
        // remove free depends list
        LinkedList2_Remove(&free_depends, &o->node);
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void depend_func_die (void *vo)
{
    struct depend *o = vo;
    
    depend_free(o);
}

static void depend_func_clean (void *vo)
{
    struct depend *o = vo;
    
    if (!(o->p && o->p->dying)) {
        return;
    }
    
    struct provide *p = o->p;
    
    // remove from provide's list
    LinkedList2_Remove(&p->depends, &o->node);
    
    // insert to free depends list
    LinkedList2_Append(&free_depends, &o->node);
    
    // set no provide
    o->p = NULL;
    
    // if provide is empty, let it die
    if (LinkedList2_IsEmpty(&p->depends)) {
        provide_free(p);
    }
}

static int depend_func_getvar (void *vo, const char *name_orig, NCDValue *out)
{
    struct depend *o = vo;
    ASSERT(o->p)
    ASSERT(!o->p->dying)
    
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
        .func_die = provide_func_die
    }, {
        .type = "depend",
        .func_new = depend_func_new,
        .func_die = depend_func_die,
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
