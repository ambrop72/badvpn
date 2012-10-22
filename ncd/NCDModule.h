/**
 * @file NCDModule.h
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

#ifndef BADVPN_NCD_NCDMODULE_H
#define BADVPN_NCD_NCDMODULE_H

#include <misc/debug.h>
#include <system/BReactor.h>
#include <base/BLog.h>
#include <system/BProcess.h>
#include <udevmonitor/NCDUdevManager.h>
#include <random/BRandom2.h>
#include <ncd/NCDObject.h>
#include <ncd/NCDStringIndex.h>

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
 * @param inst the module instance
 * @param event event number
 */
typedef void (*NCDModuleInst_func_event) (struct NCDModuleInst_s *inst, int event);

/**
 * Function called when the module instance wants the interpreter to
 * resolve an object from the point of view of its statement.
 * The instance will not be in dead state.
 * This function must not have any side effects.
 * 
 * @param inst the module instance
 * @param name name of the object as an {@link NCDStringIndex} identifier
 * @param out_object the object will be returned here
 * @return 1 on success, 0 on failure
 */
typedef int (*NCDModuleInst_func_getobj) (struct NCDModuleInst_s *inst, NCD_string_id_t name, NCDObject *out_object);

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
 * @param user value of 'user' member of {@link NCDModuleInst_iparams}
 * @param p handle for the new process backend
 * @param template_name name of the template to create the process from,
 *                      as an {@link NCDStringIndex} identifier
 * @return 1 on success, 0 on failure
 */
typedef int (*NCDModuleInst_func_initprocess) (void *user, struct NCDModuleProcess_s *p, NCD_string_id_t template_name);

/**
 * Function called when the module instance wants the interpreter to
 * initiate termination, as if it received an external terminatio request (signal).
 * 
 * @param user value of 'user' member of {@link NCDModuleInst_iparams}
 * @param exit_code exit code to return the the operating system. This overrides any previously
 *                  set exit code, and will be overriden by a signal to the value 1.
 *   
 */
typedef void (*NCDModuleInst_func_interp_exit) (void *user, int exit_code);

/**
 * Function called when the module instance wants the interpreter to
 * provide its extra command line arguments.
 * 
 * @param user value of 'user' member of {@link NCDModuleInst_iparams}
 * @param mem value memory to use
 * @param out_value write value reference here on success
 * @return 1 if available, 0 if not available. If available, but out of memory, return 1
 *         and an invalid value.
 */
typedef int (*NCDModuleInst_func_interp_getargs) (void *user, NCDValMem *mem, NCDValRef *out_value);

/**
 * Function called when the module instance wants the interpreter to
 * provide its retry time.
 * 
 * @param user value of 'user' member of {@link NCDModuleInst_iparams}
 * @return retry time in milliseconds
 */
typedef btime_t (*NCDModuleInst_func_interp_getretrytime) (void *user);

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
 * object in the process.
 * This function must have no side effects.
 * 
 * @param user as in {@link NCDModuleProcess_Init}
 * @param name name of the object as an {@link NCDStringIndex} identifier
 * @param out_object the object will be returned here
 * @return 1 on success, 0 on failure
 */
typedef int (*NCDModuleProcess_func_getspecialobj) (void *user, NCD_string_id_t name, NCDObject *out_object);

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
 * Function called to have the interpreter resolve an object within the process
 * of a process backend.
 * This function must not have any side effects.
 * 
 * @param user as in {@link NCDModuleProcess_Interp_SetHandlers}
 * @param name name of the object as an {@link NCDStringIndex} identifier
 * @param out_object the object will be returned here
 * @return 1 on success, 0 in failure
 */
typedef int (*NCDModuleProcess_interp_func_getobj) (void *user, NCD_string_id_t name, NCDObject *out_object);

struct NCDModule;

/**
 * Contains parameters to the module initialization function
 * ({@link NCDModule_func_new2}) that are passed indirectly.
 */
struct NCDModuleInst_new_params {
    /**
     * A reference to the argument list for the module instance.
     * The reference remains valid as long as the backend instance
     * exists.
     */
    NCDValRef args;
    
    /**
     * If the module instance corresponds to a method-like statement,
     * this pointer identifies the object it is being invoked with.
     * If the object is a statement (i.e. a {@link NCDModuleInst}), then this
     * will be the NCDModuleInst pointer, and {@link NCDModuleInst_Backend_GetUser}
     * can be called on this to retrieve the user pointer of the object.
     * On the other hand, if this is a method on an internal object built using
     * only {@link NCDObject_Build} or {@link NCDObject_Build2}, this pointer will
     * be whatever was passed as the "user" argument to those functions.
     */
    void *method_user;
};

/**
 * Contains parameters to {@link NCDModuleInst_Init} that are passed indirectly.
 * This itself only contains parameters related to communication between the
 * backend and the creator of the module instance; other parameters are passed
 * via the iparams member;
 */
struct NCDModuleInst_params {
    /**
     * Callback to report state changes.
     */
    NCDModuleInst_func_event func_event;
    /**
     * Callback to resolve objects from the viewpoint of the instance.
     */
    NCDModuleInst_func_getobj func_getobj;
    /**
     * Log function which appends a log prefix with {@link BLog_Append}.
     */
    BLog_logfunc logfunc;
    /**
     * Pointer to an {@link NCDModuleInst_iparams} structure, which exposes
     * services provided by the interpreter.
     */
    const struct NCDModuleInst_iparams *iparams;
};

/**
 * Contains parameters to {@link NCDModuleInst_Init} that are passed indirectly.
 * This only contains parameters related to services provided by the interpreter.
 */
struct NCDModuleInst_iparams {
    /**
     * Reactor we live in.
     */
    BReactor *reactor;
    /**
     * Process manager.
     */
    BProcessManager *manager;
    /**
     * Udev manager.
     */
    NCDUdevManager *umanager;
    /**
     * Random number generator.
     */
    BRandom2 *random2;
    /**
     * String index which keeps a mapping between strings and string identifiers.
     */
    NCDStringIndex *string_index;
    /**
     * Pointer passed to the interpreter callbacks below, for state keeping.
     */
    void *user;
    /**
     * Callback to create a new template process.
     */
    NCDModuleInst_func_initprocess func_initprocess;
    /**
     * Callback to request interpreter termination.
     */
    NCDModuleInst_func_interp_exit func_interp_exit;
    /**
     * Callback to get extra command line arguments.
     */
    NCDModuleInst_func_interp_getargs func_interp_getargs;
    /**
     * Callback to get retry time.
     */
    NCDModuleInst_func_interp_getretrytime func_interp_getretrytime;
};

/**
 * Module instance.
 * The module instance is initialized by the interpreter by calling
 * {@link NCDModuleInst_Init}. It is implemented by a module backend
 * specified in a {@link NCDModule}.
 */
typedef struct NCDModuleInst_s {
    const struct NCDModule *m;
    const struct NCDModuleInst_params *params;
    void *inst_user;
    int state;
    int is_error;
    DebugObject d_obj;
} NCDModuleInst;

/**
 * Process created from a process template on behalf of a module backend
 * instance, implemented by the interpreter.
 */
typedef struct NCDModuleProcess_s {
    NCDValRef args;
    void *user;
    NCDModuleProcess_handler_event handler_event;
    const struct NCDModuleInst_iparams *iparams; // TODO remove
    NCDModuleProcess_func_getspecialobj func_getspecialobj;
    int state;
    void *interp_user;
    NCDModuleProcess_interp_func_event interp_func_event;
    NCDModuleProcess_interp_func_getobj interp_func_getobj;
    DebugObject d_obj;
} NCDModuleProcess;

/**
 * Initializes an instance of an NCD module.
 * The instance is initialized in down state.
 * WARNING: this directly calls the module backend; expect to be called back
 * 
 * This and other non-Backend methods are the interpreter interface.
 * The Backend methods are the module backend interface and are documented
 * independently with their own logical states.
 * 
 * @param n the instance
 * @param m structure of module functions implementing the module backend
 * @param mem preallocated memory for the instance. If m->prealloc_size == 0, this must be NULL;
 *            if it is >0, it must point to a block of memory with at least that many bytes available,
 *            and properly aligned for any object.
 * @param method_object the base object if the module being initialized is a method, otherwise NULL.
 *                      The caller must ensure that this object is of the type expected by the module
 *                      being initialized.
 * @param args arguments to the module. Must be a list value. Must be available and unchanged
 *             as long as the instance exists.
 * @param user argument to callback functions
 * @param params more parameters, see {@link NCDModuleInst_params}
 */
void NCDModuleInst_Init (NCDModuleInst *n, const struct NCDModule *m, void *mem, const NCDObject *method_object, NCDValRef args, const struct NCDModuleInst_params *params);

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
 * WARNING: this directly calls the module backend; expect to be called back
 * 
 * @param n the instance
 */
void NCDModuleInst_Die (NCDModuleInst *n);

/**
 * Informs the module that it is in a clean state to proceed.
 * The instance must be in down state.
 * WARNING: this directly calls the module backend; expect to be called back
 * 
 * @param n the instance
 */
void NCDModuleInst_Clean (NCDModuleInst *n);

/**
 * Returns an {@link NCDObject} which can be used to resolve variables and objects
 * within this instance, as well as call its methods. The resulting object may only
 * be used immediately, and becomes invalid when the instance is freed.
 * 
 * @param n the instance
 * @return an NCDObject for this instance
 */
NCDObject NCDModuleInst_Object (NCDModuleInst *n);

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
 * If the alloc_size member in the {@link NCDModule} structure is >0,
 * the argument will automatically be set to the preallocated memory,
 * in which case there is no need to call this.
 * 
 * @param n backend instance handle
 * @param user value to pass to future handlers for this backend instance
 */
void NCDModuleInst_Backend_SetUser (NCDModuleInst *n, void *user);

/**
 * Retuns the argument passed to handlers of a module backend instance;
 * see {@link NCDModuleInst_Backend_SetUser}.
 * 
 * @param n backend instance handle
 * @return argument passed to handlers
 */
void * NCDModuleInst_Backend_GetUser (NCDModuleInst *n);

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
 * Resolves an object for a backend instance, from the point of the instance's
 * statement in the containing process.
 * 
 * @param n backend instance handle
 * @param name name of the object to resolve as an {@link NCDStringIndex} identifier
 * @param out_object the object will be returned here
 * @return 1 on success, 0 on failure
 */
int NCDModuleInst_Backend_GetObj (NCDModuleInst *n, NCD_string_id_t name, NCDObject *out_object) WARN_UNUSED;

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
 * Initiates interpreter termination.
 * 
 * @param n backend instance handle
 * @param exit_code exit code to return to the operating system. This overrides
 *                  any previously set exit code, and will be overriden by a
 *                  termination signal to the value 1.
 */
void NCDModuleInst_Backend_InterpExit (NCDModuleInst *n, int exit_code);

/**
 * Retrieves extra command line arguments passed to the interpreter.
 * 
 * @param n backend instance handle
 * @param mem value memory to use
 * @param out_value the arguments will be written here on success as a list value
 * @return 1 if available, 0 if not available. If available, but out of memory, returns 1
 *         and an invalid value.
 */
int NCDModuleInst_Backend_InterpGetArgs (NCDModuleInst *n, NCDValMem *mem, NCDValRef *out_value);

/**
 * Returns the retry time of the intepreter.
 * 
 * @param n backend instance handle
 * @return retry time in milliseconds
 */
btime_t NCDModuleInst_Backend_InterpGetRetryTime (NCDModuleInst *n);

/**
 * Initializes a process in the interpreter from a process template.
 * This must be called on behalf of a module backend instance.
 * The process is initializes in down state.
 * 
 * @param o the process
 * @param n backend instance whose interpreter will be providing the process
 * @param template_name name of the process template as an {@link NCDStringIndex} identifier
 * @param args arguments to the process. Must be an invalid value or a list value.
 *             The value must be available and unchanged while the process exists.
 * @param user argument to handlers
 * @param handler_event handler which reports events about the process from the
 *                      interpreter
 * @return 1 on success, 0 on failure
 */
int NCDModuleProcess_InitId (NCDModuleProcess *o, NCDModuleInst *n, NCD_string_id_t template_name, NCDValRef args, void *user, NCDModuleProcess_handler_event handler_event) WARN_UNUSED;

/**
 * Wrapper around {@link NCDModuleProcess_InitId} which takes the template name as an
 * {@link NCDValRef}, which must point to a string value.
 */
int NCDModuleProcess_InitValue (NCDModuleProcess *o, NCDModuleInst *n, NCDValRef template_name, NCDValRef args, void *user, NCDModuleProcess_handler_event handler_event) WARN_UNUSED;

/**
 * Frees the process.
 * The process must be in terminated state.
 * 
 * @param o the process
 */
void NCDModuleProcess_Free (NCDModuleProcess *o);

/**
 * Does nothing.
 * The process must be in terminated state.
 * 
 * @param o the process
 */
void NCDModuleProcess_AssertFree (NCDModuleProcess *o);

/**
 * Sets callback functions for providing special objects within the process.
 * 
 * @param o the process
 * @param func_getspecialobj function for resolving special objects, or NULL
 */
void NCDModuleProcess_SetSpecialFuncs (NCDModuleProcess *o, NCDModuleProcess_func_getspecialobj func_getspecialobj);

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
 * Resolves an object within the process from the point
 * at the end of the process.
 * This function has no side effects.
 * 
 * @param o the process
 * @param name name of the object to resolve as an {@link NCDStringIndex} identifier
 * @param out_object the object will be returned here
 * @return 1 on success, 0 on failure
 */
int NCDModuleProcess_GetObj (NCDModuleProcess *o, NCD_string_id_t name, NCDObject *out_object) WARN_UNUSED;

/**
 * Sets callback functions for the interpreter to implement the
 * process backend.
 * Must be called from within {@link NCDModuleInst_func_initprocess}
 * if success is to be reported there.
 * 
 * @param o process backend handle, as in {@link NCDModuleInst_func_initprocess}
 * @param interp_user argument to callback functions
 * @param interp_func_event function for reporting continue/terminate requests
 * @param interp_func_getobj function for resolving objects within the process
 */
void NCDModuleProcess_Interp_SetHandlers (NCDModuleProcess *o, void *interp_user,
                                          NCDModuleProcess_interp_func_event interp_func_event,
                                          NCDModuleProcess_interp_func_getobj interp_func_getobj);

/**
 * Reports the process backend as up.
 * The process backend must be in down state.
 * The process backend enters up state.
 * WARNING: this directly calls the process creator; expect to be called back
 * 
 * @param o process backend handle
 */
void NCDModuleProcess_Interp_Up (NCDModuleProcess *o);

/**
 * Reports the process backend as down.
 * The process backend must be in up state.
 * The process backend enters waiting state.
 * WARNING: this directly calls the process creator; expect to be called back
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
 * WARNING: this directly calls the process creator; expect to be called back
 * 
 * @param o process backend handle
 */
void NCDModuleProcess_Interp_Terminated (NCDModuleProcess *o);

/**
 * Resolves a special process object for the process backend.
 * 
 * @param o process backend handle
 * @param name name of the object as an {@link NCDStringIndex} identifier
 * @param out_object the object will be returned here
 * @return 1 on success, 0 on failure
 */
int NCDModuleProcess_Interp_GetSpecialObj (NCDModuleProcess *o, NCD_string_id_t name, NCDObject *out_object) WARN_UNUSED;

/**
 * Function called before any instance of any backend in a module
 * group is created;
 * 
 * @param params structure containing global resources, such as
 *               {@link BReactor}, {@link BProcessManager} and {@link NCDUdevManager}.
 * @return 1 on success, 0 on failure
 */
typedef int (*NCDModule_func_globalinit) (const struct NCDModuleInst_iparams *params);

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
 * If the backend fails initialization, this function should report the backend
 * instance to have died with error by calling {@link NCDModuleInst_Backend_SetError}
 * and {@link NCDModuleInst_Backend_Dead}.
 * 
 * @param o if the module specifies a positive alloc_size value in the {@link NCDModule}
 *          structure, this will point to the allocated memory that can be used by the
 *          module instance while it exists. Otherwise, it will be NULL.
 * @param i module backend instance handler. The backend may only use this handle via
 *          the Backend functions of {@link NCDModuleInst}.
 */
typedef void (*NCDModule_func_new2) (void *o, NCDModuleInst *i, const struct NCDModuleInst_new_params *params);

/**
 * Handler called to request termination of a backend instance.
 * The backend instance was in down or up state.
 * The backend instance enters dying state.
 * 
 * @param o see {@link NCDModuleInst_Backend_SetUser}
 */
typedef void (*NCDModule_func_die) (void *o);

/**
 * Function called to resolve a variable within a backend instance.
 * The backend instance is in up state, or in up or down state if can_resolve_when_down=1.
 * This function must not have any side effects.
 * 
 * @param o see {@link NCDModuleInst_Backend_SetUser}
 * @param name name of the variable to resolve
 * @param mem value memory to use
 * @param out on success, the backend should initialize the value here
 * @return 1 if exists, 0 if not exists. If exists, but out of memory, return 1
 *         and an invalid value.
 */
typedef int (*NCDModule_func_getvar) (void *o, const char *name, NCDValMem *mem, NCDValRef *out);

/**
 * Function called to resolve a variable within a backend instance.
 * The backend instance is in up state, or in up or down state if can_resolve_when_down=1.
 * This function must not have any side effects.
 * 
 * @param o see {@link NCDModuleInst_Backend_SetUser}
 * @param name name of the variable to resolve as an {@link NCDStringIndex} identifier
 * @param mem value memory to use
 * @param out on success, the backend should initialize the value here
 * @return 1 if exists, 0 if not exists. If exists, but out of memory, return 1
 *         and an invalid value.
 */
typedef int (*NCDModule_func_getvar2) (void *o, NCD_string_id_t name, NCDValMem *mem, NCDValRef *out);

/**
 * Function called to resolve an object within a backend instance.
 * The backend instance is in up state, or in up or down state if can_resolve_when_down=1.
 * This function must not have any side effects.
 * 
 * @param o see {@link NCDModuleInst_Backend_SetUser}
 * @param name name of the object to resolve as an {@link NCDStringIndex} identifier
 * @param out_object the object will be returned here
 * @return 1 on success, 0 on failure
 */
typedef int (*NCDModule_func_getobj) (void *o, NCD_string_id_t name, NCDObject *out_object);

/**
 * Handler called when the module instance is in a clean state.
 * This means that all statements preceding it in the process are
 * up, this statement is down, and all following statements are
 * uninitialized. When a backend instance goes down, it is guaranteed,
 * as long as it stays down, that either this will be called or
 * termination will be requested with {@link NCDModule_func_die}.
 * The backend instance was in down state.
 * 
 * @param o see {@link NCDModuleInst_Backend_SetUser}
 */
typedef void (*NCDModule_func_clean) (void *o);

#define NCDMODULE_FLAG_CAN_RESOLVE_WHEN_DOWN (1 << 0)

/**
 * Structure encapsulating the implementation of a module backend.
 */
struct NCDModule {
    /**
     * If this implements a plain statement, the name of the statement.
     * If this implements a method, then "base_type::method_name".
     */
    const char *type;
    
    /**
     * The base type for methods operating on instances of this backend.
     * Any module with type of form "base_type::method_name" is considered
     * a method of instances of this backend.
     * If this is NULL, the base type will default to type.
     */
    const char *base_type;
    
    /**
     * Function called to create an new backend instance.
     */
    NCDModule_func_new2 func_new2;
    
    /**
     * Function called to request termination of a backend instance.
     * May be NULL, in which case the default is to call NCDModuleInst_Backend_Dead().
     */
    NCDModule_func_die func_die;
    
    /**
     * Function called to resolve a variable within the backend instance.
     * May be NULL.
     */
    NCDModule_func_getvar func_getvar;
    
    /**
     * Function called to resolve a variable within the backend instance.
     * May be NULL.
     */
    NCDModule_func_getvar2 func_getvar2;
    
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
    
    /**
     * Various flags.
     * 
     * - NCDMODULE_FLAG_CAN_RESOLVE_WHEN_DOWN
     *   Whether the interpreter is allowed to call func_getvar and func_getobj
     *   even when the backend instance is in down state (as opposed to just
     *   in up state.
     */
    int flags;
    
    /**
     * The amount of memory to preallocate for each module instance.
     * Preallocation can be used to avoid having to allocate memory from
     * module initialization. The memory can be accessed via the first
     * argument to {@link NCDModule_func_new2} and other calls (except if
     * the callback pointer is changed subsequently using
     * {@link NCDModuleInst_Backend_SetUser}).
     */
    int alloc_size;
    
    /**
     * The string identifier of this module's base_type (or type if base_type is
     * not specified) according to {@link NCDStringIndex}. This is initialized
     * by the interpreter on startup and should not be set by the module.
     */
    NCD_string_id_t base_type_id;
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
    struct NCDModule *modules;
    
    /**
     * A pointer to an array of requests for string identifiers. The 'str'
     * member of each element of this array should be set to a string which
     * is to be mapped to an identifier using {@link NCDStringIndex}. The
     * array must be terminated with an element with a NULL 'str' member.
     * The interpreter will use {@link NCDStringIndex_GetRequests} to set
     * the corresponds 'id' members.
     * This can be NULL if the module does not require mapping any strings.
     */
    struct NCD_string_request *strings;
};

#endif
