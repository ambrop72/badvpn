/**
 * @file net_iptables.c
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
 * iptables module.
 * 
 * Note that all iptables commands (in general) must be issued synchronously, or
 * the kernel may randomly report errors if there is another iptables command in progress.
 * To solve this, the NCD process contains a single "iptables lock". All iptables commands
 * exposed here go through that lock.
 * In case you wish to call iptables directly, the lock is exposed via net.iptables.lock().
 * 
 * Synopsis:
 *   net.iptables.append(string table, string chain, string arg1  ...)
 * Description:
 *   init:   iptables -t table -A chain arg1 ...
 *   deinit: iptables -t table -D chain arg1 ...
 * 
 * Synopsis:
 *   net.iptables.policy(string table, string chain, string target, string revert_target)
 * Description:
 *   init:   iptables -t table -P chain target
 *   deinit: iptables -t table -P chain revert_target
 * 
 * Synopsis:
 *   net.iptables.newchain(string chain)
 * Description:
 *   init:   iptables -N chain
 *   deinit: iptables -X chain
 * 
 * Synopsis:
 *   net.iptables.lock()
 * Description:
 *   Use at the beginning of a block of custom iptables commands to make sure
 *   they do not interfere with other iptables commands.
 * 
 * Synopsis:
 *   net.iptables.lock::unlock()
 * Description:
 *   Use at the end of a block of custom iptables commands to make sure
 *   they do not interfere with other iptables commands.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <misc/debug.h>
#include <ncd/BEventLock.h>

#include <ncd/modules/command_template.h>

#include <generated/blog_channel_ncd_net_iptables.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define IPTABLES_PATH "/sbin/iptables"
#define IPTABLES_PATH2 "/usr/sbin/iptables"

static void template_free_func (void *vo, int is_error);

BEventLock iptables_lock;

struct instance {
    NCDModuleInst *i;
    command_template_instance cti;
};

struct unlock_instance;

#define LOCK_STATE_LOCKING 1
#define LOCK_STATE_LOCKED 2
#define LOCK_STATE_UNLOCKED 3
#define LOCK_STATE_RELOCKING 4

struct lock_instance {
    NCDModuleInst *i;
    BEventLockJob lock_job;
    struct unlock_instance *unlock;
    int state;
};

struct unlock_instance {
    NCDModuleInst *i;
    struct lock_instance *lock;
};

static void unlock_free (struct unlock_instance *o);

static const char *find_iptables (NCDModuleInst *i)
{
    if (access(IPTABLES_PATH, X_OK) == 0) {
        return IPTABLES_PATH;
    }
    
    if (access(IPTABLES_PATH2, X_OK) == 0) {
        return IPTABLES_PATH2;
    }
    
    ModuleLog(i, BLOG_ERROR, "failed to find iptables (tried "IPTABLES_PATH" and "IPTABLES_PATH2")");
    return NULL;
}

static int build_append_cmdline (NCDModuleInst *i, int remove, char **exec, CmdLine *cl)
{
    // read arguments
    NCDValue *table_arg;
    NCDValue *chain_arg;
    if (!NCDValue_ListReadHead(i->args, 2, &table_arg, &chain_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (NCDValue_Type(table_arg) != NCDVALUE_STRING || NCDValue_Type(chain_arg) != NCDVALUE_STRING) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    char *table = NCDValue_StringValue(table_arg);
    char *chain = NCDValue_StringValue(chain_arg);
    
    // find iptables
    const char *iptables_path = find_iptables(i);
    if (!iptables_path) {
        goto fail0;
    }
    
    // alloc exec
    if (!(*exec = strdup(iptables_path))) {
        ModuleLog(i, BLOG_ERROR, "strdup failed");
        goto fail0;
    }
    
    // start cmdline
    if (!CmdLine_Init(cl)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Init failed");
        goto fail1;
    }
    
    // add header
    if (!CmdLine_Append(cl, iptables_path) || !CmdLine_Append(cl, "-t") || !CmdLine_Append(cl, table) || !CmdLine_Append(cl, (remove ? "-D" : "-A")) || !CmdLine_Append(cl, chain)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Append failed");
        goto fail2;
    }
    
    // add additional arguments
    NCDValue *arg = NCDValue_ListNext(i->args, chain_arg);
    while (arg) {
        if (NCDValue_Type(arg) != NCDVALUE_STRING) {
            ModuleLog(i, BLOG_ERROR, "wrong type");
            goto fail2;
        }
        
        if (!CmdLine_Append(cl, NCDValue_StringValue(arg))) {
            ModuleLog(i, BLOG_ERROR, "CmdLine_Append failed");
            goto fail2;
        }
        
        arg = NCDValue_ListNext(i->args, arg);
    }
    
    // finish
    if (!CmdLine_Finish(cl)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Finish failed");
        goto fail2;
    }
    
    return 1;
    
fail2:
    CmdLine_Free(cl);
fail1:
    free(*exec);
fail0:
    return 0;
}

static int build_policy_cmdline (NCDModuleInst *i, int remove, char **exec, CmdLine *cl)
{
    // read arguments
    NCDValue *table_arg;
    NCDValue *chain_arg;
    NCDValue *target_arg;
    NCDValue *revert_target_arg;
    if (!NCDValue_ListRead(i->args, 4, &table_arg, &chain_arg, &target_arg, &revert_target_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (NCDValue_Type(table_arg) != NCDVALUE_STRING || NCDValue_Type(chain_arg) != NCDVALUE_STRING ||
        NCDValue_Type(target_arg) != NCDVALUE_STRING || NCDValue_Type(revert_target_arg) != NCDVALUE_STRING
    ) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    char *table = NCDValue_StringValue(table_arg);
    char *chain = NCDValue_StringValue(chain_arg);
    char *target = NCDValue_StringValue(target_arg);
    char *revert_target = NCDValue_StringValue(revert_target_arg);
    
    // find iptables
    const char *iptables_path = find_iptables(i);
    if (!iptables_path) {
        goto fail0;
    }
    
    // alloc exec
    if (!(*exec = strdup(iptables_path))) {
        ModuleLog(i, BLOG_ERROR, "strdup failed");
        goto fail0;
    }
    
    // start cmdline
    if (!CmdLine_Init(cl)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Init failed");
        goto fail1;
    }
    
    // add arguments
    if (!CmdLine_Append(cl, iptables_path) || !CmdLine_Append(cl, "-t") || !CmdLine_Append(cl, table) ||
        !CmdLine_Append(cl, "-P") || !CmdLine_Append(cl, chain) || !CmdLine_Append(cl, (remove ? revert_target : target))) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Append failed");
        goto fail2;
    }
    
    // finish
    if (!CmdLine_Finish(cl)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Finish failed");
        goto fail2;
    }
    
    return 1;
    
fail2:
    CmdLine_Free(cl);
fail1:
    free(*exec);
fail0:
    return 0;
}

static int build_newchain_cmdline (NCDModuleInst *i, int remove, char **exec, CmdLine *cl)
{
    // read arguments
    NCDValue *chain_arg;
    if (!NCDValue_ListRead(i->args, 1, &chain_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (NCDValue_Type(chain_arg) != NCDVALUE_STRING) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    char *chain = NCDValue_StringValue(chain_arg);
    
    // find iptables
    const char *iptables_path = find_iptables(i);
    if (!iptables_path) {
        goto fail0;
    }
    
    // alloc exec
    if (!(*exec = strdup(iptables_path))) {
        ModuleLog(i, BLOG_ERROR, "strdup failed");
        goto fail0;
    }
    
    // start cmdline
    if (!CmdLine_Init(cl)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Init failed");
        goto fail1;
    }
    
    // add arguments
    if (!CmdLine_AppendMulti(cl, 3, iptables_path, (remove ? "-X" : "-N"), chain)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_AppendMulti failed");
        goto fail2;
    }
    
    // finish
    if (!CmdLine_Finish(cl)) {
        ModuleLog(i, BLOG_ERROR, "CmdLine_Finish failed");
        goto fail2;
    }
    
    return 1;
    
fail2:
    CmdLine_Free(cl);
fail1:
    free(*exec);
fail0:
    return 0;
}

static void lock_job_handler (struct lock_instance *o)
{
    ASSERT(o->state == LOCK_STATE_LOCKING || o->state == LOCK_STATE_RELOCKING)
    
    if (o->state == LOCK_STATE_LOCKING) {
        ASSERT(!o->unlock)
        
        // up
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
        
        // set state locked
        o->state = LOCK_STATE_LOCKED;
    }
    else if (o->state == LOCK_STATE_RELOCKING) {
        ASSERT(o->unlock)
        ASSERT(o->unlock->lock == o)
        
        // die unlock
        unlock_free(o->unlock);
        o->unlock = NULL;
        
        // set state locked
        o->state = LOCK_STATE_LOCKED;
    }
}

static int func_globalinit (struct NCDModuleInitParams params)
{
    // init iptables lock
    BEventLock_Init(&iptables_lock, BReactor_PendingGroup(params.reactor));
    
    return 1;
}

static void func_globalfree (void)
{
    // free iptables lock
    BEventLock_Free(&iptables_lock);
}

static void func_new (NCDModuleInst *i, command_template_build_cmdline build_cmdline)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        BLog(BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    command_template_new(&o->cti, i, build_cmdline, template_free_func, o, BLOG_CURRENT_CHANNEL, &iptables_lock);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

void template_free_func (void *vo, int is_error)
{
    struct instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    if (is_error) {
        NCDModuleInst_Backend_SetError(i);
    }
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void append_func_new (NCDModuleInst *i)
{
    func_new(i, build_append_cmdline);
}

static void policy_func_new (NCDModuleInst *i)
{
    func_new(i, build_policy_cmdline);
}

static void newchain_func_new (NCDModuleInst *i)
{
    func_new(i, build_newchain_cmdline);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    
    command_template_die(&o->cti);
}

static void lock_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct lock_instance *o = malloc(sizeof(*o));
    if (!o) {
        BLog(BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // init lock job
    BEventLockJob_Init(&o->lock_job, &iptables_lock, (BEventLock_handler)lock_job_handler, o);
    BEventLockJob_Wait(&o->lock_job);
    
    // set no unlock
    o->unlock = NULL;
    
    // set state locking
    o->state = LOCK_STATE_LOCKING;
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void lock_func_die (void *vo)
{
    struct lock_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    if (o->state == LOCK_STATE_UNLOCKED) {
        ASSERT(o->unlock)
        ASSERT(o->unlock->lock == o)
        o->unlock->lock = NULL;
    }
    else if (o->state == LOCK_STATE_RELOCKING) {
        ASSERT(o->unlock)
        ASSERT(o->unlock->lock == o)
        unlock_free(o->unlock);
    }
    else {
        ASSERT(!o->unlock)
    }
    
    // free lock job
    BEventLockJob_Free(&o->lock_job);
    
    // free instance
    free(o);
    
    // dead
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void unlock_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct unlock_instance *o = malloc(sizeof(*o));
    if (!o) {
        BLog(BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // get lock lock
    struct lock_instance *lock = i->method_object->inst_user;
    
    // make sure lock doesn't already have an unlock
    if (lock->unlock) {
        BLog(BLOG_ERROR, "lock already has an unlock");
        goto fail1;
    }
    
    ASSERT(lock->state == LOCK_STATE_LOCKED)
    
    // set lock
    o->lock = lock;
    
    // set unlock in lock
    lock->unlock = o;
    
    // up
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    // release lock
    BEventLockJob_Release(&lock->lock_job);
    
    // set lock state unlocked
    lock->state = LOCK_STATE_UNLOCKED;
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void unlock_func_die (void *vo)
{
    struct unlock_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // if lock is gone, die right away
    if (!o->lock) {
        unlock_free(o);
        return;
    }
    
    ASSERT(o->lock->unlock == o)
    ASSERT(o->lock->state == LOCK_STATE_UNLOCKED)
    
    // wait lock
    BEventLockJob_Wait(&o->lock->lock_job);
    
    // set lock state relocking
    o->lock->state = LOCK_STATE_RELOCKING;
}

static void unlock_free (struct unlock_instance *o)
{
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static const struct NCDModule modules[] = {
    {
        .type = "net.iptables.append",
        .func_new = append_func_new,
        .func_die = func_die
    }, {
        .type = "net.iptables.policy",
        .func_new = policy_func_new,
        .func_die = func_die
    }, {
        .type = "net.iptables.newchain",
        .func_new = newchain_func_new,
        .func_die = func_die
    }, {
        .type = "net.iptables.lock",
        .func_new = lock_func_new,
        .func_die = lock_func_die
    }, {
        .type = "net.iptables.lock::unlock",
        .func_new = unlock_func_new,
        .func_die = unlock_func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_iptables = {
    .modules = modules,
    .func_globalinit = func_globalinit,
    .func_globalfree = func_globalfree
};
