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

#include <system/BReactor.h>
#include <ncdconfig/NCDConfig.h>

#define NCDINTERFACEMODULE_EVENT_UP 1
#define NCDINTERFACEMODULE_EVENT_DOWN 2
#define NCDINTERFACEMODULE_EVENT_ERROR 3

typedef void (*NCDInterfaceModule_handler_event) (void *user, int event);

struct NCDInterfaceModule_ncd_params {
    BReactor *reactor;
    struct NCDConfig_interfaces *conf;
    NCDInterfaceModule_handler_event handler_event;
    void *user;
};

typedef void * (*NCDInterfaceModule_func_new) (struct NCDInterfaceModule_ncd_params params, int *initial_up_state);
typedef void (*NCDInterfaceModule_func_free) (void *o);

struct NCDInterfaceModule {
    const char *type;
    NCDInterfaceModule_func_new func_new;
    NCDInterfaceModule_func_free func_free;
};

#endif
