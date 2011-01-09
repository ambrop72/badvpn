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

static void toevent_job_handler (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    
    switch (n->toevent_job_event) {
        case NCDMODULE_TOEVENT_DIE: {
            ASSERT(n->state == STATE_DYING)
            
            if (!n->m->func_die) {
                NCDModuleInst_Backend_Died(n, 0);
                return;
            }
            
            n->m->func_die(n->inst_user);
            return;
        } break;
        
        case NCDMODULE_TOEVENT_CLEAN: {
            ASSERT(n->state == STATE_UP || n->state == STATE_DOWN)
            
            if (n->m->func_clean) {
                n->m->func_clean(n->inst_user);
                return;
            }
        } break;
        
        default:
            ASSERT(0);
    }
}

int NCDModuleInst_Init (NCDModuleInst *n, const char *name, const struct NCDModule *m, NCDValue *args, const char *logprefix, BReactor *reactor, BProcessManager *manager,
                        NCDModule_handler_event handler_event, NCDModule_handler_died handler_died, NCDModule_handler_getvar handler_getvar, void *user)
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
    n->handler_getvar = handler_getvar;
    n->user = user;
    
    // init event job
    BPending_Init(&n->event_job, BReactor_PendingGroup(n->reactor), (BPending_handler)event_job_handler, n);
    
    // init toevent job
    BPending_Init(&n->toevent_job, BReactor_PendingGroup(n->reactor), (BPending_handler)toevent_job_handler, n);
    
    // set state
    n->state = STATE_DOWN;
    
    // init backend
    if (!(n->inst_user = n->m->func_new(n))) {
        goto fail1;
    }
    
    DebugObject_Init(&n->d_obj);
    DebugError_Init(&n->d_err, BReactor_PendingGroup(n->reactor));
    
    return 1;
    
fail1:
    BPending_Free(&n->toevent_job);
    BPending_Free(&n->event_job);
    return 0;
}

void NCDModuleInst_Free (NCDModuleInst *n)
{
    DebugError_Free(&n->d_err);
    DebugObject_Free(&n->d_obj);
    
    // free backend
    n->m->func_free(n->inst_user);
    
    // free toevent job
    BPending_Free(&n->toevent_job);
    
    // free event job
    BPending_Free(&n->event_job);
}

void NCDModuleInst_Event (NCDModuleInst *n, int event)
{
    ASSERT(event == NCDMODULE_TOEVENT_DIE || event == NCDMODULE_TOEVENT_CLEAN)
    ASSERT(n->state == STATE_UP || n->state == STATE_DOWN)
    ASSERT(!BPending_IsSet(&n->event_job))
    ASSERT(!BPending_IsSet(&n->toevent_job))
    DebugObject_Access(&n->d_obj);
    
    if (event == NCDMODULE_TOEVENT_DIE) {
        // set state
        n->state = STATE_DYING;
    }
    
    // remember event
    n->toevent_job_event = event;
    
    // set job
    BPending_Set(&n->toevent_job);
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
    ASSERT(!BPending_IsSet(&n->toevent_job))
    
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
    
    DEBUGERROR(&n->d_err, n->handler_died(n->user, is_error))
}

int NCDModuleInst_Backend_GetVar (NCDModuleInst *n, const char *modname, const char *varname, NCDValue *out)
{
    ASSERT(modname)
    ASSERT(varname)
    
    int res = n->handler_getvar(n->user, modname, varname, out);
    ASSERT(res == 0 || res == 1)
    
    return res;
}

void NCDModuleInst_Backend_Log (NCDModuleInst *n, int channel, int level, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    BLog_Append("%s", n->logprefix);
    BLog_LogToChannelVarArg(channel, level, fmt, vl);
    va_end(vl);
}
