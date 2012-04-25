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
 *              does not block). On deinitialization, waits for all corresponding use() statements
 *              to die before dying itself.
 * 
 * Synopsis: blocker::up()
 * Description: sets the blocking state to up.
 *              The immediate effects of corresponding use() statements going up are processed before
 *              this statement goes up; but this statement statement still goes up immediately,
 *              assuming the effects mentioned haven't resulted in the intepreter scheduling this
 *              very statement for destruction.
 * 
 * Synopsis: blocker::down()
 * Description: sets the blocking state to down.
 *              The immediate effects of corresponding use() statements going up are processed before
 *              this statement goes up; but this statement statement still goes up immediately,
 *              assuming the effects mentioned haven't resulted in the intepreter scheduling this
 *              very statement for destruction.
 * 
 * Synopsis: blocker::downup()
 * Description: atomically sets the blocker to down state (if it was up), then (back) to up state.
 *              Note that this is not equivalent to calling down() and immediately up(); in that case,
 *              the interpreter will first handle the immediate effects of any use() statements
 *              going down as a result of having called down() and will only later execute the up()
 *              statement. In fact, it is possible that the effects of down() will prevent up() from
 *              executing, which may leave the program in an undesirable state.
 * 
 * Synopsis: blocker::rdownup()
 * Description: on deinitialization, atomically sets the blocker to down state (if it was up), then
 *              (back) to up state.
 *              The immediate effects of corresponding use() statements changing state are processed
 *              *after* the immediate effects of this statement dying (in contrast to downup()).
 * 
 * Synopsis: blocker::use()
 * Description: blocks on the blocker. This module is in up state if and only if the blocking state of
 * the blocker is up. Multiple use statements may be used with the same blocker.
 */

#include <stdlib.h>
#include <string.h>

#include <misc/offset.h>
#include <misc/debug.h>
#include <structure/LinkedList2.h>
#include <structure/LinkedList0.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_blocker.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    LinkedList2 users;
    LinkedList0 rdownups_list;
    int up;
    int dying;
};

struct rdownup_instance {
    NCDModuleInst *i;
    struct instance *blocker;
    LinkedList0Node rdownups_list_node;
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
    
    // init rdownups list
    LinkedList0_Init(&o->rdownups_list);
    
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
    
    // break any rdownups
    LinkedList0Node *ln;
    while (ln = LinkedList0_GetFirst(&o->rdownups_list)) {
        struct rdownup_instance *rdu = UPPER_OBJECT(ln, struct rdownup_instance, rdownups_list_node);
        ASSERT(rdu->blocker == o)
        LinkedList0_Remove(&o->rdownups_list, &rdu->rdownups_list_node);
        rdu->blocker = NULL;
    }
    
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

static void updown_func_new_templ (NCDModuleInst *i, int up, int first_down)
{
    ASSERT(!first_down || up)
    
    // check arguments
    if (!NCDValue_ListRead(i->args, 0)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    
    // get method object
    struct instance *mo = NCDModuleInst_Backend_GetUser((NCDModuleInst *)i->method_user);
    
    if (first_down || mo->up != up) {
        // signal users
        LinkedList2Iterator it;
        LinkedList2Iterator_InitForward(&it, &mo->users);
        LinkedList2Node *node;
        while (node = LinkedList2Iterator_Next(&it)) {
            struct use_instance *user = UPPER_OBJECT(node, struct use_instance, blocker_node);
            ASSERT(user->blocker == mo)
            if (first_down && mo->up) {
                NCDModuleInst_Backend_Down(user->i);
            }
            if (up) {
                NCDModuleInst_Backend_Up(user->i);
            } else {
                NCDModuleInst_Backend_Down(user->i);
            }
        }
        
        // change up state
        mo->up = up;
    }
    
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void up_func_new (NCDModuleInst *i)
{
    updown_func_new_templ(i, 1, 0);
}

static void down_func_new (NCDModuleInst *i)
{
    updown_func_new_templ(i, 0, 0);
}

static void downup_func_new (NCDModuleInst *i)
{
    updown_func_new_templ(i, 1, 1);
}

static void rdownup_func_new (NCDModuleInst *i)
{
    // allocate structure
    struct rdownup_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // check arguments
    if (!NCDValue_ListRead(i->args, 0)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // get blocker
    struct instance *blk = NCDModuleInst_Backend_GetUser((NCDModuleInst *)i->method_user);
    
    // set blocker
    o->blocker = blk;
    
    // insert to rdownups list
    LinkedList0_Prepend(&blk->rdownups_list, &o->rdownups_list_node);
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void rdownup_func_die (void *vo)
{
    struct rdownup_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    struct instance *blk = o->blocker;
    
    if (blk) {
        // remove from rdownups list
        LinkedList0_Remove(&blk->rdownups_list, &o->rdownups_list_node);
        
        // downup users
        for (LinkedList2Node *ln = LinkedList2_GetFirst(&blk->users); ln; ln = LinkedList2Node_Next(ln)) {
            struct use_instance *user = UPPER_OBJECT(ln, struct use_instance, blocker_node);
            ASSERT(user->blocker == blk)
            if (blk->up) {
                NCDModuleInst_Backend_Down(user->i);
            }
            NCDModuleInst_Backend_Up(user->i);
        }
        
        // set up
        blk->up = 1;
    }
    
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
    o->blocker = NCDModuleInst_Backend_GetUser((NCDModuleInst *)i->method_user);
    
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
        .func_new = up_func_new
    }, {
        .type = "blocker::down",
        .func_new = down_func_new
    }, {
        .type = "blocker::downup",
        .func_new = downup_func_new
    }, {
        .type = "blocker::rdownup",
        .func_new = rdownup_func_new,
        .func_die = rdownup_func_die
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
