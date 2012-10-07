/**
 * @file dynamic_depend.c
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
 * Dynamic dependencies module.
 * 
 * Synopsis: dynamic_provide(string name, order_value)
 * Synopsis: dynamic_depend(string name)
 */

#include <stdlib.h>
#include <string.h>

#include <misc/offset.h>
#include <misc/debug.h>
#include <misc/compare.h>
#include <structure/LinkedList0.h>
#include <structure/BAVL.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_dynamic_depend.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct provide;

struct name {
    char *name;
    BAVLNode names_tree_node;
    BAVL provides_tree;
    LinkedList0 waiting_depends_list;
    struct provide *cur_p;
    LinkedList0 cur_bound_depends_list;
    int cur_resetting;
};

struct provide {
    NCDModuleInst *i;
    struct name *n;
    NCDValRef order_value;
    BAVLNode provides_tree_node;
    int dying;
};

struct depend {
    NCDModuleInst *i;
    struct name *n;
    int is_bound;
    LinkedList0Node depends_list_node;
};

static BAVL names_tree;

static void provide_free (struct provide *o);
static void depend_free (struct depend *o);

static int stringptr_comparator (void *user, char **v1, char **v2)
{
    int cmp = strcmp(*v1, *v2);
    return B_COMPARE(cmp, 0);
}

static int val_comparator (void *user, NCDValRef *v1, NCDValRef *v2)
{
    return NCDVal_Compare(*v1, *v2);
}

static struct name * find_name (const char *name)
{
    BAVLNode *tn = BAVL_LookupExact(&names_tree, &name);
    if (!tn) {
        return NULL;
    }
    
    struct name *n = UPPER_OBJECT(tn, struct name, names_tree_node);
    ASSERT(!strcmp(n->name, name))
    
    return n;
}

static struct name * name_init (NCDModuleInst *i, const char *name)
{
    ASSERT(!find_name(name))
    
    // allocate structure
    struct name *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    
    // copy name
    if (!(o->name = strdup(name))) {
        ModuleLog(i, BLOG_ERROR, "strdup failed");
        goto fail1;
    }
    
    // insert to names tree
    ASSERT_EXECUTE(BAVL_Insert(&names_tree, &o->names_tree_node, NULL))
    
    // init provides tree
    BAVL_Init(&o->provides_tree, OFFSET_DIFF(struct provide, order_value, provides_tree_node), (BAVL_comparator)val_comparator, NULL);
    
    // init waiting depends list
    LinkedList0_Init(&o->waiting_depends_list);
    
    // set no current provide
    o->cur_p = NULL;
    
    return o;
    
fail1:
    free(o);
fail0:
    return NULL;
}

static void name_free (struct name *o)
{
    ASSERT(BAVL_IsEmpty(&o->provides_tree))
    ASSERT(LinkedList0_IsEmpty(&o->waiting_depends_list))
    ASSERT(!o->cur_p)
    
    // remove from names tree
    BAVL_Remove(&names_tree, &o->names_tree_node);
    
    // free name
    free(o->name);
    
    // free structure
    free(o);
}

static struct provide * name_get_first_provide (struct name *o)
{
    BAVLNode *tn = BAVL_GetFirst(&o->provides_tree);
    if (!tn) {
        return NULL;
    }
    
    struct provide *p = UPPER_OBJECT(tn, struct provide, provides_tree_node);
    ASSERT(p->n == o)
    
    return p;
}

static void name_new_current (struct name *o)
{
    ASSERT(!o->cur_p)
    ASSERT(!BAVL_IsEmpty(&o->provides_tree))
    
    // set current provide
    o->cur_p = name_get_first_provide(o);
    
    // init bound depends list
    LinkedList0_Init(&o->cur_bound_depends_list);
    
    // set not resetting
    o->cur_resetting = 0;
    
    // bind waiting depends
    while (!LinkedList0_IsEmpty(&o->waiting_depends_list)) {
        struct depend *d = UPPER_OBJECT(LinkedList0_GetFirst(&o->waiting_depends_list), struct depend, depends_list_node);
        ASSERT(d->n == o)
        ASSERT(!d->is_bound)
        
        // remove from waiting depends list
        LinkedList0_Remove(&o->waiting_depends_list, &d->depends_list_node);
        
        // set bound
        d->is_bound = 1;
        
        // add to bound depends list
        LinkedList0_Prepend(&o->cur_bound_depends_list, &d->depends_list_node);
        
        // signal up
        NCDModuleInst_Backend_Up(d->i);
    }
}

static void name_free_if_unused (struct name *o)
{
    if (BAVL_IsEmpty(&o->provides_tree) && LinkedList0_IsEmpty(&o->waiting_depends_list)) {
        name_free(o);
    }
}

static void name_continue_resetting (struct name *o)
{
    ASSERT(o->cur_p)
    ASSERT(o->cur_resetting)
    
    // still have bound depends?
    if (!LinkedList0_IsEmpty(&o->cur_bound_depends_list)) {
        return;
    }
    
    struct provide *old_p = o->cur_p;
    
    // set no current provide
    o->cur_p = NULL;
    
    // free old current provide if it's dying
    if (old_p->dying) {
        provide_free(old_p);
    }
    
    if (!BAVL_IsEmpty(&o->provides_tree)) {
        // get new current provide
        name_new_current(o);
    } else {
        // free name if unused
        name_free_if_unused(o);
    }
}

static void name_start_resetting (struct name *o)
{
    ASSERT(o->cur_p)
    ASSERT(!o->cur_resetting)
    
    // set resetting
    o->cur_resetting = 1;
    
    // signal bound depends down
    for (LinkedList0Node *ln = LinkedList0_GetFirst(&o->cur_bound_depends_list); ln; ln = LinkedList0Node_Next(ln)) {
        struct depend *d = UPPER_OBJECT(ln, struct depend, depends_list_node);
        ASSERT(d->n == o)
        ASSERT(d->is_bound)
        NCDModuleInst_Backend_Down(d->i);
    }
    
    // if there were no bound depends, continue right away
    name_continue_resetting(o);
}

static int func_globalinit (struct NCDModuleInitParams params)
{
    // init names tree
    BAVL_Init(&names_tree, OFFSET_DIFF(struct name, name, names_tree_node), (BAVL_comparator)stringptr_comparator, NULL);
    
    return 1;
}

static void provide_func_new (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    struct provide *o = vo;
    o->i = i;
    
    // read arguments
    NCDValRef name_arg;
    if (!NCDVal_ListRead(params->args, 2, &name_arg, &o->order_value)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsStringNoNulls(name_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    const char *name_str = NCDVal_StringValue(name_arg);
    
    // find name, create new if needed
    struct name *n = find_name(name_str);
    if (!n && !(n = name_init(i, name_str))) {
        goto fail0;
    }
    
    // set name
    o->n = n;
    
    // check for order value conflict
    if (BAVL_LookupExact(&n->provides_tree, &o->order_value)) {
        ModuleLog(i, BLOG_ERROR, "order value already exists");
        goto fail0;
    }
    
    // add to name's provides tree
    ASSERT_EXECUTE(BAVL_Insert(&n->provides_tree, &o->provides_tree_node, NULL))
    
    // set not dying
    o->dying = 0;
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    
    // should this be the current provide?
    if (o == name_get_first_provide(n)) {
        if (!n->cur_p) {
            name_new_current(n);
        }
        else if (!n->cur_resetting) {
            name_start_resetting(n);
        }
    }
    
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void provide_free (struct provide *o)
{
    struct name *n = o->n;
    ASSERT(o->dying)
    ASSERT(o != n->cur_p)
    
    // remove from name's provides tree
    BAVL_Remove(&n->provides_tree, &o->provides_tree_node);
    
    NCDModuleInst_Backend_Dead(o->i);
}

static void provide_func_die (void *vo)
{
    struct provide *o = vo;
    struct name *n = o->n;
    ASSERT(!o->dying)
    
    // set dying
    o->dying = 1;
    
    // if this is not the current provide, die right away
    if (o != n->cur_p) {
        // free provide
        provide_free(o);
        
        // free name if unused
        name_free_if_unused(n);
        return;
    }
    
    ASSERT(!n->cur_resetting)
    
    // start resetting
    name_start_resetting(n);
}

static void depend_func_new (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    struct depend *o = vo;
    o->i = i;
    
    // read arguments
    NCDValRef name_arg;
    if (!NCDVal_ListRead(params->args, 1, &name_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsStringNoNulls(name_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    const char *name_str = NCDVal_StringValue(name_arg);
    
    // find name, create new if needed
    struct name *n = find_name(name_str);
    if (!n && !(n = name_init(i, name_str))) {
        goto fail0;
    }
    
    // set name
    o->n = n;
    
    if (n->cur_p && !n->cur_resetting) {
        // set bound
        o->is_bound = 1;
        
        // add to bound depends list
        LinkedList0_Prepend(&n->cur_bound_depends_list, &o->depends_list_node);
        
        // signal up
        NCDModuleInst_Backend_Up(i);
    } else {
        // set not bound
        o->is_bound = 0;
        
        // add to waiting depends list
        LinkedList0_Prepend(&n->waiting_depends_list, &o->depends_list_node);
    }
    
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void depend_func_die (void *vo)
{
    struct depend *o = vo;
    struct name *n = o->n;
    
    if (o->is_bound) {
        ASSERT(n->cur_p)
        
        // remove from bound depends list
        LinkedList0_Remove(&n->cur_bound_depends_list, &o->depends_list_node);
        
        // continue resetting
        if (n->cur_resetting) {
            name_continue_resetting(n);
        }
    } else {
        // remove from waiting depends list
        LinkedList0_Remove(&n->waiting_depends_list, &o->depends_list_node);
        
        // free name if unused
        name_free_if_unused(n);
    }
    
    NCDModuleInst_Backend_Dead(o->i);
}

static void depend_func_clean (void *vo)
{
    struct depend *o = vo;
    struct name *n = o->n;
    ASSERT(!o->is_bound || n->cur_p)
    
    if (!(o->is_bound && n->cur_resetting)) {
        return;
    }
    
    // remove from bound depends list
    LinkedList0_Remove(&n->cur_bound_depends_list, &o->depends_list_node);
    
    // set not bound
    o->is_bound = 0;
    
    // add to waiting depends list
    LinkedList0_Prepend(&n->waiting_depends_list, &o->depends_list_node);
    
    // continue resetting
    name_continue_resetting(n);
}

static int depend_func_getobj (void *vo, const char *objname, NCDObject *out_object)
{
    struct depend *o = vo;
    struct name *n = o->n;
    ASSERT(!o->is_bound || n->cur_p)
    
    if (!o->is_bound) {
        return 0;
    }
    
    return NCDModuleInst_Backend_GetObj(n->cur_p->i, objname, out_object);
}

static const struct NCDModule modules[] = {
    {
        .type = "dynamic_provide",
        .func_new2 = provide_func_new,
        .func_die = provide_func_die,
        .alloc_size = sizeof(struct provide)
    }, {
        .type = "dynamic_depend",
        .func_new2 = depend_func_new,
        .func_die = depend_func_die,
        .func_clean = depend_func_clean,
        .func_getobj = depend_func_getobj,
        .flags = NCDMODULE_FLAG_CAN_RESOLVE_WHEN_DOWN,
        .alloc_size = sizeof(struct depend)
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_dynamic_depend = {
    .func_globalinit = func_globalinit,
    .modules = modules
};
