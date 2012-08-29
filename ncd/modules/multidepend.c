/**
 * @file multidepend.c
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
#include <string.h>

#include <misc/offset.h>
#include <misc/debug.h>
#include <structure/LinkedList2.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_multidepend.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct provide {
    NCDModuleInst *i;
    const char *name;
    LinkedList2Node provides_node;
    LinkedList2 depends;
    int dying;
};

struct depend {
    NCDModuleInst *i;
    NCDValRef names;
    LinkedList2Node depends_node;
    struct provide *provide;
    LinkedList2Node provide_node;
    int provide_collapsing;
};

static LinkedList2 provides;
static LinkedList2 depends;

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
    size_t count = NCDVal_ListCount(o->names);
    
    for (size_t j = 0; j < count; j++) {
        NCDValRef e = NCDVal_ListGet(o->names, j);
        struct provide *p = find_provide(NCDVal_StringValue(e));
        if (p && !p->dying) {
            return p;
        }
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
    ASSERT(!bp || !bp->dying)
    
    // has anything changed?
    if (bp == o->provide) {
        return;
    }
    
    if (o->provide) {
        // set collapsing
        o->provide_collapsing = 1;
        
        // signal down
        NCDModuleInst_Backend_Down(o->i);
    } else {
        // insert to provide's list
        LinkedList2_Append(&bp->depends, &o->provide_node);
        
        // set not collapsing
        o->provide_collapsing = 0;
        
        // set provide
        o->provide = bp;
        
        // signal up
        NCDModuleInst_Backend_Up(o->i);
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

static void provide_func_new (void *vo, NCDModuleInst *i)
{
    struct provide *o = vo;
    o->i = i;
    
    // read arguments
    NCDValRef name_arg;
    if (!NCDVal_ListRead(o->i->args, 1, &name_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsStringNoNulls(name_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    o->name = NCDVal_StringValue(name_arg);
    
    // check for existing provide with this name
    if (find_provide(o->name)) {
        ModuleLog(o->i, BLOG_ERROR, "a provide with this name already exists");
        goto fail0;
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
    NCDModuleInst_Backend_Up(o->i);
    
    // update depends
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &depends);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        struct depend *d = UPPER_OBJECT(n, struct depend, depends_node);
        depend_update(d);
    }
    
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void provide_free (struct provide *o)
{
    ASSERT(LinkedList2_IsEmpty(&o->depends))
    
    // remove from provides list
    LinkedList2_Remove(&provides, &o->provides_node);
    
    NCDModuleInst_Backend_Dead(o->i);
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
    
    // start collapsing our depends
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &o->depends);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        struct depend *d = UPPER_OBJECT(n, struct depend, provide_node);
        ASSERT(d->provide == o)
        
        // update depend to make sure it is collapsing
        depend_update(d);
    }
}

static void depend_func_new (void *vo, NCDModuleInst *i)
{
    struct depend *o = vo;
    o->i = i;
    
    // read arguments
    NCDValRef names_arg;
    if (!NCDVal_ListRead(o->i->args, 1, &names_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsList(names_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    o->names = names_arg;
    
    // check names list
    size_t count = NCDVal_ListCount(o->names);
    for (size_t j = 0; j < count; j++) {
        NCDValRef e = NCDVal_ListGet(o->names, j);
        if (!NCDVal_IsStringNoNulls(e)) {
            ModuleLog(o->i, BLOG_ERROR, "wrong type");
            goto fail0;
        }
    }
    
    // insert to depends list
    LinkedList2_Append(&depends, &o->depends_node);
    
    // set no provide
    o->provide = NULL;
    
    // update
    depend_update(o);
    
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void depend_free (struct depend *o)
{
    if (o->provide) {
        // remove from provide's list
        LinkedList2_Remove(&o->provide->depends, &o->provide_node);
        
        // if provide is dying and is empty, let it die
        if (o->provide->dying && LinkedList2_IsEmpty(&o->provide->depends)) {
            provide_free(o->provide);
        }
    }
    
    // remove from depends list
    LinkedList2_Remove(&depends, &o->depends_node);
    
    NCDModuleInst_Backend_Dead(o->i);
}

static void depend_func_die (void *vo)
{
    struct depend *o = vo;
    
    depend_free(o);
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
        provide_free(o->provide);
    }
    
    // set no provide
    o->provide = NULL;
    
    // update
    depend_update(o);
}

static int depend_func_getobj (void *vo, const char *objname, NCDObject *out_object)
{
    struct depend *o = vo;
    
    if (!o->provide) {
        return 0;
    }
    
    return NCDModuleInst_Backend_GetObj(o->provide->i, objname, out_object);
}

static const struct NCDModule modules[] = {
    {
        .type = "multiprovide",
        .func_new2 = provide_func_new,
        .func_die = provide_func_die,
        .alloc_size = sizeof(struct provide)
    }, {
        .type = "multidepend",
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

const struct NCDModuleGroup ncdmodule_multidepend = {
    .func_globalinit = func_globalinit,
    .modules = modules
};
