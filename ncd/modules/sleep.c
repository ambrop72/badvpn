/**
 * @file sleep.c
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
 * Module which sleeps a given number of milliseconds on inititalization and
 * deinitialization.
 * 
 * Synopsis: sleep(string ms_start, string ms_stop)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_sleep.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    btime_t ms_start;
    btime_t ms_stop;
    BTimer timer;
    int dying;
};

static void instance_free (struct instance *o);

static void timer_handler (void *vo)
{
    struct instance *o = vo;
    
    if (!o->dying) {
        // signal up
        NCDModuleInst_Backend_Up(o->i);
    } else {
        // die
        instance_free(o);
    }
}

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
    NCDValue *ms_start_arg;
    NCDValue *ms_stop_arg;
    if (!NCDValue_ListRead(i->args, 2, &ms_start_arg, &ms_stop_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(ms_start_arg) != NCDVALUE_STRING || NCDValue_Type(ms_stop_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    if (sscanf(NCDValue_StringValue(ms_start_arg), "%"SCNi64, &o->ms_start) != 1) {
        ModuleLog(o->i, BLOG_ERROR, "wrong time");
        goto fail1;
    }
    if (sscanf(NCDValue_StringValue(ms_stop_arg), "%"SCNi64, &o->ms_stop) != 1) {
        ModuleLog(o->i, BLOG_ERROR, "wrong time");
        goto fail1;
    }
    
    // init timer
    BTimer_Init(&o->timer, 0, timer_handler, o);
    
    // set not dying
    o->dying = 0;
    
    // set timer
    BReactor_SetTimerAfter(o->i->reactor, &o->timer, o->ms_start);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Dead(i);
}

void instance_free (struct instance *o)
{
    NCDModuleInst *i = o->i;
    
    // free timer
    BReactor_RemoveTimer(o->i->reactor, &o->timer);
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Dead(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    
    // set dying
    o->dying = 1;
    
    // set timer
    BReactor_SetTimerAfter(o->i->reactor, &o->timer, o->ms_stop);
}

static const struct NCDModule modules[] = {
    {
        .type = "sleep",
        .func_new = func_new,
        .func_die = func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_sleep = {
    .modules = modules
};
