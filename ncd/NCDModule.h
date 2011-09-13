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

struct NCDModuleInst_s;
struct NCDModuleProcess_s;

typedef void (*NCDModuleInst_func_event) (void *user, int event);
typedef int (*NCDModuleInst_func_getvar) (void *user, const char *varname, NCDValue *out);
typedef struct NCDModuleInst_s * (*NCDModuleInst_func_getobj) (void *user, const char *objname);
typedef int (*NCDModuleInst_func_initprocess) (void *user, struct NCDModuleProcess_s *p, const char *template_name, NCDValue args);

#define NCDMODULEPROCESS_EVENT_UP 1
#define NCDMODULEPROCESS_EVENT_DOWN 2
#define NCDMODULEPROCESS_EVENT_TERMINATED 3

typedef void (*NCDModuleProcess_handler_event) (void *user, int event);

#define NCDMODULEPROCESS_INTERP_EVENT_CONTINUE 1
#define NCDMODULEPROCESS_INTERP_EVENT_TERMINATE 2

typedef void (*NCDModuleProcess_interp_func_event) (void *user, int event);
typedef int (*NCDModuleProcess_interp_func_getvar) (void *user, const char *name, NCDValue *out);
typedef struct NCDModuleInst_s * (*NCDModuleProcess_interp_func_getobj) (void *user, const char *name);

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
    NCDModuleInst_func_event func_event;
    NCDModuleInst_func_getvar func_getvar;
    NCDModuleInst_func_getobj func_getobj;
    NCDModuleInst_func_initprocess func_initprocess;
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
    NCDModuleProcess_handler_event handler_event;
    BPending event_job;
    int state;
    void *interp_user;
    NCDModuleProcess_interp_func_event interp_func_event;
    NCDModuleProcess_interp_func_getvar interp_func_getvar;
    NCDModuleProcess_interp_func_getobj interp_func_getobj;
    DebugObject d_obj;
} NCDModuleProcess;

void NCDModuleInst_Init (NCDModuleInst *n, const struct NCDModule *m, NCDModuleInst *method_object, NCDValue *args, BReactor *reactor, BProcessManager *manager, NCDUdevManager *umanager, void *user,
                         NCDModuleInst_func_event func_event,
                         NCDModuleInst_func_getvar func_getvar,
                         NCDModuleInst_func_getobj func_getobj,
                         NCDModuleInst_func_initprocess func_initprocess,
                         BLog_logfunc logfunc);
void NCDModuleInst_Free (NCDModuleInst *n);
void NCDModuleInst_Die (NCDModuleInst *n);
void NCDModuleInst_Clean (NCDModuleInst *n);
int NCDModuleInst_GetVar (NCDModuleInst *n, const char *name, NCDValue *out) WARN_UNUSED;
NCDModuleInst * NCDModuleInst_GetObj (NCDModuleInst *n, const char *objname) WARN_UNUSED;
int NCDModuleInst_HaveError (NCDModuleInst *n);
void NCDModuleInst_Backend_SetUser (NCDModuleInst *n, void *user);
void NCDModuleInst_Backend_Up (NCDModuleInst *n);
void NCDModuleInst_Backend_Down (NCDModuleInst *n);
void NCDModuleInst_Backend_Dead (NCDModuleInst *n);
int NCDModuleInst_Backend_GetVar (NCDModuleInst *n, const char *varname, NCDValue *out) WARN_UNUSED;
NCDModuleInst * NCDModuleInst_Backend_GetObj (NCDModuleInst *n, const char *objname) WARN_UNUSED;
void NCDModuleInst_Backend_Log (NCDModuleInst *n, int channel, int level, const char *fmt, ...);
void NCDModuleInst_Backend_SetError (NCDModuleInst *n);

int NCDModuleProcess_Init (NCDModuleProcess *o, NCDModuleInst *n, const char *template_name, NCDValue args, void *user, NCDModuleProcess_handler_event handler_event);
void NCDModuleProcess_Free (NCDModuleProcess *o);
void NCDModuleProcess_Continue (NCDModuleProcess *o);
void NCDModuleProcess_Terminate (NCDModuleProcess *o);
int NCDModuleProcess_GetVar (NCDModuleProcess *o, const char *name, NCDValue *out) WARN_UNUSED;
NCDModuleInst * NCDModuleProcess_GetObj (NCDModuleProcess *o, const char *name) WARN_UNUSED;
void NCDModuleProcess_Interp_SetHandlers (NCDModuleProcess *o, void *interp_user,
                                          NCDModuleProcess_interp_func_event interp_func_event,
                                          NCDModuleProcess_interp_func_getvar interp_func_getvar,
                                          NCDModuleProcess_interp_func_getobj interp_func_getobj);
void NCDModuleProcess_Interp_Up (NCDModuleProcess *o);
void NCDModuleProcess_Interp_Down (NCDModuleProcess *o);
void NCDModuleProcess_Interp_Terminated (NCDModuleProcess *o);

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
