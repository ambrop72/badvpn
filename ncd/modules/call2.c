/**
 * @file call2.c
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
 * Synopsis:
 *   call2(string template, list args)
 *   call2_if(string cond, string template, list args)
 *   call2_ifelse(string cond, string template, string else_template, list args)
 *   embcall2(string template)
 *   embcall2_if(string cond, string template)
 *   embcall2_ifelse(string cond, string template, string else_template)
 *   embcall2_multif(string cond1, string template1, ..., [string else_template])
 */

#include <stdlib.h>
#include <string.h>

#include <misc/debug.h>
#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_call2.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define STATE_WORKING 1
#define STATE_UP 2
#define STATE_WAITING 3
#define STATE_TERMINATING 4
#define STATE_NONE 5

struct instance {
    NCDModuleInst *i;
    NCDModuleProcess process;
    int embed;
    int state;
};

static void process_handler_event (struct instance *o, int event);
static int process_func_getspecialobj (struct instance *o, const char *name, NCDObject *out_object);
static int caller_obj_func_getobj (struct instance *o, const char *name, NCDObject *out_object);
static void func_new_templ (NCDModuleInst *i, const char *template_name, NCDValue *args, int embed);
static void instance_free (struct instance *o);

static void process_handler_event (struct instance *o, int event)
{
    switch (event) {
        case NCDMODULEPROCESS_EVENT_UP: {
            ASSERT(o->state == STATE_WORKING)
            
            // signal up
            NCDModuleInst_Backend_Up(o->i);
            
            // set state up
            o->state = STATE_UP;
        } break;
        
        case NCDMODULEPROCESS_EVENT_DOWN: {
            ASSERT(o->state == STATE_UP)
            
            // signal down
            NCDModuleInst_Backend_Down(o->i);
            
            // set state waiting
            o->state = STATE_WAITING;
        } break;
        
        case NCDMODULEPROCESS_EVENT_TERMINATED: {
            ASSERT(o->state == STATE_TERMINATING)
            
            // die finally
            instance_free(o);
            return;
        } break;
        
        default: ASSERT(0);
    }
}

static int process_func_getspecialobj (struct instance *o, const char *name, NCDObject *out_object)
{
    if (o->embed) {
        return NCDModuleInst_Backend_GetObj(o->i, name, out_object);
    }
    
    if (!strcmp(name, "_caller")) {
        *out_object = NCDObject_Build(NULL, o, NULL, (NCDObject_func_getobj)caller_obj_func_getobj);
        return 1;
    }
    
    return 0;
}

static int caller_obj_func_getobj (struct instance *o, const char *name, NCDObject *out_object)
{
    return NCDModuleInst_Backend_GetObj(o->i, name, out_object);
}

static void func_new_templ (NCDModuleInst *i, const char *template_name, NCDValue *args, int embed)
{
    ASSERT(!args || NCDValue_IsList(args))
    ASSERT(embed == !!embed)
    
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    o->i = i;
    NCDModuleInst_Backend_SetUser(i, o);
    
    // remember embed
    o->embed = embed;
    
    if (!template_name || !strcmp(template_name, "<none>")) {
        // signal up
        NCDModuleInst_Backend_Up(o->i);
        
        // set state none
        o->state = STATE_NONE;
    } else {
        // copy arguments
        NCDValue args_copy;
        if (!args) {
            NCDValue_InitList(&args_copy);
        } else {
            if (!NCDValue_InitCopy(&args_copy, args)) {
                ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitCopy failed");
                goto fail1;
            }
        }
        
        // create process
        if (!NCDModuleProcess_Init(&o->process, o->i, template_name, args_copy, o, (NCDModuleProcess_handler_event)process_handler_event)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDModuleProcess_Init failed");
            NCDValue_Free(&args_copy);
            goto fail1;
        }
        
        // if this is an embed call, make the process ignore arguments
        if (embed) {
            NCDModuleProcess_SetNoArgs(&o->process);
        }
        
        // set special functions
        NCDModuleProcess_SetSpecialFuncs(&o->process, (NCDModuleProcess_func_getspecialobj)process_func_getspecialobj);
        
        // set state working
        o->state = STATE_WORKING;
    }
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void instance_free (struct instance *o)
{
    NCDModuleInst *i = o->i;
    
    // free process
    if (o->state != STATE_NONE) {
        NCDModuleProcess_Free(&o->process);
    }
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_call (NCDModuleInst *i)
{
    NCDValue *template_arg;
    NCDValue *args_arg;
    if (!NCDValue_ListRead(i->args, 2, &template_arg, &args_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDValue_IsStringNoNulls(template_arg) || !NCDValue_IsList(args_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    func_new_templ(i, NCDValue_StringValue(template_arg), args_arg, 0);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_embcall (NCDModuleInst *i)
{
    NCDValue *template_arg;
    if (!NCDValue_ListRead(i->args, 1, &template_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDValue_IsStringNoNulls(template_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    func_new_templ(i, NCDValue_StringValue(template_arg), NULL, 1);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_call_if (NCDModuleInst *i)
{
    NCDValue *cond_arg;
    NCDValue *template_arg;
    NCDValue *args_arg;
    if (!NCDValue_ListRead(i->args, 3, &cond_arg, &template_arg, &args_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDValue_IsString(cond_arg) || !NCDValue_IsStringNoNulls(template_arg) || !NCDValue_IsList(args_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    const char *template_name = NULL;
    
    if (NCDValue_StringEquals(cond_arg, "true")) {
        template_name = NCDValue_StringValue(template_arg);
    }
    
    func_new_templ(i, template_name, args_arg, 0);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_embcall_if (NCDModuleInst *i)
{
    NCDValue *cond_arg;
    NCDValue *template_arg;
    if (!NCDValue_ListRead(i->args, 2, &cond_arg, &template_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDValue_IsString(cond_arg) || !NCDValue_IsStringNoNulls(template_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    const char *template_name = NULL;
    
    if (NCDValue_StringEquals(cond_arg, "true")) {
        template_name = NCDValue_StringValue(template_arg);
    }
    
    func_new_templ(i, template_name, NULL, 1);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_call_ifelse (NCDModuleInst *i)
{
    NCDValue *cond_arg;
    NCDValue *template_arg;
    NCDValue *else_template_arg;
    NCDValue *args_arg;
    if (!NCDValue_ListRead(i->args, 4, &cond_arg, &template_arg, &else_template_arg, &args_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDValue_IsString(cond_arg) || !NCDValue_IsStringNoNulls(template_arg) || !NCDValue_IsStringNoNulls(else_template_arg) || !NCDValue_IsList(args_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    const char *template_name;
    
    if (NCDValue_StringEquals(cond_arg, "true")) {
        template_name = NCDValue_StringValue(template_arg);
    } else {
        template_name = NCDValue_StringValue(else_template_arg);
    }
    
    func_new_templ(i, template_name, args_arg, 0);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_embcall_ifelse (NCDModuleInst *i)
{
    NCDValue *cond_arg;
    NCDValue *template_arg;
    NCDValue *else_template_arg;
    if (!NCDValue_ListRead(i->args, 3, &cond_arg, &template_arg, &else_template_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDValue_IsString(cond_arg) || !NCDValue_IsStringNoNulls(template_arg) || !NCDValue_IsStringNoNulls(else_template_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    const char *template_name;
    
    if (NCDValue_StringEquals(cond_arg, "true")) {
        template_name = NCDValue_StringValue(template_arg);
    } else {
        template_name = NCDValue_StringValue(else_template_arg);
    }
    
    func_new_templ(i, template_name, NULL, 1);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_new_embcall_multif (NCDModuleInst *i)
{
    const char *template_name = NULL;
    
    NCDValue *arg = NCDValue_ListFirst(i->args);
    
    while (arg) {
        NCDValue *arg2 = NCDValue_ListNext(i->args, arg);
        if (!arg2) {
            if (!NCDValue_IsStringNoNulls(arg)) {
                ModuleLog(i, BLOG_ERROR, "bad arguments");
                goto fail0;
            }
            
            template_name = NCDValue_StringValue(arg);
            break;
        }
        
        if (!NCDValue_IsString(arg) || !NCDValue_IsStringNoNulls(arg2)) {
            ModuleLog(i, BLOG_ERROR, "bad arguments");
            goto fail0;
        }
        
        if (NCDValue_StringEquals(arg, "true")) {
            template_name = NCDValue_StringValue(arg2);
            break;
        }
        
        arg = NCDValue_ListNext(i->args, arg2);
    }
    
    func_new_templ(i, template_name, NULL, 1);
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    ASSERT(o->state != STATE_TERMINATING)
    
    // if none, die now
    if (o->state == STATE_NONE) {
        instance_free(o);
        return;
    }
    
    // request process to terminate
    NCDModuleProcess_Terminate(&o->process);
    
    // set state terminating
    o->state = STATE_TERMINATING;
}

static void func_clean (void *vo)
{
    struct instance *o = vo;
    if (o->state != STATE_WAITING) {
        return;
    }
    
    // allow process to continue
    NCDModuleProcess_Continue(&o->process);
    
    // set state working
    o->state = STATE_WORKING;
}

static int func_getobj (void *vo, const char *name, NCDObject *out_object)
{
    struct instance *o = vo;
    
    if (o->state == STATE_NONE) {
        return 0;
    }
    
    return NCDModuleProcess_GetObj(&o->process, name, out_object);
}

static const struct NCDModule modules[] = {
    {
        .type = "call2",
        .func_new = func_new_call,
        .func_die = func_die,
        .func_clean = func_clean,
        .func_getobj = func_getobj,
        .can_resolve_when_down = 1
    }, {
        .type = "call2_if",
        .func_new = func_new_call_if,
        .func_die = func_die,
        .func_clean = func_clean,
        .func_getobj = func_getobj,
        .can_resolve_when_down = 1
    }, {
        .type = "call2_ifelse",
        .func_new = func_new_call_ifelse,
        .func_die = func_die,
        .func_clean = func_clean,
        .func_getobj = func_getobj,
        .can_resolve_when_down = 1
    }, {
        .type = "embcall2",
        .func_new = func_new_embcall,
        .func_die = func_die,
        .func_clean = func_clean,
        .func_getobj = func_getobj,
        .can_resolve_when_down = 1
    }, {
        .type = "embcall2_if",
        .func_new = func_new_embcall_if,
        .func_die = func_die,
        .func_clean = func_clean,
        .func_getobj = func_getobj,
        .can_resolve_when_down = 1
    }, {
        .type = "embcall2_ifelse",
        .func_new = func_new_embcall_ifelse,
        .func_die = func_die,
        .func_clean = func_clean,
        .func_getobj = func_getobj,
        .can_resolve_when_down = 1
    }, {
        .type = "embcall2_multif",
        .func_new = func_new_embcall_multif,
        .func_die = func_die,
        .func_clean = func_clean,
        .func_getobj = func_getobj,
        .can_resolve_when_down = 1
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_call2 = {
    .modules = modules
};
