/**
 * @file NCDModule.c
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
 */

#include <stdarg.h>
#include <string.h>
#include <stddef.h>

#include <misc/string_begins_with.h>
#include <misc/parse_number.h>

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

static int object_func_getvar (NCDModuleInst *n, const char *name, NCDValue *out_value);
static int object_func_getobj (NCDModuleInst *n, const char *name, NCDObject *out_object);
static int process_args_object_func_getvar (NCDModuleProcess *o, const char *name, NCDValue *out_value);
static int process_arg_object_func_getvar2 (NCDModuleProcess *o, NCDValue *arg, const char *name, NCDValue *out_value);

static void frontend_event (NCDModuleInst *n, int event)
{
    n->params->func_event(n->user, event);
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

void NCDModuleInst_Init (NCDModuleInst *n, const struct NCDModule *m, const NCDObject *method_object, NCDValue *args, void *user, const struct NCDModuleInst_params *params)
{
    ASSERT(m)
    ASSERT(args)
    ASSERT(NCDValue_Type(args) == NCDVALUE_LIST)
    ASSERT(params)
    ASSERT(params->func_event)
    ASSERT(params->func_getobj)
    ASSERT(params->func_initprocess)
    ASSERT(params->logfunc)
    
    // init arguments
    n->m = m;
    n->method_user = (method_object ? method_object->user : NULL);
    n->args = args;
    n->user = user;
    n->params = params;
    
    // init jobs
    BPending_Init(&n->init_job, BReactor_PendingGroup(params->reactor), (BPending_handler)init_job_handler, n);
    BPending_Init(&n->uninit_job, BReactor_PendingGroup(params->reactor), (BPending_handler)uninit_job_handler, n);
    BPending_Init(&n->die_job, BReactor_PendingGroup(params->reactor), (BPending_handler)die_job_handler, n);
    BPending_Init(&n->clean_job, BReactor_PendingGroup(params->reactor), (BPending_handler)clean_job_handler, n);
    
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

NCDObject NCDModuleInst_Object (NCDModuleInst *n)
{
    DebugObject_Access(&n->d_obj);
    
    const char *type = (n->m->base_type ? n->m->base_type : n->m->type);
    
    return NCDObject_Build(type, n, (NCDObject_func_getvar)object_func_getvar, (NCDObject_func_getobj)object_func_getobj);
}

static int can_resolve (NCDModuleInst *n)
{
    switch (n->state) {
        case STATE_UP:
        case STATE_UP_DIE:
            return 1;
        case STATE_DOWN_CLEAN:
        case STATE_DOWN_UNCLEAN:
        case STATE_DOWN_PCLEAN:
        case STATE_DOWN_DIE:
            return n->m->can_resolve_when_down;
        default:
            return 0;
    }
}

static int object_func_getvar (NCDModuleInst *n, const char *name, NCDValue *out_value)
{
    DebugObject_Access(&n->d_obj);
    
    if (!n->m->func_getvar || !can_resolve(n)) {
        return 0;
    }
    
    int res = n->m->func_getvar(n->inst_user, name, out_value);
    ASSERT(res == 0 || res == 1)
    
    return res;
}

static int object_func_getobj (NCDModuleInst *n, const char *name, NCDObject *out_object)
{
    DebugObject_Access(&n->d_obj);
    
    if (!n->m->func_getobj || !can_resolve(n)) {
        return 0;
    }
    
    int res = n->m->func_getobj(n->inst_user, name, out_object);
    ASSERT(res == 0 || res == 1)
    
    return res;
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

int NCDModuleInst_Backend_GetObj (NCDModuleInst *n, const char *name, NCDObject *out_object)
{
    DebugObject_Access(&n->d_obj);
    ASSERT(n->state == STATE_DOWN_PCLEAN || n->state == STATE_DOWN_UNCLEAN || n->state == STATE_DOWN_CLEAN ||
           n->state == STATE_UP || n->state == STATE_DOWN_DIE || n->state == STATE_UP_DIE ||
           n->state == STATE_DYING)
    ASSERT(name)
    ASSERT(out_object)
    
    int res = n->params->func_getobj(n->user, name, out_object);
    ASSERT(res == 0 || res == 1)
    
    return res;
}

void NCDModuleInst_Backend_Log (NCDModuleInst *n, int channel, int level, const char *fmt, ...)
{
    DebugObject_Access(&n->d_obj);
    
    va_list vl;
    va_start(vl, fmt);
    BLog_LogViaFuncVarArg(n->params->logfunc, n->user, channel, level, fmt, vl);
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
    o->args = args;
    o->user = user;
    o->handler_event = handler_event;
    
    // set no special functions
    o->func_getspecialobj = NULL;
    
    // init event job
    BPending_Init(&o->event_job, BReactor_PendingGroup(n->params->reactor), (BPending_handler)process_event_job_handler, o);
    
    // set state
    o->state = PROCESS_STATE_INIT;
    
    // clear interp functions so we can assert they were set
    o->interp_func_event = NULL;
    o->interp_func_getobj = NULL;
    
    // init interpreter part
    if (!(n->params->func_initprocess(n->user, o, template_name))) {
        goto fail1;
    }
    
    ASSERT(o->interp_func_event)
    ASSERT(o->interp_func_getobj)
    
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
    
    // free arguments
    NCDValue_Free(&o->args);
}

void NCDModuleProcess_SetSpecialFuncs (NCDModuleProcess *o, NCDModuleProcess_func_getspecialobj func_getspecialobj)
{
    DebugObject_Access(&o->d_obj);
    
    o->func_getspecialobj = func_getspecialobj;
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

int NCDModuleProcess_GetObj (NCDModuleProcess *o, const char *name, NCDObject *out_object)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->state != PROCESS_STATE_INIT)
    ASSERT(name)
    ASSERT(out_object)
    
    // interpreter gone?
    if (o->state == PROCESS_STATE_TERMINATED_PENDING || o->state == PROCESS_STATE_TERMINATED) {
        return 0;
    }
    
    int res = o->interp_func_getobj(o->interp_user, name, out_object);
    ASSERT(res == 0 || res == 1)
    
    return res;
}

static void process_assert_interp (NCDModuleProcess *o)
{
    // assert that the interpreter knows about the object, and we're not in init
    ASSERT(o->state == PROCESS_STATE_DOWN || o->state == PROCESS_STATE_UP_PENDING ||
           o->state == PROCESS_STATE_DOWN_CONTINUE_PENDING || o->state == PROCESS_STATE_UP ||
           o->state == PROCESS_STATE_DOWN_PENDING || o->state == PROCESS_STATE_DOWN_WAITING ||
           o->state == PROCESS_STATE_TERMINATING)
}

void NCDModuleProcess_Interp_SetHandlers (NCDModuleProcess *o, void *interp_user,
                                          NCDModuleProcess_interp_func_event interp_func_event,
                                          NCDModuleProcess_interp_func_getobj interp_func_getobj)
{
    ASSERT(o->state == PROCESS_STATE_INIT)
    ASSERT(interp_func_event)
    ASSERT(interp_func_getobj)
    
    o->interp_user = interp_user;
    o->interp_func_event = interp_func_event;
    o->interp_func_getobj = interp_func_getobj;
}

void NCDModuleProcess_Interp_Up (NCDModuleProcess *o)
{
    DebugObject_Access(&o->d_obj);
    process_assert_interp(o);
    ASSERT(o->state == PROCESS_STATE_DOWN)
    
    BPending_Set(&o->event_job);
    o->state = PROCESS_STATE_UP_PENDING;
}

void NCDModuleProcess_Interp_Down (NCDModuleProcess *o)
{
    DebugObject_Access(&o->d_obj);
    process_assert_interp(o);
    
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
    process_assert_interp(o);
    ASSERT(o->state == PROCESS_STATE_TERMINATING)
    
    BPending_Set(&o->event_job);
    o->state = PROCESS_STATE_TERMINATED_PENDING;
}

int NCDModuleProcess_Interp_GetSpecialObj (NCDModuleProcess *o, const char *name, NCDObject *out_object)
{
    DebugObject_Access(&o->d_obj);
    process_assert_interp(o);
    ASSERT(name)
    ASSERT(out_object)
    
    if (!strcmp(name, "_args")) {
        *out_object = NCDObject_Build(NULL, o, (NCDObject_func_getvar)process_args_object_func_getvar, NULL);
        return 1;
    }
    
    size_t len;
    uintmax_t n;
    if ((len = string_begins_with(name, "_arg")) && parse_unsigned_integer(name + len, &n) && n < NCDValue_ListCount(&o->args)) {
        *out_object = NCDObject_Build2(NULL, o, NCDValue_ListGet(&o->args, n), (NCDObject_func_getvar2)process_arg_object_func_getvar2, NULL);
        return 1;
    }
    
    if (!o->func_getspecialobj) {
        return 0;
    }
    
    int res = o->func_getspecialobj(o->user, name, out_object);
    ASSERT(res == 0 || res == 1)
    
    return res;
}

static int process_args_object_func_getvar (NCDModuleProcess *o, const char *name, NCDValue *out_value)
{
    DebugObject_Access(&o->d_obj);
    process_assert_interp(o);
    
    if (strcmp(name, "")) {
        return 0;
    }
    
    if (!NCDValue_InitCopy(out_value, &o->args)) {
        BLog_LogToChannel(BLOG_CHANNEL_NCDModuleProcess, BLOG_ERROR, "NCDValue_InitCopy failed");
        return 0;
    }
    
    return 1;
}

static int process_arg_object_func_getvar2 (NCDModuleProcess *o, NCDValue *arg, const char *name, NCDValue *out_value)
{
    DebugObject_Access(&o->d_obj);
    process_assert_interp(o);
    
    if (!NCDValue_InitCopy(out_value, arg)) {
        BLog_LogToChannel(BLOG_CHANNEL_NCDModuleProcess, BLOG_ERROR, "NCDValue_InitCopy failed");
        return 0;
    }
    
    return 1;
}
