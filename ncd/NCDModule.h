/**
 * @file NCDModule.h
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

#ifndef BADVPN_NCD_NCDMODULE_H
#define BADVPN_NCD_NCDMODULE_H

#include <stdarg.h>

#include <system/BReactor.h>
#include <system/BProcess.h>
#include <system/BPending.h>
#include <system/BLog.h>
#include <ncdconfig/NCDConfig.h>
#include <ncd/NCDValue.h>

#define NCDMODULE_EVENT_UP 1
#define NCDMODULE_EVENT_DOWN 2
#define NCDMODULE_EVENT_DYING 3

#define NCDMODULE_TOEVENT_DIE 101
#define NCDMODULE_TOEVENT_CLEAN 102

typedef void (*NCDModule_handler_event) (void *user, int event);
typedef void (*NCDModule_handler_died) (void *user, int is_error);

struct NCDModule;

typedef struct {
    const char *name;
    const struct NCDModule *m;
    NCDValue *args;
    const char *logprefix;
    BReactor *reactor;
    BProcessManager *manager;
    NCDModule_handler_event handler_event;
    NCDModule_handler_died handler_died;
    void *user;
    BPending event_job;
    int event_job_event;
    BPending toevent_job;
    int toevent_job_event;
    int state;
    void *inst_user;
    DebugObject d_obj;
    #ifndef NDEBUG
    dead_t d_dead;
    #endif
} NCDModuleInst;

int NCDModuleInst_Init (NCDModuleInst *n, const char *name, const struct NCDModule *m, NCDValue *args, const char *logprefix, BReactor *reactor, BProcessManager *manager,
                        NCDModule_handler_event handler_event, NCDModule_handler_died handler_died, void *user);
void NCDModuleInst_Free (NCDModuleInst *n);
void NCDModuleInst_Event (NCDModuleInst *n, int event);
int NCDModuleInst_GetVar (NCDModuleInst *n, const char *name, NCDValue *out);
void NCDModuleInst_Backend_Event (NCDModuleInst *n, int event);
void NCDModuleInst_Backend_Died (NCDModuleInst *n, int is_error);
void NCDModuleInst_Backend_Log (NCDModuleInst *n, int channel, int level, const char *fmt, ...);

typedef int (*NCDModule_func_globalinit) (void);
typedef void * (*NCDModule_func_new) (NCDModuleInst *params);
typedef void (*NCDModule_func_free) (void *o);
typedef void (*NCDModule_func_die) (void *o);
typedef int (*NCDModule_func_getvar) (void *o, const char *name, NCDValue *out);
typedef void (*NCDModule_func_clean) (void *o);

struct NCDModule {
    const char *type;
    NCDModule_func_new func_new;
    NCDModule_func_free func_free;
    NCDModule_func_die func_die;
    NCDModule_func_getvar func_getvar;
    NCDModule_func_clean func_clean;
};

struct NCDModuleGroup {
    NCDModule_func_globalinit func_globalinit;
    const struct NCDModule *modules;
};

#endif
