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

#include <misc/debug.h>
#include <system/BReactor.h>
#include <base/BPending.h>
#include <base/BLog.h>
#include <system/BProcess.h>
#include <udevmonitor/NCDUdevManager.h>
#include <ncd/NCDValue.h>

#define NCDMODULE_EVENT_UP 1
#define NCDMODULE_EVENT_DOWN 2
#define NCDMODULE_EVENT_DEAD 3

#define NCDMODULE_TOEVENT_DIE 101
#define NCDMODULE_TOEVENT_CLEAN 102

struct NCDModuleInst_s;
struct NCDModuleProcess_s;

typedef void (*NCDModule_handler_event) (void *user, int event);
typedef int (*NCDModule_handler_getvar) (void *user, const char *varname, NCDValue *out);
typedef struct NCDModuleInst_s * (*NCDModule_handler_getobj) (void *user, const char *objname);
typedef int (*NCDModule_handler_initprocess) (void *user, struct NCDModuleProcess_s *p, const char *template_name, NCDValue args);

typedef void (*NCDModuleProcess_handler_dead) (void *user);
typedef void (*NCDModuleProcess_interp_handler_die) (void *user);

struct NCDModule;

struct NCDModuleInitParams {
    BReactor *reactor;
    BProcessManager *manager;
    NCDUdevManager *umanager;
};

typedef struct NCDModuleInst_s {
    const struct NCDModule *m;
    struct NCDModuleInst_s *method_object;
    NCDValue *args;
    BReactor *reactor;
    BProcessManager *manager;
    NCDUdevManager *umanager;
    void *user;
    NCDModule_handler_event handler_event;
    NCDModule_handler_getvar handler_getvar;
    NCDModule_handler_getobj handler_getobj;
    NCDModule_handler_initprocess handler_initprocess;
    BLog_logfunc logfunc;
    BPending init_job;
    BPending uninit_job;
    BPending die_job;
    BPending clean_job;
    int state;
    void *inst_user;
    int is_error;
    DebugObject d_obj;
} NCDModuleInst;

typedef struct NCDModuleProcess_s {
    NCDModuleInst *n;
    void *user;
    NCDModuleProcess_handler_dead handler_dead;
    void *interp_user;
    NCDModuleProcess_interp_handler_die interp_handler_die;
    BPending die_job;
    BPending dead_job;
    int state;
    DebugObject d_obj;
} NCDModuleProcess;

void NCDModuleInst_Init (NCDModuleInst *n, const struct NCDModule *m, NCDModuleInst *method_object, NCDValue *args, BReactor *reactor, BProcessManager *manager, NCDUdevManager *umanager, void *user,
                         NCDModule_handler_event handler_event,
                         NCDModule_handler_getvar handler_getvar,
                         NCDModule_handler_getobj handler_getobj,
                         NCDModule_handler_initprocess handler_initprocess,
                         BLog_logfunc logfunc);
void NCDModuleInst_Free (NCDModuleInst *n);
void NCDModuleInst_Event (NCDModuleInst *n, int event);
int NCDModuleInst_GetVar (NCDModuleInst *n, const char *name, NCDValue *out) WARN_UNUSED;
NCDModuleInst * NCDModuleInst_GetObj (NCDModuleInst *n, const char *objname) WARN_UNUSED;
int NCDModuleInst_HaveError (NCDModuleInst *n);
void NCDModuleInst_Backend_SetUser (NCDModuleInst *n, void *user);
void NCDModuleInst_Backend_Event (NCDModuleInst *n, int event);
int NCDModuleInst_Backend_GetVar (NCDModuleInst *n, const char *varname, NCDValue *out) WARN_UNUSED;
NCDModuleInst * NCDModuleInst_Backend_GetObj (NCDModuleInst *n, const char *objname) WARN_UNUSED;
void NCDModuleInst_Backend_Log (NCDModuleInst *n, int channel, int level, const char *fmt, ...);
void NCDModuleInst_Backend_SetError (NCDModuleInst *n);

int NCDModuleProcess_Init (NCDModuleProcess *o, NCDModuleInst *n, const char *template_name, NCDValue args, void *user, NCDModuleProcess_handler_dead handler_dead);
void NCDModuleProcess_Free (NCDModuleProcess *o);
void NCDModuleProcess_Die (NCDModuleProcess *o);
void NCDModuleProcess_Interp_SetHandlers (NCDModuleProcess *o, void *interp_user, NCDModuleProcess_interp_handler_die interp_handler_die);
void NCDModuleProcess_Interp_Dead (NCDModuleProcess *o);

typedef int (*NCDModule_func_globalinit) (const struct NCDModuleInitParams params);
typedef void (*NCDModule_func_globalfree) (void);
typedef void (*NCDModule_func_new) (NCDModuleInst *params);
typedef void (*NCDModule_func_die) (void *o);
typedef int (*NCDModule_func_getvar) (void *o, const char *name, NCDValue *out);
typedef NCDModuleInst * (*NCDModule_func_getobj) (void *o, const char *objname);
typedef void (*NCDModule_func_clean) (void *o);

struct NCDModule {
    const char *type;
    NCDModule_func_new func_new;
    NCDModule_func_die func_die;
    NCDModule_func_getvar func_getvar;
    NCDModule_func_getobj func_getobj;
    NCDModule_func_clean func_clean;
};

struct NCDModuleGroup {
    NCDModule_func_globalinit func_globalinit;
    NCDModule_func_globalfree func_globalfree;
    const struct NCDModule *modules;
};

#endif
