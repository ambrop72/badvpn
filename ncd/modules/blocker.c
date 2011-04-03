/**
 * @file blocker.c
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
 * Blocker module. Provides a statement that blocks when initialized, and which can be blocked
 * and unblocked from outside.
 * 
 * Synopsis: blocker()
 * Description: provides blocking operations. Initially the blocking state is down (but this statement
 * does not block).
 * 
 * Synopsis: blocker::up()
 * Description: sets the blocking state to up.
 * 
 * Synopsis: blocker::down()
 * Description: sets the blocking state to down.
 * 
 * Synopsis: blocker::use()
 * Description: blocks on the blocker. This module is in up state if and only if the blocking state of
 * the blocker is up. Multiple use statements may be used with the same blocker.
 */

#include <stdlib.h>
#include <string.h>

#include <misc/offset.h>
#include <misc/debug.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_blocker.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    LinkedList2 users;
    int up;
    int dying;
};

struct updown_instance {
    NCDModuleInst *i;
};

struct use_instance {
    NCDModuleInst *i;
    struct instance *blocker;
    LinkedList2Node blocker_node;
};

static void func_new (NCDModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    if (!NCDValue_ListRead(o->i->args, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // init users list
    LinkedList2_Init(&o->users);
    
    // set not up
    o->up = 0;
    
    // set not dying
    o->dying = 0;
    
    // signal up
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void instance_free (struct instance *o)
{
    ASSERT(LinkedList2_IsEmpty(&o->users))
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    ASSERT(!o->dying)
    
    // if we have no users, die right away, else wait for users
    if (LinkedList2_IsEmpty(&o->users)) {
        instance_free(o);
        return;
    }
    
    // set dying
    o->dying = 1;
}

static void updown_func_new_templ (NCDModuleInst *i, int up)
{
    // allocate instance
    struct updown_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    if (!NCDValue_ListRead(o->i->args, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // signal up
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    
    if (mo->up != up) {
        // change up state
        mo->up = up;
        
        // signal users
        LinkedList2Iterator it;
        LinkedList2Iterator_InitForward(&it, &mo->users);
        LinkedList2Node *node;
        while (node = LinkedList2Iterator_Next(&it)) {
            struct use_instance *user = UPPER_OBJECT(node, struct use_instance, blocker_node);
            ASSERT(user->blocker == mo)
            NCDModuleInst_Backend_Event(user->i, up ? NCDMODULE_EVENT_UP : NCDMODULE_EVENT_DOWN);
        }
    }
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void up_func_new (NCDModuleInst *i)
{
    updown_func_new_templ(i, 1);
}

static void down_func_new (NCDModuleInst *i)
{
    updown_func_new_templ(i, 0);
}

static void updown_func_die (void *vo)
{
    struct updown_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void use_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct use_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    if (!NCDValue_ListRead(o->i->args, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // set blocker
    o->blocker = i->method_object->inst_user;
    
    // add to blocker's list
    LinkedList2_Append(&o->blocker->users, &o->blocker_node);
    
    // signal up if needed
    if (o->blocker->up) {
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    }
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void use_func_die (void *vo)
{
    struct use_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // remove from blocker's list
    LinkedList2_Remove(&o->blocker->users, &o->blocker_node);
    
    // make the blocker die if needed
    if (o->blocker->dying && LinkedList2_IsEmpty(&o->blocker->users)) {
        instance_free(o->blocker);
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static const struct NCDModule modules[] = {
    {
        .type = "blocker",
        .func_new = func_new,
        .func_die = func_die
    }, {
        .type = "blocker::up",
        .func_new = up_func_new,
        .func_die = updown_func_die,
    }, {
        .type = "blocker::down",
        .func_new = down_func_new,
        .func_die = updown_func_die,
    }, {
        .type = "blocker::use",
        .func_new = use_func_new,
        .func_die = use_func_die,
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_blocker = {
    .modules = modules
};
