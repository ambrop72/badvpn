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

#include <stdarg.h>

#include <ncd/NCDModule.h>

#define STATE_INIT 1
#define STATE_UNINIT 2
#define STATE_DEAD 3
#define STATE_DOWN_CLEAN 4
#define STATE_UP 5
#define STATE_DOWN_UNCLEAN 6
#define STATE_DOWN_PCLEAN 7
#define STATE_DOWN_DIE 8
#define STATE_UP_DIE 9
#define STATE_DYING 10
#define STATE_UNDEAD 11

#define PROCESS_STATE_INIT 1
#define PROCESS_STATE_DOWN 2
#define PROCESS_STATE_UP_PENDING 3
#define PROCESS_STATE_UP 4
#define PROCESS_STATE_DOWN_PENDING 5
#define PROCESS_STATE_DOWN_WAITING 6
#define PROCESS_STATE_DOWN_CONTINUE_PENDING 7
#define PROCESS_STATE_TERMINATING 8
#define PROCESS_STATE_TERMINATED_PENDING 9
#define PROCESS_STATE_TERMINATED 10

static void frontend_event (NCDModuleInst *n, int event)
{
    n->func_event(n->user, event);
}

static void init_job_handler (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_INIT)
    
    n->state = STATE_DOWN_CLEAN;
    
    n->m->func_new(n);
    return;
}

static void uninit_job_handler (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_UNINIT)
    
    n->state = STATE_UNDEAD;
    
    frontend_event(n, NCDMODULE_EVENT_DEAD);
    return;
}

static void die_job_handler (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_DOWN_DIE || n->state == STATE_UP_DIE)
    
    n->state = STATE_DYING;
    
    n->m->func_die(n->inst_user);
    return;
}

static void clean_job_handler (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_DOWN_PCLEAN)
    
    n->state = STATE_DOWN_CLEAN;
    
    if (n->m->func_clean) {
        n->m->func_clean(n->inst_user);
        return;
    }
}

static void process_event_job_handler (NCDModuleProcess *o)
{
    DebugObject_Access(&o->d_obj);
    
    switch (o->state) {
        case PROCESS_STATE_DOWN_CONTINUE_PENDING: {
            o->state = PROCESS_STATE_DOWN;
            
            o->interp_func_event(o->interp_user, NCDMODULEPROCESS_INTERP_EVENT_CONTINUE);
        } break;
        
        case PROCESS_STATE_UP_PENDING: {
            o->state = PROCESS_STATE_UP;
            
            o->handler_event(o->user, NCDMODULEPROCESS_EVENT_UP);
            return;
        } break;
        
        case PROCESS_STATE_DOWN_PENDING: {
            o->state = PROCESS_STATE_DOWN_WAITING;
            
            o->handler_event(o->user, NCDMODULEPROCESS_EVENT_DOWN);
            return;
        } break;
        
        case PROCESS_STATE_TERMINATED_PENDING: {
            o->state = PROCESS_STATE_TERMINATED;
            
            o->handler_event(o->user, NCDMODULEPROCESS_EVENT_TERMINATED);
            return;
        } break;
        
        default: ASSERT(0);
    }
}

void NCDModuleInst_Init (NCDModuleInst *n, const struct NCDModule *m, NCDModuleInst *method_object, NCDValue *args, BReactor *reactor, BProcessManager *manager, NCDUdevManager *umanager, void *user,
                         NCDModuleInst_func_event func_event,
                         NCDModuleInst_func_getvar func_getvar,
                         NCDModuleInst_func_getobj func_getobj,
                         NCDModuleInst_func_initprocess func_initprocess,
                         BLog_logfunc logfunc)
{
    ASSERT(args)
    ASSERT(NCDValue_Type(args) == NCDVALUE_LIST)
    ASSERT(func_event)
    ASSERT(func_getvar)
    ASSERT(func_getobj)
    ASSERT(func_initprocess)
    ASSERT(logfunc)
    
    // init arguments
    n->m = m;
    n->method_object = method_object;
    n->args = args;
    n->reactor = reactor;
    n->manager = manager;
    n->umanager = umanager;
    n->user = user;
    n->func_event = func_event;
    n->func_getvar = func_getvar;
    n->func_getobj = func_getobj;
    n->func_initprocess = func_initprocess;
    n->logfunc = logfunc;
    
    // init jobs
    BPending_Init(&n->init_job, BReactor_PendingGroup(n->reactor), (BPending_handler)init_job_handler, n);
    BPending_Init(&n->uninit_job, BReactor_PendingGroup(n->reactor), (BPending_handler)uninit_job_handler, n);
    BPending_Init(&n->die_job, BReactor_PendingGroup(n->reactor), (BPending_handler)die_job_handler, n);
    BPending_Init(&n->clean_job, BReactor_PendingGroup(n->reactor), (BPending_handler)clean_job_handler, n);
    
    // set initial state
    n->state = STATE_INIT;
    BPending_Set(&n->init_job);
    
    // set initial instance argument
    n->inst_user = NULL;
    
    // clear error flag
    n->is_error = 0;
    
    DebugObject_Init(&n->d_obj);
}

void NCDModuleInst_Free (NCDModuleInst *n)
{
    DebugObject_Free(&n->d_obj);
    ASSERT(n->state == STATE_DEAD || n->state == STATE_UNDEAD)
    
    // free jobs
    BPending_Free(&n->clean_job);
    BPending_Free(&n->die_job);
    BPending_Free(&n->uninit_job);
    BPending_Free(&n->init_job);
}

void NCDModuleInst_Die (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    
    switch (n->state) {
        case STATE_INIT: {
            n->state = STATE_UNINIT;
            BPending_Unset(&n->init_job);
            BPending_Set(&n->uninit_job);
        } break;
        
        case STATE_DOWN_CLEAN:
        case STATE_DOWN_UNCLEAN: {
            n->state = STATE_DOWN_DIE;
            BPending_Set(&n->die_job);
        } break;
        
        case STATE_DOWN_PCLEAN: {
            n->state = STATE_DOWN_DIE;
            BPending_Unset(&n->clean_job);
            BPending_Set(&n->die_job);
        } break;
        
        case STATE_UP: {
            n->state = STATE_UP_DIE;
            BPending_Set(&n->die_job);
        } break;
        
        default: ASSERT(0);
    }
}

void NCDModuleInst_Clean (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    
    switch (n->state) {
        case STATE_INIT:
        case STATE_DOWN_CLEAN:
        case STATE_DOWN_PCLEAN: {
        } break;
        
        case STATE_DOWN_UNCLEAN: {
            n->state = STATE_DOWN_PCLEAN;
            BPending_Set(&n->clean_job);
        } break;
        
        default: ASSERT(0);
    }
}

int NCDModuleInst_GetVar (NCDModuleInst *n, const char *name, NCDValue *out)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_UP)
    ASSERT(name)
    
    if (!n->m->func_getvar) {
        return 0;
    }
    
    return n->m->func_getvar(n->inst_user, name, out);
}

NCDModuleInst * NCDModuleInst_GetObj (NCDModuleInst *n, const char *name)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_UP)
    ASSERT(name)
    
    if (!n->m->func_getobj) {
        return NULL;
    }
    
    return n->m->func_getobj(n->inst_user, name);
}

int NCDModuleInst_HaveError (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_DEAD || n->state == STATE_UNDEAD)
    
    return n->is_error;
}

void NCDModuleInst_Backend_SetUser (NCDModuleInst *n, void *user)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_DOWN_PCLEAN || n->state == STATE_DOWN_UNCLEAN || n->state == STATE_DOWN_CLEAN ||
           n->state == STATE_UP || n->state == STATE_DOWN_DIE || n->state == STATE_UP_DIE ||
           n->state == STATE_DYING)
    
    n->inst_user = user;
}

void NCDModuleInst_Backend_Up (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    
    switch (n->state) {
        case STATE_DOWN_CLEAN:
        case STATE_DOWN_UNCLEAN: {
            n->state = STATE_UP;
            frontend_event(n, NCDMODULE_EVENT_UP);
        } break;
        
        case STATE_DOWN_PCLEAN: {
            n->state = STATE_UP;
            BPending_Unset(&n->clean_job);
            frontend_event(n, NCDMODULE_EVENT_UP);
        } break;
        
        case STATE_DOWN_DIE: {
            n->state = STATE_UP_DIE;
        } break;
        
        default: ASSERT(0);
    }
}

void NCDModuleInst_Backend_Down (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    
    switch (n->state) {
        case STATE_UP: {
            n->state = STATE_DOWN_UNCLEAN;
            frontend_event(n, NCDMODULE_EVENT_DOWN);
        } break;
        
        case STATE_UP_DIE: {
            n->state = STATE_DOWN_DIE;
        } break;
        
        default: ASSERT(0);
    }
}

void NCDModuleInst_Backend_Dead (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    
    switch (n->state) {
        case STATE_DOWN_DIE:
        case STATE_UP_DIE: {
            n->state = STATE_DEAD;
            BPending_Unset(&n->die_job);
        } break;
        
        case STATE_DOWN_CLEAN:
        case STATE_DOWN_UNCLEAN:
        case STATE_UP:
        case STATE_DYING: {
            n->state = STATE_DEAD;
        } break;
        
        case STATE_DOWN_PCLEAN: {
            n->state = STATE_DEAD;
            BPending_Unset(&n->clean_job);
        } break;
        
        default: ASSERT(0);
    }
    
    frontend_event(n, NCDMODULE_EVENT_DEAD);
    return;
}

int NCDModuleInst_Backend_GetVar (NCDModuleInst *n, const char *name, NCDValue *out)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_DOWN_PCLEAN || n->state == STATE_DOWN_UNCLEAN || n->state == STATE_DOWN_CLEAN ||
           n->state == STATE_UP || n->state == STATE_DOWN_DIE || n->state == STATE_UP_DIE ||
           n->state == STATE_DYING)
    ASSERT(name)
    
    int res = n->func_getvar(n->user, name, out);
    ASSERT(res == 0 || res == 1)
    
    return res;
}

NCDModuleInst * NCDModuleInst_Backend_GetObj (NCDModuleInst *n, const char *name)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_DOWN_PCLEAN || n->state == STATE_DOWN_UNCLEAN || n->state == STATE_DOWN_CLEAN ||
           n->state == STATE_UP || n->state == STATE_DOWN_DIE || n->state == STATE_UP_DIE ||
           n->state == STATE_DYING)
    ASSERT(name)
    
    return n->func_getobj(n->user, name);
}

void NCDModuleInst_Backend_Log (NCDModuleInst *n, int channel, int level, const char *fmt, ...)
{
    DebugObject_Access(&n->d_obj);
    
    va_list vl;
    va_start(vl, fmt);
    BLog_LogViaFuncVarArg(n->logfunc, n->user, channel, level, fmt, vl);
    va_end(vl);
}

void NCDModuleInst_Backend_SetError (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_DOWN_PCLEAN || n->state == STATE_DOWN_UNCLEAN || n->state == STATE_DOWN_CLEAN ||
           n->state == STATE_UP || n->state == STATE_DOWN_DIE || n->state == STATE_UP_DIE ||
           n->state == STATE_DYING)
    ASSERT(!n->is_error)
    
    n->is_error = 1;
}

int NCDModuleProcess_Init (NCDModuleProcess *o, NCDModuleInst *n, const char *template_name, NCDValue args, void *user, NCDModuleProcess_handler_event handler_event)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_DOWN_PCLEAN || n->state == STATE_DOWN_UNCLEAN || n->state == STATE_DOWN_CLEAN ||
           n->state == STATE_UP || n->state == STATE_DOWN_DIE || n->state == STATE_UP_DIE ||
           n->state == STATE_DYING)
    ASSERT(template_name)
    ASSERT(NCDValue_Type(&args) == NCDVALUE_LIST)
    ASSERT(handler_event)
    
    // init arguments
    o->n = n;
    o->user = user;
    o->handler_event = handler_event;
    
    // init event job
    BPending_Init(&o->event_job, BReactor_PendingGroup(n->reactor), (BPending_handler)process_event_job_handler, o);
    
    // set state
    o->state = PROCESS_STATE_INIT;
    
    // clear event func so we can assert it was set
    o->interp_func_event = NULL;
    
    // init interpreter part
    if (!(n->func_initprocess(n->user, o, template_name, args))) {
        goto fail1;
    }
    
    ASSERT(o->interp_func_event)
    
    // set state
    o->state = PROCESS_STATE_DOWN;
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail1:
    BPending_Free(&o->event_job);
    return 0;
}

void NCDModuleProcess_Free (NCDModuleProcess *o)
{
    DebugObject_Free(&o->d_obj);
    ASSERT(o->state == PROCESS_STATE_TERMINATED)
    
    // free event job
    BPending_Free(&o->event_job);
}

void NCDModuleProcess_Continue (NCDModuleProcess *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == PROCESS_STATE_DOWN_WAITING)
    
    o->state = PROCESS_STATE_DOWN;
    
    o->interp_func_event(o->interp_user, NCDMODULEPROCESS_INTERP_EVENT_CONTINUE);
}

void NCDModuleProcess_Terminate (NCDModuleProcess *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == PROCESS_STATE_DOWN || o->state == PROCESS_STATE_UP_PENDING ||
           o->state == PROCESS_STATE_DOWN_CONTINUE_PENDING || o->state == PROCESS_STATE_UP ||
           o->state == PROCESS_STATE_DOWN_PENDING || o->state == PROCESS_STATE_DOWN_WAITING)
    
    BPending_Unset(&o->event_job);
    o->state = PROCESS_STATE_TERMINATING;
    
    o->interp_func_event(o->interp_user, NCDMODULEPROCESS_INTERP_EVENT_TERMINATE);
}

int NCDModuleProcess_GetVar (NCDModuleProcess *o, const char *name, NCDValue *out)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state != PROCESS_STATE_INIT)
    ASSERT(name)
    ASSERT(out)
    
    // interpreter gone?
    if (o->state == PROCESS_STATE_TERMINATED_PENDING || o->state == PROCESS_STATE_TERMINATED) {
        return 0;
    }
    
    int res = o->interp_func_getvar(o->interp_user, name, out);
    ASSERT(res == 0 || res == 1)
    
    return res;
}

NCDModuleInst * NCDModuleProcess_GetObj (NCDModuleProcess *o, const char *name)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state != PROCESS_STATE_INIT)
    ASSERT(name)
    
    // interpreter gone?
    if (o->state == PROCESS_STATE_TERMINATED_PENDING || o->state == PROCESS_STATE_TERMINATED) {
        return NULL;
    }
    
    return o->interp_func_getobj(o->interp_user, name);
}

void NCDModuleProcess_Interp_SetHandlers (NCDModuleProcess *o, void *interp_user,
                                          NCDModuleProcess_interp_func_event interp_func_event,
                                          NCDModuleProcess_interp_func_getvar interp_func_getvar,
                                          NCDModuleProcess_interp_func_getobj interp_func_getobj)
{
    ASSERT(interp_func_event)
    ASSERT(interp_func_getvar)
    ASSERT(interp_func_getobj)
    
    o->interp_user = interp_user;
    o->interp_func_event = interp_func_event;
    o->interp_func_getvar = interp_func_getvar;
    o->interp_func_getobj = interp_func_getobj;
}

void NCDModuleProcess_Interp_Up (NCDModuleProcess *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == PROCESS_STATE_DOWN)
    
    BPending_Set(&o->event_job);
    o->state = PROCESS_STATE_UP_PENDING;
}

void NCDModuleProcess_Interp_Down (NCDModuleProcess *o)
{
    DebugObject_Access(&o->d_obj);
    
    switch (o->state) {
        case PROCESS_STATE_UP_PENDING: {
            BPending_Unset(&o->event_job);
            BPending_Set(&o->event_job);
            o->state = PROCESS_STATE_DOWN_CONTINUE_PENDING;
        } break;
        
        case PROCESS_STATE_UP: {
            BPending_Set(&o->event_job);
            o->state = PROCESS_STATE_DOWN_PENDING;
        } break;
        
        default: ASSERT(0);
    }
}

void NCDModuleProcess_Interp_Terminated (NCDModuleProcess *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state == PROCESS_STATE_TERMINATING)
    
    BPending_Set(&o->event_job);
    o->state = PROCESS_STATE_TERMINATED_PENDING;
}
