/**
 * @file multidepend.c
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
 * Multiple-option dependencies module.
 * 
 * Synopsis: multiprovide(string name)
 * Arguments:
 *   name - provider identifier
 * 
 * Synopsis: multidepend(list(string) names)
 * Arguments:
 *   names - list of provider identifiers. The dependency is satisfied by any
 *     provide statement with a provider identifier contained in this list.
 *     The order of provider identifiers in the list specifies priority
 *     (higher priority first).
 * Variables: Provides variables available from the corresponding provide,
 *     ("modname.varname" or "modname").
 */

#include <stdlib.h>

#include <misc/offset.h>
#include <structure/LinkedList2.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_multidepend.h>

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
    NCDValue *names;
    LinkedList2Node depends_node;
    struct provide *provide;
    LinkedList2Node provide_node;
    int provide_collapsing;
};

LinkedList2 provides;
LinkedList2 depends;

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

static struct provide * depend_find_best_provide (struct depend *o)
{
    NCDValue *e = NCDValue_ListFirst(o->names);
    while (e) {
        struct provide *p = find_provide(NCDValue_StringValue(e));
        if (p && !p->dying) {
            return p;
        }
        e = NCDValue_ListNext(o->names, e);
    }
    
    return NULL;
}

static void depend_update (struct depend *o)
{
    // if we're collapsing, do nothing
    if (o->provide && o->provide_collapsing) {
        return;
    }
    
    // find best provide
    struct provide *bp = depend_find_best_provide(o);
    
    // has anything changed?
    if (bp == o->provide) {
        return;
    }
    
    // if we have an existing provide, start collpsing
    if (o->provide) {
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_DOWN);
        o->provide_collapsing = 1;
        return;
    }
    
    if (bp) {
        // insert to provide's list
        LinkedList2_Append(&bp->depends, &o->provide_node);
        
        // set not collapsing
        o->provide_collapsing = 0;
        
        // set provide
        o->provide = bp;
        
        // signal up
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    }
}

static int func_globalinit (struct NCDModuleInitParams params)
{
    // init provides list
    LinkedList2_Init(&provides);
    
    // init depends list
    LinkedList2_Init(&depends);
    
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
    
    // update depends
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &depends);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        struct depend *d = UPPER_OBJECT(n, struct depend, depends_node);
        depend_update(d);
    }
    
    // signal up
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
    
    // start collapsing our depends
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &o->depends);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        struct depend *d = UPPER_OBJECT(n, struct depend, provide_node);
        ASSERT(d->provide == o)
        depend_update(d);
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
    NCDValue *names_arg;
    if (!NCDValue_ListRead(o->i->args, 1, &names_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(names_arg) != NCDVALUE_LIST) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->names = names_arg;
    
    // check names list
    NCDValue *e = NCDValue_ListFirst(o->names);
    while (e) {
        if (NCDValue_Type(e) != NCDVALUE_STRING) {
            ModuleLog(o->i, BLOG_ERROR, "wrong type");
            goto fail1;
        }
        e = NCDValue_ListNext(o->names, e);
    }
    
    // insert to depends list
    LinkedList2_Append(&depends, &o->depends_node);
    
    // set no provide
    o->provide = NULL;
    
    // update
    depend_update(o);
    
    return o;
    
fail1:
    free(o);
fail0:
    return NULL;
}

static void depend_func_free (void *vo)
{
    struct depend *o = vo;
    
    if (o->provide) {
        // remove from provide's list
        LinkedList2_Remove(&o->provide->depends, &o->provide_node);
        
        // if provide is dying and is empty, let it die
        if (o->provide->dying && LinkedList2_IsEmpty(&o->provide->depends)) {
            NCDModuleInst_Backend_Died(o->provide->i, 0);
        }
    }
    
    // remove from depends list
    LinkedList2_Remove(&depends, &o->depends_node);
    
    // free instance
    free(o);
}

static void depend_func_clean (void *vo)
{
    struct depend *o = vo;
    
    if (!(o->provide && o->provide_collapsing)) {
        return;
    }
    
    // remove from provide's list
    LinkedList2_Remove(&o->provide->depends, &o->provide_node);
    
    // if provide is dying and is empty, let it die
    if (o->provide->dying && LinkedList2_IsEmpty(&o->provide->depends)) {
        NCDModuleInst_Backend_Died(o->provide->i, 0);
    }
    
    // set no provide
    o->provide = NULL;
    
    // update
    depend_update(o);
}

static int depend_func_getvar (void *vo, const char *name_orig, NCDValue *out)
{
    struct depend *o = vo;
    ASSERT(o->provide)
    ASSERT(!o->provide_collapsing)
    
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
    
    ret = NCDModuleInst_Backend_GetVar(o->provide->i, modname, varname, out);
    
    free(name);
fail0:
    return ret;
}

static const struct NCDModule modules[] = {
    {
        .type = "multiprovide",
        .func_new = provide_func_new,
        .func_free = provide_func_free,
        .func_die = provide_func_die
    }, {
        .type = "multidepend",
        .func_new = depend_func_new,
        .func_free = depend_func_free,
        .func_clean = depend_func_clean,
        .func_getvar = depend_func_getvar
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_multidepend = {
    .func_globalinit = func_globalinit,
    .modules = modules
};
