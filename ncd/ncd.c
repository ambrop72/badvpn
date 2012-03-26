/**
 * @file ncd.c
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <misc/version.h>
#include <misc/loggers_string.h>
#include <misc/loglevel.h>
#include <misc/offset.h>
#include <misc/read_file.h>
#include <misc/balloc.h>
#include <misc/concat_strings.h>
#include <misc/string_begins_with.h>
#include <misc/parse_number.h>
#include <misc/open_standard_streams.h>
#include <misc/expstring.h>
#include <structure/LinkedList1.h>
#include <structure/LinkedList2.h>
#include <base/BLog.h>
#include <system/BReactor.h>
#include <system/BSignal.h>
#include <system/BConnection.h>
#include <system/BProcess.h>
#include <udevmonitor/NCDUdevManager.h>
#include <ncd/NCDConfigParser.h>
#include <ncd/NCDModule.h>
#include <ncd/NCDModuleIndex.h>
#include <ncd/modules/modules.h>

#ifndef BADVPN_USE_WINAPI
#include <base/BLog_syslog.h>
#endif

#include <ncd/ncd.h>

#include <generated/blog_channel_ncd.h>

#define LOGGER_STDOUT 1
#define LOGGER_SYSLOG 2

#define ARG_VALUE_TYPE_STRING 1
#define ARG_VALUE_TYPE_VARIABLE 2
#define ARG_VALUE_TYPE_LIST 3
#define ARG_VALUE_TYPE_MAP 4

#define SSTATE_CHILD 1
#define SSTATE_ADULT 2
#define SSTATE_DYING 3
#define SSTATE_FORGOTTEN 4

#define PSTATE_WORKING 1
#define PSTATE_UP 2
#define PSTATE_WAITING 3
#define PSTATE_TERMINATING 4

struct arg_value {
    int type;
    union {
        char *string;
        char **variable_names;
        LinkedList1 list;
        LinkedList1 maplist;
    };
};

struct arg_list_elem {
    LinkedList1Node list_node;
    struct arg_value value;
};

struct arg_map_elem {
    LinkedList1Node maplist_node;
    struct arg_value key;
    struct arg_value val;
};

struct statement {
    char **object_names;
    char *method_name;
    struct arg_value args;
    char *name;
};

struct process {
    NCDModuleProcess *module_process;
    char *name;
    size_t num_statements;
    struct process_statement *statements;
    int state;
    size_t ap;
    size_t fp;
    BTimer wait_timer;
    BPending advance_job;
    BPending work_job;
    LinkedList2Node list_node; // node in processes
};

struct process_statement {
    struct process *p;
    size_t i;
    struct statement s;
    int state;
    const struct NCDModule *module;
    int have_error;
    btime_t error_until;
    NCDModuleInst inst;
    NCDValue inst_args;
};

// command-line options
struct {
    int help;
    int version;
    int logger;
    #ifndef BADVPN_USE_WINAPI
    char *logger_syslog_facility;
    char *logger_syslog_ident;
    #endif
    int loglevel;
    int loglevels[BLOG_NUM_CHANNELS];
    char *config_file;
    int retry_time;
    int no_udev;
    char **extra_args;
    int num_extra_args;
} options;

// reactor
BReactor ss;

// are we terminating
int terminating;
int main_exit_code;

// process manager
BProcessManager manager;

// udev manager
NCDUdevManager umanager;

// module index
NCDModuleIndex mindex;

// config AST
struct NCDConfig_processes *config_ast;

// common module parameters
struct NCDModuleInst_params module_params;

// processes
LinkedList2 processes;

static void print_help (const char *name);
static void print_version (void);
static int parse_arguments (int argc, char *argv[]);
static void signal_handler (void *unused);
static void start_terminate (int exit_code);
static int arg_value_init_string (struct arg_value *o, const char *string);
static int arg_value_init_variable (struct arg_value *o, struct NCDConfig_strings *ast_names);
static void arg_value_init_list (struct arg_value *o);
static int arg_value_list_append (struct arg_value *o, struct arg_value v);
static void arg_value_init_map (struct arg_value *o);
static int arg_value_map_append (struct arg_value *o, struct arg_value key, struct arg_value val);
static void arg_value_free (struct arg_value *o);
static int build_arg_from_ast (struct arg_value *o, struct NCDConfig_list *ast);
static int build_arg_list_from_ast_list (struct arg_value *o, struct NCDConfig_list *list);
static int build_arg_map_from_ast_list (struct arg_value *o, struct NCDConfig_list *list);
static char ** names_new (struct NCDConfig_strings *ast_names);
static size_t names_count (char **names);
static char * names_tostring (char **names);
static void names_free (char **names);
static int statement_init (struct statement *s, struct NCDConfig_statements *conf);
static void statement_free (struct statement *s);
static int process_new (struct NCDConfig_processes *conf, NCDModuleProcess *module_process);
static void process_free (struct process *p);
static void process_start_terminating (struct process *p);
static void process_free_statements (struct process *p);
static size_t process_rap (struct process *p);
static void process_assert_pointers (struct process *p);
static void process_logfunc (struct process *p);
static void process_log (struct process *p, int level, const char *fmt, ...);
static void process_schedule_work (struct process *p);
static void process_work_job_handler (struct process *p);
static void process_advance_job_handler (struct process *p);
static void process_wait_timer_handler (struct process *p);
static int process_find_object (struct process *p, size_t pos, const char *name, NCDObject *out_object);
static int process_resolve_object_expr (struct process *p, size_t pos, char **names, NCDObject *out_object);
static int process_resolve_variable_expr (struct process *p, size_t pos, char **names, NCDValue *out_value);
static void process_statement_logfunc (struct process_statement *ps);
static void process_statement_log (struct process_statement *ps, int level, const char *fmt, ...);
static void process_statement_set_error (struct process_statement *ps);
static int process_statement_resolve_argument (struct process_statement *ps, struct arg_value *arg, NCDValue *out);
static void process_statement_instance_func_event (struct process_statement *ps, int event);
static int process_statement_instance_func_getobj (struct process_statement *ps, const char *objname, NCDObject *out_object);
static int process_statement_instance_func_initprocess (struct process_statement *ps, NCDModuleProcess *mp, const char *template_name);
static void process_statement_instance_logfunc (struct process_statement *ps);
static void process_statement_instance_func_interp_exit (struct process_statement *ps, int exit_code);
static int process_statement_instance_func_interp_getargs (struct process_statement *ps, NCDValue *out_value);
static void process_moduleprocess_func_event (struct process *p, int event);
static int process_moduleprocess_func_getobj (struct process *p, const char *name, NCDObject *out_object);

int main (int argc, char **argv)
{
    if (argc <= 0) {
        return 1;
    }
    
    // set exit code
    main_exit_code = 1;
    
    // open standard streams
    open_standard_streams();
    
    // parse command-line arguments
    if (!parse_arguments(argc, argv)) {
        fprintf(stderr, "Failed to parse arguments\n");
        print_help(argv[0]);
        goto fail0;
    }
    
    // handle --help and --version
    if (options.help) {
        print_version();
        print_help(argv[0]);
        return 0;
    }
    if (options.version) {
        print_version();
        return 0;
    }
    
    // initialize logger
    switch (options.logger) {
        case LOGGER_STDOUT:
            BLog_InitStdout();
            break;
        #ifndef BADVPN_USE_WINAPI
        case LOGGER_SYSLOG:
            if (!BLog_InitSyslog(options.logger_syslog_ident, options.logger_syslog_facility)) {
                fprintf(stderr, "Failed to initialize syslog logger\n");
                goto fail0;
            }
            break;
        #endif
        default:
            ASSERT(0);
    }
    
    // configure logger channels
    for (int i = 0; i < BLOG_NUM_CHANNELS; i++) {
        if (options.loglevels[i] >= 0) {
            BLog_SetChannelLoglevel(i, options.loglevels[i]);
        }
        else if (options.loglevel >= 0) {
            BLog_SetChannelLoglevel(i, options.loglevel);
        }
    }
    
    BLog(BLOG_NOTICE, "initializing "GLOBAL_PRODUCT_NAME" "PROGRAM_NAME" "GLOBAL_VERSION);
    
    // initialize network
    if (!BNetwork_GlobalInit()) {
        BLog(BLOG_ERROR, "BNetwork_GlobalInit failed");
        goto fail1;
    }
    
    // init time
    BTime_Init();
    
    // init reactor
    if (!BReactor_Init(&ss)) {
        BLog(BLOG_ERROR, "BReactor_Init failed");
        goto fail1;
    }
    
    // set not terminating
    terminating = 0;
    
    // init process manager
    if (!BProcessManager_Init(&manager, &ss)) {
        BLog(BLOG_ERROR, "BProcessManager_Init failed");
        goto fail1a;
    }
    
    // init udev manager
    NCDUdevManager_Init(&umanager, options.no_udev, &ss, &manager);
    
    // init module index
    NCDModuleIndex_Init(&mindex);
    
    // add module groups to index
    for (const struct NCDModuleGroup **g = ncd_modules; *g; g++) {
        if (!NCDModuleIndex_AddGroup(&mindex, *g)) {
            BLog(BLOG_ERROR, "NCDModuleIndex_AddGroup failed");
            goto fail2;
        }
    }
    
    // setup signal handler
    if (!BSignal_Init(&ss, signal_handler, NULL)) {
        BLog(BLOG_ERROR, "BSignal_Init failed");
        goto fail2;
    }
    
    // read config file
    uint8_t *file;
    size_t file_len;
    if (!read_file(options.config_file, &file, &file_len)) {
        BLog(BLOG_ERROR, "failed to read config file");
        goto fail3;
    }
    
    // parse config file
    if (!NCDConfigParser_Parse((char *)file, file_len, &config_ast)) {
        BLog(BLOG_ERROR, "NCDConfigParser_Parse failed");
        free(file);
        goto fail3;
    }
    
    // fee config file memory
    free(file);
    
    // init module params
    struct NCDModuleInitParams params;
    params.reactor = &ss;
    params.manager = &manager;
    params.umanager = &umanager;
    
    // init modules
    size_t num_inited_modules = 0;
    for (const struct NCDModuleGroup **g = ncd_modules; *g; g++) {
        if ((*g)->func_globalinit && !(*g)->func_globalinit(params)) {
            BLog(BLOG_ERROR, "globalinit failed for some module");
            goto fail5;
        }
        num_inited_modules++;
    }
    
    // init common module params
    module_params.reactor = &ss;
    module_params.manager = &manager;
    module_params.umanager = &umanager;
    module_params.func_event = (NCDModuleInst_func_event)process_statement_instance_func_event;
    module_params.func_getobj = (NCDModuleInst_func_getobj)process_statement_instance_func_getobj;
    module_params.func_initprocess = (NCDModuleInst_func_initprocess)process_statement_instance_func_initprocess;
    module_params.logfunc = (BLog_logfunc)process_statement_instance_logfunc;
    module_params.func_interp_exit = (NCDModuleInst_func_interp_exit)process_statement_instance_func_interp_exit;
    module_params.func_interp_getargs = (NCDModuleInst_func_interp_getargs)process_statement_instance_func_interp_getargs;
    
    // init processes list
    LinkedList2_Init(&processes);
    
    // init processes
    struct NCDConfig_processes *conf = config_ast;
    while (conf) {
        if (!conf->is_template) {
            process_new(conf, NULL);
        }
        conf = conf->next;
    }
    
    // enter event loop
    BLog(BLOG_NOTICE, "entering event loop");
    BReactor_Exec(&ss);
    
    ASSERT(LinkedList2_IsEmpty(&processes))
    
fail5:
    // free modules
    while (num_inited_modules > 0) {
        const struct NCDModuleGroup **g = &ncd_modules[num_inited_modules - 1];
        if ((*g)->func_globalfree) {
            (*g)->func_globalfree();
        }
        num_inited_modules--;
    }
    // free configuration
    NCDConfig_free_processes(config_ast);
fail3:
    // remove signal handler
    BSignal_Finish();
fail2:
    // free module index
    NCDModuleIndex_Free(&mindex);
    
    // free udev manager
    NCDUdevManager_Free(&umanager);
    
    // free process manager
    BProcessManager_Free(&manager);
fail1a:
    // free reactor
    BReactor_Free(&ss);
fail1:
    // free logger
    BLog(BLOG_NOTICE, "exiting");
    BLog_Free();
fail0:
    // finish objects
    DebugObjectGlobal_Finish();
    
    return main_exit_code;
}

void print_help (const char *name)
{
    printf(
        "Usage:\n"
        "    %s\n"
        "        [--help]\n"
        "        [--version]\n"
        "        [--logger <"LOGGERS_STRING">]\n"
        #ifndef BADVPN_USE_WINAPI
        "        (logger=syslog?\n"
        "            [--syslog-facility <string>]\n"
        "            [--syslog-ident <string>]\n"
        "        )\n"
        #endif
        "        [--loglevel <0-5/none/error/warning/notice/info/debug>]\n"
        "        [--channel-loglevel <channel-name> <0-5/none/error/warning/notice/info/debug>] ...\n"
        "        --config-file <file>\n"
        "        [--retry-time <ms>]\n"
        "        [--no-udev]\n"
        "        [-- [<extra_arg>] ...]\n",
        name
    );
}

void print_version (void)
{
    printf(GLOBAL_PRODUCT_NAME" "PROGRAM_NAME" "GLOBAL_VERSION"\n"GLOBAL_COPYRIGHT_NOTICE"\n");
}

int parse_arguments (int argc, char *argv[])
{
    if (argc <= 0) {
        return 0;
    }
    
    options.help = 0;
    options.version = 0;
    options.logger = LOGGER_STDOUT;
    #ifndef BADVPN_USE_WINAPI
    options.logger_syslog_facility = "daemon";
    options.logger_syslog_ident = argv[0];
    #endif
    options.loglevel = -1;
    for (int i = 0; i < BLOG_NUM_CHANNELS; i++) {
        options.loglevels[i] = -1;
    }
    options.config_file = NULL;
    options.retry_time = DEFAULT_RETRY_TIME;
    options.no_udev = 0;
    options.extra_args = NULL;
    options.num_extra_args = 0;
    
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (!strcmp(arg, "--help")) {
            options.help = 1;
        }
        else if (!strcmp(arg, "--version")) {
            options.version = 1;
        }
        else if (!strcmp(arg, "--logger")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            char *arg2 = argv[i + 1];
            if (!strcmp(arg2, "stdout")) {
                options.logger = LOGGER_STDOUT;
            }
            #ifndef BADVPN_USE_WINAPI
            else if (!strcmp(arg2, "syslog")) {
                options.logger = LOGGER_SYSLOG;
            }
            #endif
            else {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        #ifndef BADVPN_USE_WINAPI
        else if (!strcmp(arg, "--syslog-facility")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.logger_syslog_facility = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--syslog-ident")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.logger_syslog_ident = argv[i + 1];
            i++;
        }
        #endif
        else if (!strcmp(arg, "--loglevel")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if ((options.loglevel = parse_loglevel(argv[i + 1])) < 0) {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        else if (!strcmp(arg, "--channel-loglevel")) {
            if (2 >= argc - i) {
                fprintf(stderr, "%s: requires two arguments\n", arg);
                return 0;
            }
            int channel = BLogGlobal_GetChannelByName(argv[i + 1]);
            if (channel < 0) {
                fprintf(stderr, "%s: wrong channel argument\n", arg);
                return 0;
            }
            int loglevel = parse_loglevel(argv[i + 2]);
            if (loglevel < 0) {
                fprintf(stderr, "%s: wrong loglevel argument\n", arg);
                return 0;
            }
            options.loglevels[channel] = loglevel;
            i += 2;
        }
        else if (!strcmp(arg, "--config-file")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.config_file = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--retry-time")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if ((options.retry_time = atoi(argv[i + 1])) < 0) {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
        else if (!strcmp(arg, "--no-udev")) {
            options.no_udev = 1;
        }
        else if (!strcmp(arg, "--")) {
            options.extra_args = &argv[i + 1];
            options.num_extra_args = argc - i - 1;
            i += options.num_extra_args;
        }
        else {
            fprintf(stderr, "unknown option: %s\n", arg);
            return 0;
        }
    }
    
    if (options.help || options.version) {
        return 1;
    }
    
    if (!options.config_file) {
        fprintf(stderr, "--config-file is required\n");
        return 0;
    }
    
    return 1;
}

void signal_handler (void *unused)
{
    BLog(BLOG_NOTICE, "termination requested");
    
    start_terminate(1);
}

void start_terminate (int exit_code)
{
    main_exit_code = exit_code;
    
    if (terminating) {
        return;
    }
    
    terminating = 1;
    
    if (LinkedList2_IsEmpty(&processes)) {
        BReactor_Quit(&ss, 0);
        return;
    }
    
    // start terminating non-template processes
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &processes);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        struct process *p = UPPER_OBJECT(n, struct process, list_node);
        if (p->module_process) {
            continue;
        }
        if (p->state != PSTATE_TERMINATING) {
            process_start_terminating(p);
        }
    }
}

int arg_value_init_string (struct arg_value *o, const char *string)
{
    o->type = ARG_VALUE_TYPE_STRING;
    if (!(o->string = strdup(string))) {
        BLog(BLOG_ERROR, "strdup failed");
        return 0;
    }
    
    return 1;
}

int arg_value_init_variable (struct arg_value *o, struct NCDConfig_strings *ast_names)
{
    ASSERT(ast_names)
    
    o->type = ARG_VALUE_TYPE_VARIABLE;
    if (!(o->variable_names = names_new(ast_names))) {
        return 0;
    }
    
    return 1;
}

void arg_value_init_list (struct arg_value *o)
{
    o->type = ARG_VALUE_TYPE_LIST;
    LinkedList1_Init(&o->list);
}

int arg_value_list_append (struct arg_value *o, struct arg_value v)
{
    ASSERT(o->type == ARG_VALUE_TYPE_LIST)
    
    struct arg_list_elem *elem = malloc(sizeof(*elem));
    if (!elem) {
        BLog(BLOG_ERROR, "malloc failed");
        return 0;
    }
    LinkedList1_Append(&o->list, &elem->list_node);
    elem->value = v;
    
    return 1;
}

void arg_value_init_map (struct arg_value *o)
{
    o->type = ARG_VALUE_TYPE_MAP;
    LinkedList1_Init(&o->maplist);
}

int arg_value_map_append (struct arg_value *o, struct arg_value key, struct arg_value val)
{
    ASSERT(o->type == ARG_VALUE_TYPE_MAP)
    
    struct arg_map_elem *elem = malloc(sizeof(*elem));
    if (!elem) {
        BLog(BLOG_ERROR, "malloc failed");
        return 0;
    }
    LinkedList1_Append(&o->maplist, &elem->maplist_node);
    elem->key = key;
    elem->val = val;
    
    return 1;
}

void arg_value_free (struct arg_value *o)
{
    switch (o->type) {
        case ARG_VALUE_TYPE_STRING: {
            free(o->string);
        } break;
        
        case ARG_VALUE_TYPE_VARIABLE: {
            names_free(o->variable_names);
        } break;
        
        case ARG_VALUE_TYPE_LIST: {
            while (!LinkedList1_IsEmpty(&o->list)) {
                struct arg_list_elem *elem = UPPER_OBJECT(LinkedList1_GetFirst(&o->list), struct arg_list_elem, list_node);
                arg_value_free(&elem->value);
                LinkedList1_Remove(&o->list, &elem->list_node);
                free(elem);
            }
        } break;
        
        case ARG_VALUE_TYPE_MAP: {
            while (!LinkedList1_IsEmpty(&o->maplist)) {
                struct arg_map_elem *elem = UPPER_OBJECT(LinkedList1_GetFirst(&o->maplist), struct arg_map_elem, maplist_node);
                arg_value_free(&elem->key);
                arg_value_free(&elem->val);
                LinkedList1_Remove(&o->maplist, &elem->maplist_node);
                free(elem);
            }
        } break;
        
        default: ASSERT(0);
    }
}

int build_arg_from_ast (struct arg_value *o, struct NCDConfig_list *ast)
{
    switch (ast->type) {
        case NCDCONFIG_ARG_STRING: {
            if (!arg_value_init_string(o, ast->string)) {
                return 0;
            }
        } break;
        
        case NCDCONFIG_ARG_VAR: {
            if (!arg_value_init_variable(o, ast->var)) {
                return 0;
            }
        } break;
        
        case NCDCONFIG_ARG_LIST: {
            if (!build_arg_list_from_ast_list(o, ast->list)) {
                return 0;
            }
        } break;
        
        case NCDCONFIG_ARG_MAPLIST: {
            if (!build_arg_map_from_ast_list(o, ast->list)) {
                return 0;
            }
        } break;
        
        default: ASSERT(0);
    }
    
    return 1;
}

int build_arg_list_from_ast_list (struct arg_value *o, struct NCDConfig_list *list)
{
    arg_value_init_list(o);
    
    for (struct NCDConfig_list *c = list; c; c = c->next) {
        struct arg_value e;
        
        if (!build_arg_from_ast(&e, c)) {
            goto fail;
        }
        
        if (!arg_value_list_append(o, e)) {
            arg_value_free(&e);
            goto fail;
        }
    }
    
    return 1;
    
fail:
    arg_value_free(o);
    return 0;
}

int build_arg_map_from_ast_list (struct arg_value *o, struct NCDConfig_list *list)
{
    arg_value_init_map(o);
    
    for (struct NCDConfig_list *c = list; c; c = c->next->next) {
        ASSERT(c->next)
        
        struct arg_value key;
        struct arg_value val;
        
        if (!build_arg_from_ast(&key, c)) {
            goto fail;
        }
        
        if (!build_arg_from_ast(&val, c->next)) {
            arg_value_free(&key);
            goto fail;
        }
        
        if (!arg_value_map_append(o, key, val)) {
            arg_value_free(&key);
            arg_value_free(&val);
            goto fail;
        }
    }
    
    return 1;
    
fail:
    arg_value_free(o);
    return 0;
}

static char ** names_new (struct NCDConfig_strings *ast_names)
{
    ASSERT(ast_names)
    
    bsize_t size = bsize_fromsize(1);
    for (struct NCDConfig_strings *n = ast_names; n; n = n->next) {
        size = bsize_add(size, bsize_fromsize(1));
    }
    
    char **names;
    if (size.is_overflow || !(names = BAllocArray(size.value, sizeof(names[0])))) {
        BLog(BLOG_ERROR, "BAllocArray failed");
        goto fail0;
    }
    
    size_t i = 0;
    for (struct NCDConfig_strings *n = ast_names; n; n = n->next) {
        if (!(names[i] = strdup(n->value))) {
            BLog(BLOG_ERROR, "strdup failed");
            goto fail1;
        }
        i++;
    }
    
    names[i] = NULL;
    
    return names;
    
fail1:
    while (i-- > 0) {
        free(names[i]);
    }
    BFree(names);
fail0:
    return NULL;
}

static size_t names_count (char **names)
{
    ASSERT(names)
    
    size_t i;
    for (i = 0; names[i]; i++);
    
    return i;
}

static char * names_tostring (char **names)
{
    ASSERT(names)
    
    ExpString str;
    if (!ExpString_Init(&str)) {
        goto fail0;
    }
    
    for (size_t i = 0; names[i]; i++) {
        if (i > 0 && !ExpString_AppendChar(&str, '.')) {
            goto fail1;
        }
        if (!ExpString_Append(&str, names[i])) {
            goto fail1;
        }
    }
    
    return ExpString_Get(&str);
    
fail1:
    ExpString_Free(&str);
fail0:
    return NULL;
}

static void names_free (char **names)
{
    ASSERT(names)
    
    size_t i = names_count(names);
    
    while (i-- > 0) {
        free(names[i]);
    }
    
    BFree(names);
}

int statement_init (struct statement *s, struct NCDConfig_statements *conf)
{
    // set object names
    if (conf->objname) {
        if (!(s->object_names = names_new(conf->objname))) {
            goto fail0;
        }
    } else {
        s->object_names = NULL;
    }
    
    // set method name
    if (!(s->method_name = NCDConfig_concat_strings(conf->names))) {
        BLog(BLOG_ERROR, "NCDConfig_concat_strings failed");
        goto fail1;
    }
    
    // init name
    if (conf->name) {
        if (!(s->name = strdup(conf->name))) {
            BLog(BLOG_ERROR, "strdup failed");
            goto fail2;
        }
    } else {
        s->name = NULL;
    }
    
    // init arguments
    if (!build_arg_list_from_ast_list(&s->args, conf->args)) {
        BLog(BLOG_ERROR, "build_arg_list_from_ast_list failed");
        goto fail3;
    }
    
    return 1;
    
fail3:
    free(s->name);
fail2:
    free(s->method_name);
fail1:
    if (s->object_names) {
        names_free(s->object_names);
    }
fail0:
    return 0;
}

void statement_free (struct statement *s)
{
    // free arguments
    arg_value_free(&s->args);
    
    // free name
    free(s->name);
    
    // free method name
    free(s->method_name);
    
    // free object names
    if (s->object_names) {
        names_free(s->object_names);
    }
}

int process_new (struct NCDConfig_processes *conf, NCDModuleProcess *module_process)
{
    // allocate strucure
    struct process *p = malloc(sizeof(*p));
    if (!p) {
        BLog(BLOG_ERROR, "malloc failed");
        goto fail0;
    }
    
    // set module process
    p->module_process = module_process;
    
    // set module process handlers
    if (p->module_process) {
        NCDModuleProcess_Interp_SetHandlers(p->module_process, p,
                                            (NCDModuleProcess_interp_func_event)process_moduleprocess_func_event,
                                            (NCDModuleProcess_interp_func_getobj)process_moduleprocess_func_getobj);
    }
    
    // init name
    if (!(p->name = strdup(conf->name))) {
        BLog(BLOG_ERROR, "strdup failed");
        goto fail1;
    }
    
    // count statements
    size_t num_st = 0;
    struct NCDConfig_statements *st = conf->statements;
    while (st) {
        if (num_st == SIZE_MAX) {
            BLog(BLOG_ERROR, "too many statements");
            goto fail2;
        }
        num_st++;
        st = st->next;
    }
    
    // allocate statements array
    if (!(p->statements = BAllocArray(num_st, sizeof(p->statements[0])))) {
        goto fail2;
    }
    p->num_statements = 0;
    
    // init statements
    st = conf->statements;
    while (st) {
        struct process_statement *ps = &p->statements[p->num_statements];
        
        ps->p = p;
        ps->i = p->num_statements;
        
        if (!statement_init(&ps->s, st)) {
            goto fail3;
        }
        
        ps->state = SSTATE_FORGOTTEN;
        
        ps->have_error = 0;
        
        p->num_statements++;
        
        st = st->next;
    }
    
    // set state working
    p->state = PSTATE_WORKING;
    
    // set AP=0
    p->ap = 0;
    
    // set FP=0
    p->fp = 0;
    
    // init timer
    BTimer_Init(&p->wait_timer, 0, (BTimer_handler)process_wait_timer_handler, p);
    
    // init advance job
    BPending_Init(&p->advance_job, BReactor_PendingGroup(&ss), (BPending_handler)process_advance_job_handler, p);
    
    // init work job
    BPending_Init(&p->work_job, BReactor_PendingGroup(&ss), (BPending_handler)process_work_job_handler, p);
    
    // insert to processes list
    LinkedList2_Append(&processes, &p->list_node);
    
    // schedule work
    BPending_Set(&p->work_job);
    
    return 1;
    
fail3:
    process_free_statements(p);
fail2:
    free(p->name);
fail1:
    free(p);
fail0:
    BLog(BLOG_ERROR, "failed to initialize process %s", conf->name);
    return 0;
}

void process_free (struct process *p)
{
    ASSERT(p->ap == 0)
    ASSERT(p->fp == 0)
    ASSERT(p->state == PSTATE_TERMINATING)
    
    // inform module process that the process is terminated
    if (p->module_process) {
        NCDModuleProcess_Interp_Terminated(p->module_process);
    }
    
    // remove from processes list
    LinkedList2_Remove(&processes, &p->list_node);
    
    // free work job
    BPending_Free(&p->work_job);
    
    // free advance job
    BPending_Free(&p->advance_job);
    
    // free timer
    BReactor_RemoveTimer(&ss, &p->wait_timer);
    
    // free statements
    process_free_statements(p);
    
    // free name
    free(p->name);
    
    // free strucure
    free(p);
}

void process_start_terminating (struct process *p)
{
    ASSERT(p->state != PSTATE_TERMINATING)
    
    // set terminating
    p->state = PSTATE_TERMINATING;
    
    // schedule work
    process_schedule_work(p);
}

size_t process_rap (struct process *p)
{
    if (p->ap > 0 && p->statements[p->ap - 1].state == SSTATE_CHILD) {
        return (p->ap - 1);
    } else {
        return p->ap;
    }
}

void process_free_statements (struct process *p)
{
    // free statments
    while (p->num_statements > 0) {
        statement_free(&p->statements[p->num_statements - 1].s);
        p->num_statements--;
    }
    
    // free stataments array
    free(p->statements);
}

void process_assert_pointers (struct process *p)
{
    ASSERT(p->ap <= p->num_statements)
    ASSERT(p->fp >= p->ap)
    ASSERT(p->fp <= p->num_statements)
    
#ifndef NDEBUG
    // check AP
    for (size_t i = 0; i < p->ap; i++) {
        if (i == p->ap - 1) {
            ASSERT(p->statements[i].state == SSTATE_ADULT || p->statements[i].state == SSTATE_CHILD)
        } else {
            ASSERT(p->statements[i].state == SSTATE_ADULT)
        }
    }
    
    // check FP
    size_t fp = p->num_statements;
    while (fp > 0 && p->statements[fp - 1].state == SSTATE_FORGOTTEN) {
        fp--;
    }
    ASSERT(p->fp == fp)
#endif
}

void process_logfunc (struct process *p)
{
    BLog_Append("process %s: ", p->name);
}

void process_log (struct process *p, int level, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    BLog_LogViaFuncVarArg((BLog_logfunc)process_logfunc, p, BLOG_CURRENT_CHANNEL, level, fmt, vl);
    va_end(vl);
}

void process_schedule_work (struct process *p)
{
    process_assert_pointers(p);
    
    // stop timer
    BReactor_RemoveTimer(&ss, &p->wait_timer);
    
    // stop advance job
    BPending_Unset(&p->advance_job);
    
    // schedule work
    BPending_Set(&p->work_job);
}

void process_work_job_handler (struct process *p)
{
    process_assert_pointers(p);
    ASSERT(!BTimer_IsRunning(&p->wait_timer))
    ASSERT(!BPending_IsSet(&p->advance_job))
    
    if (p->state == PSTATE_WAITING) {
        return;
    }
    
    if (p->state == PSTATE_TERMINATING) {
        if (p->fp == 0) {
            // finished retreating
            process_free(p);
            
            // if program is terminating amd there are no more processes, exit program
            if (terminating && LinkedList2_IsEmpty(&processes)) {
                BReactor_Quit(&ss, 0);
            }
            return;
        }
        
        // order the last living statement to die, if needed
        struct process_statement *ps = &p->statements[p->fp - 1];
        ASSERT(ps->state != SSTATE_FORGOTTEN)
        if (ps->state != SSTATE_DYING) {
            process_statement_log(ps, BLOG_INFO, "killing");
            
            // order it to die
            NCDModuleInst_Die(&ps->inst);
            
            // set statement state DYING
            ps->state = SSTATE_DYING;
            
            // update AP
            if (p->ap > ps->i) {
                p->ap = ps->i;
            }
        }
        return;
    }
    
    // process was up but is no longer?
    if (p->state == PSTATE_UP && !(p->ap == process_rap(p) && p->ap == p->num_statements)) {
        // if we have module process, wait for its permission to continue
        if (p->module_process) {
            // set module process down
            NCDModuleProcess_Interp_Down(p->module_process);
            
            // set state waiting
            p->state = PSTATE_WAITING;
            return;
        }
        
        // set state working
        p->state = PSTATE_WORKING;
    }
    
    // cleaning up?
    if (p->ap < p->fp) {
        // order the last living statement to die, if needed
        struct process_statement *ps = &p->statements[p->fp - 1];
        if (ps->state != SSTATE_DYING) {
            process_statement_log(ps, BLOG_INFO, "killing");
            
            // order it to die
            NCDModuleInst_Die(&ps->inst);
            
            // set statement state DYING
            ps->state = SSTATE_DYING;
        }
        return;
    }
    
    // clean?
    if (p->ap > process_rap(p)) {
        ASSERT(p->ap > 0)
        ASSERT(p->ap <= p->num_statements)
        
        struct process_statement *ps = &p->statements[p->ap - 1];
        ASSERT(ps->state == SSTATE_CHILD)
        
        process_statement_log(ps, BLOG_INFO, "clean");
        
        // report clean
        NCDModuleInst_Clean(&ps->inst);
        return;
    }
    
    // advancing?
    if (p->ap < p->num_statements) {
        ASSERT(p->state == PSTATE_WORKING)
        struct process_statement *ps = &p->statements[p->ap];
        ASSERT(ps->state == SSTATE_FORGOTTEN)
        
        // clear expired error
        if (ps->have_error && ps->error_until <= btime_gettime()) {
            ps->have_error = 0;
        }
        
        if (ps->have_error) {
            process_statement_log(ps, BLOG_INFO, "waiting after error");
            
            // set wait timer
            BReactor_SetTimerAbsolute(&ss, &p->wait_timer, ps->error_until);
        } else {
            // schedule advance
            BPending_Set(&p->advance_job);
        }
        return;
    }
    
    // have we just finished?
    if (p->state == PSTATE_WORKING) {
        process_log(p, BLOG_INFO, "victory");
        
        // set module process up
        if (p->module_process) {
            NCDModuleProcess_Interp_Up(p->module_process);
        }
        
        // set state up
        p->state = PSTATE_UP;
    }
}

void process_advance_job_handler (struct process *p)
{
    process_assert_pointers(p);
    ASSERT(p->ap == p->fp)
    ASSERT(p->ap == process_rap(p))
    ASSERT(p->ap < p->num_statements)
    ASSERT(!p->statements[p->ap].have_error)
    ASSERT(!BPending_IsSet(&p->work_job))
    ASSERT(!BTimer_IsRunning(&p->wait_timer))
    ASSERT(p->state == PSTATE_WORKING)
    
    struct process_statement *ps = &p->statements[p->ap];
    ASSERT(ps->state == SSTATE_FORGOTTEN)
    
    process_statement_log(ps, BLOG_INFO, "initializing");
    
    NCDObject object;
    NCDObject *object_ptr = NULL;
    char *type;
    int free_type = 0;
    
    if (!ps->s.object_names) {
        // this is a function_call(); type is "function_call"
        type = ps->s.method_name;
    } else {
        // this is a some.object.somewhere->method_call(); type is "base_type(some.object.somewhere)::method_call"
        
        // get object
        if (!process_resolve_object_expr(p, p->ap, ps->s.object_names, &object)) {
            goto fail;
        }
        object_ptr = &object;
        
        // get object type
        const char *object_type = NCDObject_Type(&object);
        if (!object_type) {
            process_statement_log(ps, BLOG_ERROR, "cannot call method on object with no type");
            goto fail;
        }
        
        // build type string
        if (!(type = concat_strings(3, object_type, "::", ps->s.method_name))) {
            process_statement_log(ps, BLOG_ERROR, "concat_strings failed");
            goto fail;
        }
        free_type = 1;
    }
    
    // find module to instantiate
    if (!(ps->module = NCDModuleIndex_FindModule(&mindex, type))) {
        process_statement_log(ps, BLOG_ERROR, "failed to find module: %s", type);
        goto fail;
    }
    
    // resolve arguments
    if (!process_statement_resolve_argument(ps, &ps->s.args, &ps->inst_args)) {
        process_statement_log(ps, BLOG_ERROR, "failed to resolve arguments");
        goto fail;
    }
    
    // initialize module instance
    NCDModuleInst_Init(&ps->inst, ps->module, object_ptr, &ps->inst_args, ps, &module_params);
    
    // set statement state CHILD
    ps->state = SSTATE_CHILD;
    
    // increment AP
    p->ap++;
    
    // increment FP
    p->fp++;
    
    if (free_type) {
        free(type);
    }
    
    process_assert_pointers(p);
    return;
    
fail:
    if (free_type) {
        free(type);
    }
    
    // mark error
    process_statement_set_error(ps);
    
    // schedule work to start the timer
    process_schedule_work(p);
}

void process_wait_timer_handler (struct process *p)
{
    process_assert_pointers(p);
    ASSERT(p->ap == p->fp)
    ASSERT(p->ap == process_rap(p))
    ASSERT(p->ap < p->num_statements)
    ASSERT(p->statements[p->ap].have_error)
    ASSERT(!BPending_IsSet(&p->work_job))
    ASSERT(!BPending_IsSet(&p->advance_job))
    ASSERT(p->state == PSTATE_WORKING)
    
    process_log(p, BLOG_INFO, "retrying");
    
    // clear error
    p->statements[p->ap].have_error = 0;
    
    // schedule work
    BPending_Set(&p->work_job);
}

int process_find_object (struct process *p, size_t pos, const char *name, NCDObject *out_object)
{
    ASSERT(pos <= p->num_statements)
    ASSERT(name)
    ASSERT(out_object)
    
    for (size_t i = pos; i > 0; i--) {
        struct process_statement *ps = &p->statements[i - 1];
        if (ps->s.name && !strcmp(ps->s.name, name)) {
            if (ps->state == SSTATE_FORGOTTEN) {
                process_log(p, BLOG_ERROR, "statement (%zu) is uninitialized", i - 1);
                goto fail;
            }
            
            *out_object = NCDModuleInst_Object(&ps->inst);
            return 1;
        }
    }
    
    if (p->module_process && NCDModuleProcess_Interp_GetSpecialObj(p->module_process, name, out_object)) {
        return 1;
    }
    
fail:
    return 0;
}

int process_resolve_object_expr (struct process *p, size_t pos, char **names, NCDObject *out_object)
{
    ASSERT(pos <= p->num_statements)
    ASSERT(names)
    ASSERT(names_count(names) > 0)
    ASSERT(out_object)
    
    NCDObject object;
    if (!process_find_object(p, pos, names[0], &object)) {
        goto fail;
    }
    
    if (!NCDObject_ResolveObjExpr(&object, names + 1, out_object)) {
        goto fail;
    }
    
    return 1;
    
fail:;
    char *name = names_tostring(names);
    process_log(p, BLOG_ERROR, "failed to resolve object (%s) from position %zu", (name ? name : ""), pos);
    free(name);
    return 0;
}

int process_resolve_variable_expr (struct process *p, size_t pos, char **names, NCDValue *out_value)
{
    ASSERT(pos <= p->num_statements)
    ASSERT(names)
    ASSERT(names_count(names) > 0)
    ASSERT(out_value)
    
    NCDObject object;
    if (!process_find_object(p, pos, names[0], &object)) {
        goto fail;
    }
    
    if (!NCDObject_ResolveVarExpr(&object, names + 1, out_value)) {
        goto fail;
    }
    
    return 1;
    
fail:;
    char *name = names_tostring(names);
    process_log(p, BLOG_ERROR, "failed to resolve variable (%s) from position %zu", (name ? name : ""), pos);
    free(name);
    return 0;
}

void process_statement_logfunc (struct process_statement *ps)
{
    process_logfunc(ps->p);
    BLog_Append("statement %zu: ", ps->i);
}

void process_statement_log (struct process_statement *ps, int level, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    BLog_LogViaFuncVarArg((BLog_logfunc)process_statement_logfunc, ps, BLOG_CURRENT_CHANNEL, level, fmt, vl);
    va_end(vl);
}

void process_statement_set_error (struct process_statement *ps)
{
    ASSERT(ps->state == SSTATE_FORGOTTEN)
    
    ps->have_error = 1;
    ps->error_until = btime_add(btime_gettime(), options.retry_time);
}

int process_statement_resolve_argument (struct process_statement *ps, struct arg_value *arg, NCDValue *out)
{
    ASSERT(ps->i <= process_rap(ps->p))
    ASSERT(arg)
    ASSERT(out)
    
    switch (arg->type) {
        case ARG_VALUE_TYPE_STRING: {
            if (!NCDValue_InitString(out, arg->string)) {
                process_statement_log(ps, BLOG_ERROR, "NCDValue_InitString failed");
                return 0;
            }
        } break;
        
        case ARG_VALUE_TYPE_VARIABLE: {
            if (!process_resolve_variable_expr(ps->p, ps->i, arg->variable_names, out)) {
                return 0;
            }
        } break;
        
        case ARG_VALUE_TYPE_LIST: do {
            NCDValue_InitList(out);
            
            for (LinkedList1Node *n = LinkedList1_GetFirst(&arg->list); n; n = LinkedList1Node_Next(n)) {
                struct arg_list_elem *elem = UPPER_OBJECT(n, struct arg_list_elem, list_node);
                
                NCDValue v;
                if (!process_statement_resolve_argument(ps, &elem->value, &v)) {
                    goto list_fail1;
                }
                
                if (!NCDValue_ListAppend(out, v)) {
                    process_statement_log(ps, BLOG_ERROR, "NCDValue_ListAppend failed");
                    NCDValue_Free(&v);
                    goto list_fail1;
                }
            }
            
            break;
            
        list_fail1:
            NCDValue_Free(out);
            return 0;
        } while (0); break;
        
        case ARG_VALUE_TYPE_MAP: do {
            NCDValue_InitMap(out);
            
            for (LinkedList1Node *n = LinkedList1_GetFirst(&arg->maplist); n; n = LinkedList1Node_Next(n)) {
                struct arg_map_elem *elem = UPPER_OBJECT(n, struct arg_map_elem, maplist_node);
                
                NCDValue key;
                NCDValue val;
                
                if (!process_statement_resolve_argument(ps, &elem->key, &key)) {
                    goto map_fail;
                }
                
                if (!process_statement_resolve_argument(ps, &elem->val, &val)) {
                    NCDValue_Free(&key);
                    goto map_fail;
                }
                
                if (NCDValue_MapFindKey(out, &key)) {
                    process_statement_log(ps, BLOG_ERROR, "duplicate map keys");
                    NCDValue_Free(&key);
                    NCDValue_Free(&val);
                    goto map_fail;
                }
                
                if (!NCDValue_MapInsert(out, key, val)) {
                    process_statement_log(ps, BLOG_ERROR, "NCDValue_MapInsert failed");
                    NCDValue_Free(&key);
                    NCDValue_Free(&val);
                    goto map_fail;
                }
            }
            
            break;
            
        map_fail:
            NCDValue_Free(out);
            return 0;
        } while (0); break;
        
        default: ASSERT(0);
    }
    
    return 1;
}

void process_statement_instance_func_event (struct process_statement *ps, int event)
{
    ASSERT(ps->state == SSTATE_CHILD || ps->state == SSTATE_ADULT || ps->state == SSTATE_DYING)
    
    struct process *p = ps->p;
    process_assert_pointers(p);
    
    // schedule work
    process_schedule_work(p);
    
    switch (event) {
        case NCDMODULE_EVENT_UP: {
            ASSERT(ps->state == SSTATE_CHILD)
            
            process_statement_log(ps, BLOG_INFO, "up");
            
            // set state ADULT
            ps->state = SSTATE_ADULT;
        } break;
        
        case NCDMODULE_EVENT_DOWN: {
            ASSERT(ps->state == SSTATE_ADULT)
            
            process_statement_log(ps, BLOG_INFO, "down");
            
            // set state CHILD
            ps->state = SSTATE_CHILD;
            
            // update AP
            if (p->ap > ps->i + 1) {
                p->ap = ps->i + 1;
            }
        } break;
        
        case NCDMODULE_EVENT_DEAD: {
            int is_error = NCDModuleInst_HaveError(&ps->inst);
            
            if (is_error) {
                process_statement_log(ps, BLOG_ERROR, "died with error");
            } else {
                process_statement_log(ps, BLOG_INFO, "died");
            }
            
            // free instance
            NCDModuleInst_Free(&ps->inst);
            
            // free instance arguments
            NCDValue_Free(&ps->inst_args);
            
            // set state FORGOTTEN
            ps->state = SSTATE_FORGOTTEN;
            
            // set error
            if (is_error) {
                process_statement_set_error(ps);
            }
            
            // update AP
            if (p->ap > ps->i) {
                p->ap = ps->i;
            }
            
            // update FP
            while (p->fp > 0 && p->statements[p->fp - 1].state == SSTATE_FORGOTTEN) {
                p->fp--;
            }
        } break;
    }
}

int process_statement_instance_func_getobj (struct process_statement *ps, const char *objname, NCDObject *out_object)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    return process_find_object(ps->p, ps->i, objname, out_object);
}

int process_statement_instance_func_initprocess (struct process_statement *ps, NCDModuleProcess *mp, const char *template_name)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    // find template
    struct NCDConfig_processes *conf = config_ast;
    while (conf) {
        if (conf->is_template && !strcmp(conf->name, template_name)) {
            break;
        }
        conf = conf->next;
    }
    
    if (!conf) {
        process_statement_log(ps, BLOG_ERROR, "no template named %s", template_name);
        return 0;
    }
    
    // create process
    if (!process_new(conf, mp)) {
        process_statement_log(ps, BLOG_ERROR, "failed to create process from template %s", template_name);
        return 0;
    }
    
    process_statement_log(ps, BLOG_INFO, "created process from template %s", template_name);
    
    return 1;
}

void process_statement_instance_logfunc (struct process_statement *ps)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    process_statement_logfunc(ps);
    BLog_Append("module: ");
}

void process_statement_instance_func_interp_exit (struct process_statement *ps, int exit_code)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    start_terminate(exit_code);
}

int process_statement_instance_func_interp_getargs (struct process_statement *ps, NCDValue *out_value)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    NCDValue_InitList(out_value);
    
    for (int i = 0; i < options.num_extra_args; i++) {
        NCDValue arg;
        if (!NCDValue_InitString(&arg, options.extra_args[i])) {
             process_statement_log(ps, BLOG_ERROR, "NCDValue_InitString failed");
             goto fail1;
        }
        
        if (!NCDValue_ListAppend(out_value, arg)) {
            process_statement_log(ps, BLOG_ERROR, "NCDValue_ListAppend failed");
            NCDValue_Free(&arg);
            goto fail1;
        }
    }
    
    return 1;
    
fail1:
    NCDValue_Free(out_value);
    return 0;
}

void process_moduleprocess_func_event (struct process *p, int event)
{
    ASSERT(p->module_process)
    
    switch (event) {
        case NCDMODULEPROCESS_INTERP_EVENT_CONTINUE: {
            ASSERT(p->state == PSTATE_WAITING)
            
            // set state working
            p->state = PSTATE_WORKING;
            
            // schedule work
            process_schedule_work(p);
        } break;
        
        case NCDMODULEPROCESS_INTERP_EVENT_TERMINATE: {
            ASSERT(p->state != PSTATE_TERMINATING)
            
            process_log(p, BLOG_INFO, "process termination requested");
        
            // start terminating
            process_start_terminating(p);
        } break;
        
        default: ASSERT(0);
    }
}

int process_moduleprocess_func_getobj (struct process *p, const char *name, NCDObject *out_object)
{
    ASSERT(p->module_process)
    
    return process_find_object(p, p->num_statements, name, out_object);
}
