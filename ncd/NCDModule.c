/**
 * @file NCDModule.c
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

#include <ncd/NCDModule.h>

#define STATE_DOWN 1
#define STATE_UP 2
#define STATE_DYING 3

static void event_job_handler (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    
    n->handler_event(n->user, n->event_job_event);
    return;
}

static void die_job_handler (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    
    if (!n->m->func_die) {
        NCDModuleInst_Backend_Died(n, 0);
        return;
    }
    
    n->m->func_die(n->inst_user);
    return;
}

int NCDModuleInst_Init (NCDModuleInst *n, const char *name, const struct NCDModule *m, NCDValue *args, const char *logprefix, BReactor *reactor, BProcessManager *manager,
                        NCDModule_handler_event handler_event, NCDModule_handler_died handler_died, void *user)
{
    // init arguments
    n->name = name;
    n->m = m;
    n->args = args;
    n->logprefix = logprefix;
    n->reactor = reactor;
    n->manager = manager;
    n->handler_event = handler_event;
    n->handler_died = handler_died;
    n->user = user;
    
    // init event job
    BPending_Init(&n->event_job, BReactor_PendingGroup(n->reactor), (BPending_handler)event_job_handler, n);
    
    // init die job
    BPending_Init(&n->die_job, BReactor_PendingGroup(n->reactor), (BPending_handler)die_job_handler, n);
    
    // set state
    n->state = STATE_DOWN;
    
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
    BPending_Free(&n->die_job);
    BPending_Free(&n->event_job);
    return 0;
}

void NCDModuleInst_Free (NCDModuleInst *n)
{
    #ifndef NDEBUG
    DEAD_KILL(n->d_dead);
    #endif
    DebugObject_Free(&n->d_obj);
    
    // free backend
    n->m->func_free(n->inst_user);
    
    // free die job
    BPending_Free(&n->die_job);
    
    // free event job
    BPending_Free(&n->event_job);
}

void NCDModuleInst_Die (NCDModuleInst *n)
{
    ASSERT(n->state == STATE_UP || n->state == STATE_DOWN)
    DebugObject_Access(&n->d_obj);
    
    // set state
    n->state = STATE_DYING;
    
    // set job
    BPending_Set(&n->die_job);
}

int NCDModuleInst_GetVar (NCDModuleInst *n, const char *name, NCDValue *out)
{
    ASSERT(n->state == STATE_UP)
    DebugObject_Access(&n->d_obj);
    
    if (!n->m->func_getvar) {
        return 0;
    }
    
    return n->m->func_getvar(n->inst_user, name, out);
}

void NCDModuleInst_Backend_Event (NCDModuleInst *n, int event)
{
    ASSERT(event == NCDMODULE_EVENT_UP || event == NCDMODULE_EVENT_DOWN || event == NCDMODULE_EVENT_DYING)
    ASSERT(!(event == NCDMODULE_EVENT_UP) || n->state == STATE_DOWN)
    ASSERT(!(event == NCDMODULE_EVENT_DOWN) || n->state == STATE_UP)
    ASSERT(!(event == NCDMODULE_EVENT_DYING) || (n->state == STATE_DOWN || n->state == STATE_UP))
    ASSERT(!BPending_IsSet(&n->event_job))
    
    switch (event) {
        case NCDMODULE_EVENT_UP:
            n->state = STATE_UP;
            break;
        case NCDMODULE_EVENT_DOWN:
            n->state = STATE_DOWN;
            break;
        case NCDMODULE_EVENT_DYING:
            n->state = STATE_DYING;
            break;
        default:
            ASSERT(0);
    }
    
    // remember event
    n->event_job_event = event;
    
    // set job
    BPending_Set(&n->event_job);
}

void NCDModuleInst_Backend_Died (NCDModuleInst *n, int is_error)
{
    ASSERT(is_error == 0 || is_error == 1)
    
    #ifndef NDEBUG
    DEAD_ENTER(n->d_dead)
    #endif
    
    n->handler_died(n->user, is_error);
    
    #ifndef NDEBUG
    ASSERT(DEAD_KILLED)
    DEAD_LEAVE(n->d_dead);
    #endif
}

void NCDModuleInst_Backend_Log (NCDModuleInst *n, int channel, int level, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    BLog_Append("%s", n->logprefix);
    BLog_LogToChannelVarArg(channel, level, fmt, vl);
    va_end(vl);
}
