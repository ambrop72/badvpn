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

static void frontend_event (NCDModuleInst *n, int event)
{
    n->handler_event(n->user, event);
    return;
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

void NCDModuleInst_Init (NCDModuleInst *n, const struct NCDModule *m, NCDModuleInst *method_object, NCDValue *args, const char *logprefix, BReactor *reactor, BProcessManager *manager,
                         NCDModule_handler_event handler_event, NCDModule_handler_getvar handler_getvar, void *user)
{
    // init arguments
    n->m = m;
    n->method_object = method_object;
    n->args = args;
    n->logprefix = logprefix;
    n->reactor = reactor;
    n->manager = manager;
    n->handler_event = handler_event;
    n->handler_getvar = handler_getvar;
    n->user = user;
    
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

void NCDModuleInst_Event (NCDModuleInst *n, int event)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(event == NCDMODULE_TOEVENT_DIE || event == NCDMODULE_TOEVENT_CLEAN)
    
    if (event == NCDMODULE_TOEVENT_DIE) {
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
    else if (event == NCDMODULE_TOEVENT_CLEAN) {
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
}

int NCDModuleInst_GetVar (NCDModuleInst *n, const char *name, NCDValue *out)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_UP)
    
    if (!n->m->func_getvar) {
        return 0;
    }
    
    return n->m->func_getvar(n->inst_user, name, out);
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

void NCDModuleInst_Backend_Event (NCDModuleInst *n, int event)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(event == NCDMODULE_EVENT_UP || event == NCDMODULE_EVENT_DOWN || event == NCDMODULE_EVENT_DEAD)
    
    if (event == NCDMODULE_EVENT_UP) {
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
    else if (event == NCDMODULE_EVENT_DOWN) {
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
    else if (event == NCDMODULE_EVENT_DEAD) {
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
}

int NCDModuleInst_Backend_GetVar (NCDModuleInst *n, const char *modname, const char *varname, NCDValue *out)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_DOWN_PCLEAN || n->state == STATE_DOWN_UNCLEAN || n->state == STATE_DOWN_CLEAN ||
           n->state == STATE_UP || n->state == STATE_DOWN_DIE || n->state == STATE_UP_DIE ||
           n->state == STATE_DYING)
    ASSERT(modname)
    ASSERT(varname)
    
    int res = n->handler_getvar(n->user, modname, varname, out);
    ASSERT(res == 0 || res == 1)
    
    return res;
}

void NCDModuleInst_Backend_Log (NCDModuleInst *n, int channel, int level, const char *fmt, ...)
{
    DebugObject_Access(&n->d_obj);
    
    va_list vl;
    va_start(vl, fmt);
    BLog_Append("%s", n->logprefix);
    BLog_LogToChannelVarArg(channel, level, fmt, vl);
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
