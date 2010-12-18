/**
 * @file NCDInterfaceModule.c
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

#include <ncd/NCDInterfaceModule.h>

static void event_job_handler (NCDInterfaceModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    
    n->handler_event(n->user, (n->up ? NCDINTERFACEMODULE_EVENT_UP : NCDINTERFACEMODULE_EVENT_DOWN));
    return;
}

static void finish_job_handler (NCDInterfaceModuleInst *n)
{
    ASSERT(n->finishing)
    DebugObject_Access(&n->d_obj);
    
    n->m->func_finish(n->inst_user);
    return;
}

int NCDInterfaceModuleInst_Init (
    NCDInterfaceModuleInst *n, const struct NCDInterfaceModule *m, BReactor *reactor, BProcessManager *manager,
    struct NCDConfig_interfaces *conf, NCDInterfaceModule_handler_event handler_event,
    NCDInterfaceModule_handler_error handler_error,
    void *user
)
{
    // init arguments
    n->m = m;
    n->reactor = reactor;
    n->manager = manager;
    n->conf = conf;
    n->handler_event = handler_event;
    n->handler_error = handler_error;
    n->user = user;
    
    // init event job
    BPending_Init(&n->event_job, BReactor_PendingGroup(n->reactor), (BPending_handler)event_job_handler, n);
    
    // init finish job
    BPending_Init(&n->finish_job, BReactor_PendingGroup(n->reactor), (BPending_handler)finish_job_handler, n);
    
    // set not up
    n->up = 0;
    
    // set not finishing
    n->finishing = 0;
    
    // init backend
    if (!(n->inst_user = n->m->func_new(n))) {
        goto fail1;
    }
    
    DebugObject_Init(&n->d_obj);
    #ifndef NDEBUG
    DEAD_INIT(n->d_dead);
    #endif
    
    return 1;
    
fail1:
    BPending_Free(&n->finish_job);
    BPending_Free(&n->event_job);
    return 0;
}

void NCDInterfaceModuleInst_Free (NCDInterfaceModuleInst *n)
{
    DebugObject_Free(&n->d_obj);
    #ifndef NDEBUG
    DEAD_KILL(n->d_dead);
    #endif
    
    // free backend
    n->m->func_free(n->inst_user);
    
    // free finish job
    BPending_Free(&n->finish_job);
    
    // free event job
    BPending_Free(&n->event_job);
}

void NCDInterfaceModuleInst_Finish (NCDInterfaceModuleInst *n)
{
    ASSERT(!n->finishing)
    DebugObject_Access(&n->d_obj);
    
    // set finishing
    n->finishing = 1;
    
    // set job
    BPending_Set(&n->finish_job);
}

void NCDInterfaceModuleInst_Backend_Event (NCDInterfaceModuleInst *n, int event)
{
    ASSERT(event == NCDINTERFACEMODULE_EVENT_UP || event == NCDINTERFACEMODULE_EVENT_DOWN)
    ASSERT((event == NCDINTERFACEMODULE_EVENT_UP) == !n->up)
    ASSERT(!BPending_IsSet(&n->event_job))
    ASSERT(!n->finishing)
    
    // change up state
    n->up = !n->up;
    
    // set job
    BPending_Set(&n->event_job);
}

void NCDInterfaceModuleInst_Backend_Error (NCDInterfaceModuleInst *n)
{
    #ifndef NDEBUG
    DEAD_ENTER(n->d_dead)
    #endif
    
    n->handler_error(n->user);
    
    #ifndef NDEBUG
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(n->d_dead);
    #endif
}

void NCDInterfaceModuleInst_Backend_Log (NCDInterfaceModuleInst *n, int level, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    BLog_Append("interface %s: module: ", n->conf->name);
    BLog_LogToChannelVarArg(BLOG_CURRENT_CHANNEL, level, fmt, vl);
    va_end(vl);
}
