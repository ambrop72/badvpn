/**
 * @file blocker.c
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
    NCDModuleInst_Backend_Up(o->i);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void instance_free (struct instance *o)
{
    ASSERT(LinkedList2_IsEmpty(&o->users))
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
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
    NCDModuleInst_Backend_Up(o->i);
    
    // get method object
    struct instance *mo = ((NCDModuleInst *)i->method_user)->inst_user;
    
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
            if (up) {
                NCDModuleInst_Backend_Up(user->i);
            } else {
                NCDModuleInst_Backend_Down(user->i);
            }
        }
    }
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
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
    
    NCDModuleInst_Backend_Dead(i);
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
    o->blocker = ((NCDModuleInst *)i->method_user)->inst_user;
    
    // add to blocker's list
    LinkedList2_Append(&o->blocker->users, &o->blocker_node);
    
    // signal up if needed
    if (o->blocker->up) {
        NCDModuleInst_Backend_Up(o->i);
    }
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
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
    
    NCDModuleInst_Backend_Dead(i);
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
