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

/**
 * Function called to inform the interpeter of state changes of the
 * module instance.
 * Possible events are:
 * 
 * - NCDMODULE_EVENT_UP: the instance came up.
 *   The instance was in down state.
 *   The instance enters up state.
 * 
 * - NCDMODULE_EVENT_DOWN: the instance went down.
 *   The instance was in up state.
 *   The instance enters down state.
 * 
 *   After the instance goes down, the interpreter should eventually call
 *   {@link NCDModuleInst_Clean} or {@link NCDModuleInst_Die}, unless
 *   the module goes up again.
 * 
 * - NCDMODULE_EVENT_DEAD: the module died.
 *   The instance enters dead state.
 * 
 * This function is not being called in event context. The interpreter should
 * only update its internal state, and visibly react only via jobs that it pushes
 * from within this function. The only exception is that it may free the
 * instance from within the NCDMODULE_EVENT_DEAD event.
 * 
 * @param user as in {@link NCDModuleInst_Init}
 * @param event event number
 */
typedef void (*NCDModuleInst_func_event) (void *user, int event);

/**
 * Function called when the module instance wants the interpreter to
 * resolve a variable from the point of view of its statement.
 * The instance will not be in dead state.
 * This function must not have any side effects.
 * 
 * @param user as in {@link NCDModuleInst_Init}
 * @param name name of the variable
 * @param out on success, the interpreter should initialize the value here
 * @return 1 on success, 0 on failure
 */
typedef int (*NCDModuleInst_func_getvar) (void *user, const char *name, NCDValue *out);

/**
 * Function called when the module instance wants the interpreter to
 * resolve an object from the point of view of its statement.
 * The instance will not be in dead state.
 * This function must not have any side effects.
 * 
 * @param user as in {@link NCDModuleInst_Init}
 * @param name name of the object
 * @return object, or NULL on failure
 */
typedef struct NCDModuleInst_s * (*NCDModuleInst_func_getobj) (void *user, const char *name);

/**
 * Function called when the module instance wants the interpreter to
 * create a new process backend from a process template.
 * The instance will not be in dead state.
 * 
 * On success, the interpreter must have called {@link NCDModuleProcess_Interp_SetHandlers}
 * from within this function, to allow communication with the controller of the process.
 * On success, the new process backend enters down state.
 * 
 * This function is not being called in event context. The interpreter should
 * only update its internal state, and visibly react only via jobs that it pushes
 * from within this function.
 * 
 * @param user as in {@link NCDModuleInst_Init}
 * @param p handle for the new process backend
 * @param template_name name of the template to create the process from
 * @param args process arguments. On success, the arguments become owned
 *             by the interpreter. On failure, the interpreter must leave
 *             them unchanged.
 * @return 1 on success, 0 on failure
 */
typedef int (*NCDModuleInst_func_initprocess) (void *user, struct NCDModuleProcess_s *p, const char *template_name, NCDValue args);

#define NCDMODULEPROCESS_EVENT_UP 1
#define NCDMODULEPROCESS_EVENT_DOWN 2
#define NCDMODULEPROCESS_EVENT_TERMINATED 3

/**
 * Handler which reports process state changes from the interpreter.
 * Possible events are:
 * 
 * - NCDMODULEPROCESS_EVENT_UP: the process went up.
 *   The process was in down state.
 *   The process enters up state.
 * 
 * - NCDMODULEPROCESS_EVENT_DOWN: the process went down.
 *   The process was in up state.
 *   The process enters waiting state.
 * 
 *   NOTE: the process enters waiting state, NOT down state, and is paused.
 *   To allow the process to continue, call {@link NCDModuleProcess_Continue}.
 * 
 * - NCDMODULEPROCESS_EVENT_TERMINATED: the process terminated.
 *   The process was in terminating state.
 *   The process enters terminated state.
 * 
 * @param user as in {@link NCDModuleProcess_Init}
 * @param event event number
 */
typedef void (*NCDModuleProcess_handler_event) (void *user, int event);

/**
 * Function called when the interpreter wants to resolve a special
 * variable in the process.
 * This function must have no side effects.
 * 
 * @param user as in {@link NCDModuleProcess_Init}
 * @param name name of the variable
 * @param out on success, should initialize the value here
 * @return 1 on success, 0 on failure
 */
typedef int (*NCDModuleProcess_func_getspecialvar) (void *user, const char *name, NCDValue *out);

/**
 * Function called when the interpreter wants to resolve a special
 * object in the process.
 * This function must have no side effects.
 * 
 * @param user as in {@link NCDModuleProcess_Init}
 * @param name name of the object
 * @return object, or NULL on failure
 */
typedef struct NCDModuleInst_s * (*NCDModuleProcess_func_getspecialobj) (void *user, const char *name);

#define NCDMODULEPROCESS_INTERP_EVENT_CONTINUE 1
#define NCDMODULEPROCESS_INTERP_EVENT_TERMINATE 2

/**
 * Function called to report process backend requests to the interpreter.
 * Possible events are:
 * 
 * - NCDMODULEPROCESS_INTERP_EVENT_CONTINUE: the process can continue.
 *   The process backend was in waiting state.
 *   The process backend enters down state.
 * 
 * - NCDMODULEPROCESS_INTERP_EVENT_TERMINATE: the process should terminate.
 *   The process backend was in down, up or waiting state.
 *   The process backend enters terminating state.
 * 
 *   The interpreter should call {@link NCDModuleProcess_Interp_Terminated}
 *   when the process terminates.
 * 
 * This function is not being called in event context. The interpreter should
 * only update its internal state, and visibly react only via jobs that it pushes
 * from within this function.
 * 
 * @param user as in {@link NCDModuleProcess_Interp_SetHandlers}
 * @param event event number
 */
typedef void (*NCDModuleProcess_interp_func_event) (void *user, int event);

/**
 * Function called to have the interpreter resolve a variable within the process
 * of a process backend.
 * This function must not have any side effects.
 * 
 * @param user as in {@link NCDModuleProcess_Interp_SetHandlers}
 * @param name name of the variable
 * @param out on success, the interpreter should initialize the value here
 * @return 1 on success, 0 on failure
 */
typedef int (*NCDModuleProcess_interp_func_getvar) (void *user, const char *name, NCDValue *out);

/**
 * Function called to have the interpreter resolve an object within the process
 * of a process backend.
 * This function must not have any side effects.
 * 
 * @param user as in {@link NCDModuleProcess_Interp_SetHandlers}
 * @param name name of the object
 * @return object, or NULL on failure
 */
typedef struct NCDModuleInst_s * (*NCDModuleProcess_interp_func_getobj) (void *user, const char *name);

struct NCDModule;

struct NCDModuleInitParams {
    BReactor *reactor;
    BProcessManager *manager;
    NCDUdevManager *umanager;
};

/**
 * Module instance.
 * The module instance is initialized by the interpreter by calling
 * {@link NCDModuleInst_Init}. It is implemented by a module backend
 * specified in a {@link NCDModule}.
 */
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

/**
 * Process created from a process template on behalf of a module backend
 * instance, implemented by the interpreter.
 */
typedef struct NCDModuleProcess_s {
    NCDModuleInst *n;
    void *user;
    NCDModuleProcess_handler_event handler_event;
    NCDModuleProcess_func_getspecialvar func_getspecialvar;
    NCDModuleProcess_func_getspecialobj func_getspecialobj;
    BPending event_job;
    int state;
    void *interp_user;
    NCDModuleProcess_interp_func_event interp_func_event;
    NCDModuleProcess_interp_func_getvar interp_func_getvar;
    NCDModuleProcess_interp_func_getobj interp_func_getobj;
    DebugObject d_obj;
} NCDModuleProcess;

/**
 * Initializes an instance of an NCD module.
 * The instance is initialized in down state.
 * 
 * This and other non-Backend methods are the interpreter interface.
 * The Backend methods are the module backend interface and are documented
 * independently with their own logical states.
 * 
 * @param n the instance
 * @param m structure of module functions implementing the module backend
 * @param method_object the base module if the module being initialized is a method, otherwise NULL.
 *                      The caller must ensure that this module is of the type expected by the module
 *                      being initialized.
 * @param args arguments to the module. Must be a NCDVALUE_LIST value. Must be available as long as
 *             the instance is freed.
 * @param reactor reactor we live in
 * @param manager process manager
 * @param umanager udev manager
 * @param user argument to callback functions
 * @param func_event callback to report state changes
 * @param func_getvar callback to resolve variables from the viewpoint of the instance
 * @param func_getobj callback to resolve objects from the viewpoint of the instance
 * @param logfunc log function which appends a log prefix with {@link BLog_Append}
 */
void NCDModuleInst_Init (NCDModuleInst *n, const struct NCDModule *m, NCDModuleInst *method_object, NCDValue *args, BReactor *reactor, BProcessManager *manager, NCDUdevManager *umanager, void *user,
                         NCDModuleInst_func_event func_event,
                         NCDModuleInst_func_getvar func_getvar,
                         NCDModuleInst_func_getobj func_getobj,
                         NCDModuleInst_func_initprocess func_initprocess,
                         BLog_logfunc logfunc);

/**
 * Frees the instance.
 * The instance must be in dead state.
 * 
 * @param n the instance
 */
void NCDModuleInst_Free (NCDModuleInst *n);

/**
 * Requests the instance to die.
 * The instance must be in down or up state.
 * The instance enters dying state.
 * 
 * @param n the instance
 */
void NCDModuleInst_Die (NCDModuleInst *n);

/**
 * Informs the module that it is in a clean state to proceed.
 * The instance must be in down state.
 * 
 * @param n the instance
 */
void NCDModuleInst_Clean (NCDModuleInst *n);

/**
 * Resolves a variable within the instance.
 * The instance must be in up state.
 * This function does not have any side effects.
 * 
 * @param n the instance
 * @param name name of the variable to resolve
 * @param out the value will be initialized here if successful
 * @return 1 on success, 0 on failure
 */
int NCDModuleInst_GetVar (NCDModuleInst *n, const char *name, NCDValue *out) WARN_UNUSED;

/**
 * Resolves an object within the instance.
 * The instance must be in up state.
 * This function does not have any side effects.
 * 
 * @param n the instance
 * @param name name of the object to resolve
 * @return module instance, or NULL on failure
 */
NCDModuleInst * NCDModuleInst_GetObj (NCDModuleInst *n, const char *name) WARN_UNUSED;

/**
 * Checks whether the module terminated unsuccessfully.
 * The instance must be in dead state.
 * 
 * @param n the instance
 * @return 1 if module terminated unsuccessfully, 0 if successfully
 */
int NCDModuleInst_HaveError (NCDModuleInst *n);

/**
 * Sets the argument passed to handlers of a module backend instance.
 * 
 * @param n backend instance handle
 * @param user value to pass to future handlers for this backend instance
 */
void NCDModuleInst_Backend_SetUser (NCDModuleInst *n, void *user);

/**
 * Puts the backend instance into up state.
 * The instance must be in down state.
 * The instance enters up state.
 * 
 * @param n backend instance handle
 */
void NCDModuleInst_Backend_Up (NCDModuleInst *n);

/**
 * Puts the backend instance into down state.
 * The instance must be in up state.
 * The instance enters down state.
 * 
 * @param n backend instance handle
 */
void NCDModuleInst_Backend_Down (NCDModuleInst *n);

/**
 * Destroys the backend instance.
 * The backend instance handle becomes invalid and must not be used from
 * the backend any longer.
 * 
 * @param n backend instance handle
 */
void NCDModuleInst_Backend_Dead (NCDModuleInst *n);

/**
 * Resolves a variable for a backend instance, from the point of the instance's
 * statement in the containing process.
 * 
 * @param n backend instance handle
 * @param name name of the variable to resolve
 * @param out the value will be initialized here if successful
 * @return 1 on success, 0 on failure
 */
int NCDModuleInst_Backend_GetVar (NCDModuleInst *n, const char *name, NCDValue *out) WARN_UNUSED;

/**
 * Resolves an object for a backend instance, from the point of the instance's
 * statement in the containing process.
 * 
 * @param n backend instance handle
 * @param name name of the object to resolve
 * @return module instance, or NULL on failure
 */
NCDModuleInst * NCDModuleInst_Backend_GetObj (NCDModuleInst *n, const char *name) WARN_UNUSED;

/**
 * Logs a backend instance message.
 * 
 * @param n backend instance handle
 * @param channel log channel
 * @param level loglevel
 * @param fmt format string as in printf, arguments follow
 */
void NCDModuleInst_Backend_Log (NCDModuleInst *n, int channel, int level, const char *fmt, ...);

/**
 * Sets the error flag for the module instance.
 * The error flag only has no effect until the backend calls
 * {@link NCDModuleInst_Backend_Dead}.
 * 
 * @param n backend instance handle
 */
void NCDModuleInst_Backend_SetError (NCDModuleInst *n);

/**
 * Initializes a process in the interpreter from a process template.
 * This must be called on behalf of a module backend instance.
 * The process is initializes in down state.
 * 
 * @param o the process
 * @param n backend instance whose interpreter will be providing the process
 * @param template_name name of the process template
 * @param args arguments to the process. On success, the arguments become owned
 *             by the interpreter. On failure, they are left untouched.
 * @param user argument to handlers
 * @param handler_event handler which reports events about the process from the
 *                      interpreter
 * @return 1 on success, 0 on failure
 */
int NCDModuleProcess_Init (NCDModuleProcess *o, NCDModuleInst *n, const char *template_name, NCDValue args, void *user, NCDModuleProcess_handler_event handler_event);

/**
 * Frees the process.
 * The process must be in terminated state.
 * 
 * @param o the process
 */
void NCDModuleProcess_Free (NCDModuleProcess *o);

/**
 * Sets callback functions for providing special variables and objects within
 * the process.
 * 
 * @param o the process
 * @param func_getspecialvar function for resolving special variables, or NULL
 * @param func_getspecialobj function for resolving special objects, or NULL
 */
void NCDModuleProcess_SetSpecialFuncs (NCDModuleProcess *o, NCDModuleProcess_func_getspecialvar func_getspecialvar, NCDModuleProcess_func_getspecialobj func_getspecialobj);

/**
 * Continues the process after the process went down.
 * The process must be in waiting state.
 * The process enters down state.
 * 
 * @param o the process
 */
void NCDModuleProcess_Continue (NCDModuleProcess *o);

/**
 * Requests the process to terminate.
 * The process must be in down, up or waiting state.
 * The process enters terminating state.
 * 
 * @param o the process
 */
void NCDModuleProcess_Terminate (NCDModuleProcess *o);

/**
 * Resolves a variable within the process from the point
 * at the end of the process.
 * This function has no side effects.
 * 
 * @param o the process
 * @param name name of the variable to resolve
 * @param out the value will be initialized here if successful
 * @return 1 on success, 0 on failure
 */
int NCDModuleProcess_GetVar (NCDModuleProcess *o, const char *name, NCDValue *out) WARN_UNUSED;

/**
 * Resolves an object within the process from the point
 * at the end of the process.
 * This function has no side effects.
 * 
 * @param o the process
 * @param name name of the object to resolve
 * @return module instance, or NULL on failure
 */
NCDModuleInst * NCDModuleProcess_GetObj (NCDModuleProcess *o, const char *name) WARN_UNUSED;

/**
 * Sets callback functions for the interpreter to implement the
 * process backend.
 * Must be called from within {@link NCDModuleInst_func_initprocess}
 * if success is to be reported there.
 * 
 * @param o process backend handle, as in {@link NCDModuleInst_func_initprocess}
 * @param interp_user argument to callback functions
 * @param interp_func_event function for reporting continue/terminate requests
 * @param interp_func_getvar function for resolving variables within the process
 * @param interp_func_getobj function for resolving objects within the process
 */
void NCDModuleProcess_Interp_SetHandlers (NCDModuleProcess *o, void *interp_user,
                                          NCDModuleProcess_interp_func_event interp_func_event,
                                          NCDModuleProcess_interp_func_getvar interp_func_getvar,
                                          NCDModuleProcess_interp_func_getobj interp_func_getobj);

/**
 * Reports the process backend as up.
 * The process backend must be in down state.
 * The process backend enters up state.
 * 
 * @param o process backend handle
 */
void NCDModuleProcess_Interp_Up (NCDModuleProcess *o);

/**
 * Reports the process backend as down.
 * The process backend must be in up state.
 * The process backend enters waiting state.
 * 
 * NOTE: the backend enters waiting state, NOT down state. The interpreter should
 * pause the process until {@link NCDModuleProcess_interp_func_event} reports
 * NCDMODULEPROCESS_INTERP_EVENT_CONTINUE, unless termination is requested via
 * NCDMODULEPROCESS_INTERP_EVENT_TERMINATE.
 * 
 * @param o process backend handle
 */
void NCDModuleProcess_Interp_Down (NCDModuleProcess *o);

/**
 * Reports termination of the process backend.
 * The process backend must be in terminating state.
 * The process backend handle becomes invalid and must not be used
 * by the interpreter any longer.
 * 
 * @param o process backend handle
 */
void NCDModuleProcess_Interp_Terminated (NCDModuleProcess *o);

/**
 * Resolves a special process variable for the process backend.
 * 
 * @param o process backend handle
 * @param name name of the variable
 * @param out the value will be initialized here if successful
 * @return 1 on success, 0 on failure
 */
int NCDModuleProcess_Interp_GetSpecialVar (NCDModuleProcess *o, const char *name, NCDValue *out) WARN_UNUSED;

/**
 * Resolves a special process object for the process backend.
 * 
 * @param o process backend handle
 * @param name name of the object
 * @return object, or NULL on failure
 */
NCDModuleInst * NCDModuleProcess_Interp_GetSpecialObj (NCDModuleProcess *o, const char *name) WARN_UNUSED;

/**
 * Function called before any instance of any backend in a module
 * group is created;
 * 
 * @param params structure containing global resources, in particular
 *               {@link BReactor}, {@link BProcessManager} and {@link NCDUdevManager}
 * @return 1 on success, 0 on failure
 */
typedef int (*NCDModule_func_globalinit) (const struct NCDModuleInitParams params);

/**
 * Function called to clean up after {@link NCDModule_func_globalinit} and modules
 * in a module group.
 * There are no backend instances alive from this module group.
 */ 
typedef void (*NCDModule_func_globalfree) (void);

/**
 * Handler called to create an new module backend instance.
 * The backend is initialized in down state.
 * 
 * This handler should call {@link NCDModuleInst_Backend_SetUser} to provide
 * an argument for handlers of this backend instance.
 * 
 * If the backend fails initialization, this function should report the backend
 * instance to have died with error by calling {@link NCDModuleInst_Backend_SetError}
 * and {@link NCDModuleInst_Backend_Dead}.
 * 
 * @param i module backend instance handler. The backend may only use this handle via
 *          the Backend functions of {@link NCDModuleInst}.
 */
typedef void (*NCDModule_func_new) (NCDModuleInst *i);

/**
 * Handler called to request termination of a backend instance.
 * The backend instance was in down or up state.
 * The backend instance enters dying state.
 * 
 * @param o as in {@link NCDModuleInst_Backend_SetUser}, or NULL by default
 */
typedef void (*NCDModule_func_die) (void *o);

/**
 * Function called to resolve a variable within a backend instance.
 * The backend instance is in up state.
 * This function must not have any side effects.
 * 
 * @param o as in {@link NCDModuleInst_Backend_SetUser}, or NULL by default
 * @param name name of the variable to resolve
 * @param out on success, the backend should initialize the value here
 * @return 1 on success, 0 on failure
 */
typedef int (*NCDModule_func_getvar) (void *o, const char *name, NCDValue *out);

/**
 * Function called to resolve an object within a backend instance.
 * The backend instance is in up state.
 * This function must not have any side effects.
 * 
 * @param o as in {@link NCDModuleInst_Backend_SetUser}, or NULL by default
 * @param name name of the object to resolve
 * @return object, or NULL on failure
 */
typedef NCDModuleInst * (*NCDModule_func_getobj) (void *o, const char *name);

/**
 * Handler called when the module instance is in a clean state.
 * This means that all statements preceding it in the process are
 * up, this statement is down, and all following statements are
 * uninitialized. When a backend instance goes down, it is guaranteed,
 * as long as it stays down, that either this will be called or
 * termination will be requested with {@link NCDModule_func_die}.
 * The backend instance was in down state.
 * 
 * @param o as in {@link NCDModuleInst_Backend_SetUser}, or NULL by default
 */
typedef void (*NCDModule_func_clean) (void *o);

/**
 * Structure encapsulating the implementation of a module backend.
 */
struct NCDModule {
    /**
     * Type string of the backend. This is either a plain name,
     * or "type_name::method_name" for a method operating on a backend instance
     * of type type_name.
     */
    const char *type;
    
    /**
     * Function called to create an new backend instance.
     */
    NCDModule_func_new func_new;
    
    /**
     * Function called to request termination of a backend instance.
     */
    NCDModule_func_die func_die;
    
    /**
     * Function called to resolve a variable within the backend instance.
     * May be NULL.
     */
    NCDModule_func_getvar func_getvar;
    
    /**
     * Function called to resolve an object within the backend instance.
     * May be NULL.
     */
    NCDModule_func_getobj func_getobj;
    
    /**
     * Function called when the backend instance is in a clean state.
     * May be NULL.
     */
    NCDModule_func_clean func_clean;
};

/**
 * Structure encapsulating a group of module backend implementations,
 * with global init and free functions.
 */
struct NCDModuleGroup {
    /**
     * Function called before any instance of any module backend in this
     * group is crated. May be NULL.
     */
    NCDModule_func_globalinit func_globalinit;
    
    /**
     * Function called to clean up after {@link NCDModule_func_globalinit}.
     * May be NULL.
     */
    NCDModule_func_globalfree func_globalfree;
    
    /**
     * Array of module backends. The array must be terminated with a
     * structure that has a NULL type member.
     */
    const struct NCDModule *modules;
};

#endif
