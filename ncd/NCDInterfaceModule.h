/**
 * @file NCDInterfaceModule.h
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

#ifndef BADVPN_NCD_NCDINTERFACEMODULE_H
#define BADVPN_NCD_NCDINTERFACEMODULE_H

#include <stdarg.h>

#include <system/BReactor.h>
#include <system/BLog.h>
#include <system/BProcess.h>
#include <system/BPending.h>
#include <ncdconfig/NCDConfig.h>

#include <generated/blog_channel_ncd.h>

#define NCDINTERFACEMODULE_EVENT_UP 1
#define NCDINTERFACEMODULE_EVENT_DOWN 2

typedef void (*NCDInterfaceModule_handler_event) (void *user, int event);
typedef void (*NCDInterfaceModule_handler_error) (void *user);

struct NCDInterfaceModule;

typedef struct {
    const struct NCDInterfaceModule *m;
    BReactor *reactor;
    BProcessManager *manager;
    struct NCDConfig_interfaces *conf;
    NCDInterfaceModule_handler_event handler_event;
    NCDInterfaceModule_handler_error handler_error;
    void *user;
    BPending event_job;
    BPending finish_job;
    int up;
    int finishing;
    void *inst_user;
    DebugObject d_obj;
    #ifndef NDEBUG
    dead_t d_dead;
    #endif
} NCDInterfaceModuleInst;

int NCDInterfaceModuleInst_Init (
    NCDInterfaceModuleInst *n, const struct NCDInterfaceModule *m, BReactor *reactor, BProcessManager *manager,
    struct NCDConfig_interfaces *conf, NCDInterfaceModule_handler_event handler_event,
    NCDInterfaceModule_handler_error handler_error,
    void *user
);
void NCDInterfaceModuleInst_Free (NCDInterfaceModuleInst *n);
void NCDInterfaceModuleInst_Finish (NCDInterfaceModuleInst *n);
void NCDInterfaceModuleInst_Backend_Event (NCDInterfaceModuleInst *n, int event);
void NCDInterfaceModuleInst_Backend_Error (NCDInterfaceModuleInst *n);
void NCDInterfaceModuleInst_Backend_Log (NCDInterfaceModuleInst *n, int level, const char *fmt, ...);

typedef void * (*NCDInterfaceModule_func_new) (NCDInterfaceModuleInst *params);
typedef void (*NCDInterfaceModule_func_free) (void *o);
typedef void (*NCDInterfaceModule_func_finish) (void *o);

struct NCDInterfaceModule {
    const char *type;
    NCDInterfaceModule_func_new func_new;
    NCDInterfaceModule_func_free func_free;
    NCDInterfaceModule_func_finish func_finish;
};

#endif
