/**
 * @file command_template.h
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
 */

#include <stdlib.h>
#include <string.h>

#include <ncd/modules/command_template.h>

#define STATE_ADDING_LOCK 1
#define STATE_ADDING 2
#define STATE_ADDING_NEED_DELETE 3
#define STATE_DONE 4
#define STATE_DELETING_LOCK 5
#define STATE_DELETING 6

struct instance {
    NCDModuleInst *i;
    command_template_build_cmdline build_cmdline;
    int blog_channel;
    BEventLockJob elock_job;
    int state;
    int have_process;
    BProcess process;
};

static int start_process (struct instance *o, int remove);
static void process_handler (struct instance *o, int normally, uint8_t normally_exit_status);

int start_process (struct instance *o, int remove)
{
    int ret = 0;
    
    // build command line
    char *exec;
    CmdLine cl;
    if (!(o->build_cmdline(o->i, remove, &exec, &cl))) {
        NCDModuleInst_Backend_Log(o->i, o->blog_channel, BLOG_ERROR, "build_cmdline callback failed");
        goto fail0;
    }
    
    // start process
    if (!BProcess_Init(&o->process, o->i->manager, (BProcess_handler)process_handler, o, exec, CmdLine_Get(&cl), NULL)) {
        NCDModuleInst_Backend_Log(o->i, o->blog_channel, BLOG_ERROR, "BProcess_Init failed");
        goto fail1;
    }
    
    ret = 1;
    
fail1:
    CmdLine_Free(&cl);
    free(exec);
fail0:
    return ret;
}

static void lock_handler (struct instance *o)
{
    ASSERT(o->state == STATE_ADDING_LOCK || o->state == STATE_DELETING_LOCK)
    ASSERT(!o->have_process)
    
    NCDModuleInst_Backend_Log(o->i, o->blog_channel, BLOG_ERROR, "GotLock");
    
    int remove = (o->state == STATE_DELETING_LOCK);
    
    // start process
    if (!start_process(o, remove)) {
        NCDModuleInst_Backend_Died(o->i, 1);
        return;
    }
    
    // set have process
    o->have_process = 1;
    
    // set state
    o->state = (remove ? STATE_DELETING : STATE_ADDING);
}

void process_handler (struct instance *o, int normally, uint8_t normally_exit_status)
{
    ASSERT(o->have_process)
    ASSERT(o->state == STATE_ADDING || o->state == STATE_ADDING_NEED_DELETE || o->state == STATE_DELETING)
    
    // release lock
    BEventLockJob_Release(&o->elock_job);
    
    // free process
    BProcess_Free(&o->process);
    
    // set have no process
    o->have_process = 0;
    
    if (!normally || normally_exit_status != 0) {
        NCDModuleInst_Backend_Log(o->i, o->blog_channel, BLOG_ERROR, "command failed");
        
        NCDModuleInst_Backend_Died(o->i, 1);
        return;
    }
    
    switch (o->state) {
        case STATE_ADDING: {
            o->state = STATE_DONE;
            
            // signal up
            NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
        } break;
        
        case STATE_ADDING_NEED_DELETE: {
            // wait for lock
            BEventLockJob_Wait(&o->elock_job);
            
            // set state
            o->state = STATE_DELETING_LOCK;
        } break;
        
        case STATE_DELETING: {
            // finish
            NCDModuleInst_Backend_Died(o->i, 0);
            return;
        } break;
    }
}

void * command_template_new (NCDModuleInst *i, command_template_build_cmdline build_cmdline, int blog_channel, BEventLock *elock)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        NCDModuleInst_Backend_Log(i, blog_channel, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    
    // init arguments
    o->i = i;
    o->build_cmdline = build_cmdline;
    o->blog_channel = blog_channel;
    
    // init lock job
    BEventLockJob_Init(&o->elock_job, elock, (BEventLock_handler)lock_handler, o);
    
    // set have no process
    o->have_process = 0;
    
    // wait for lock
    BEventLockJob_Wait(&o->elock_job);
    
    // set state
    o->state = STATE_ADDING_LOCK;
    
    return o;
    
fail0:
    return NULL;
}

void command_template_func_free (void *vo)
{
    struct instance *o = vo;
    
    // free process
    if (o->have_process) {
        // kill process
        BProcess_Kill(&o->process);
        
        // free process
        BProcess_Free(&o->process);
    }
    
    // free lock job
    BEventLockJob_Free(&o->elock_job);
    
    // free instance
    free(o);
}

void command_template_func_die (void *vo)
{
    struct instance *o = vo;
    ASSERT(o->state == STATE_ADDING_LOCK || o->state == STATE_ADDING || o->state == STATE_DONE)
    
    switch (o->state) {
        case STATE_ADDING_LOCK: {
            NCDModuleInst_Backend_Died(o->i, 0);
            return;
        } break;
        
        case STATE_ADDING: {
            ASSERT(o->have_process)
            
            o->state = STATE_ADDING_NEED_DELETE;
        } break;
        
        case STATE_DONE: {
            ASSERT(!o->have_process)
            
            // wait for lock
            BEventLockJob_Wait(&o->elock_job);
            
            // set state
            o->state = STATE_DELETING_LOCK;
        } break;
    }
}
