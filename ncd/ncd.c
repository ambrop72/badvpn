/**
 * @file ncd.c
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
        char *variable;
        LinkedList1 list;
    };
};

struct arg_list_elem {
    LinkedList1Node list_node;
    struct arg_value value;
};

struct statement {
    char *object_name;
    char *method_name;
    struct arg_value args;
    char *name;
};

struct process {
    NCDModuleProcess *module_process;
    NCDValue args;
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
} options;

// reactor
BReactor ss;

// are we terminating
int terminating;

// process manager
BProcessManager manager;

// udev manager
NCDUdevManager umanager;

// config AST
struct NCDConfig_processes *config_ast;

// processes
LinkedList2 processes;

static void print_help (const char *name);
static void print_version (void);
static int parse_arguments (int argc, char *argv[]);
static void signal_handler (void *unused);
static const struct NCDModule * find_module (const char *name);
static int arg_value_init_string (struct arg_value *o, const char *string);
static int arg_value_init_variable (struct arg_value *o, const char *variable);
static void arg_value_init_list (struct arg_value *o);
static int arg_value_list_append (struct arg_value *o, struct arg_value v);
static void arg_value_free (struct arg_value *o);
static int build_arg_list_from_ast_list (struct arg_value *o, struct NCDConfig_list *list);
static int statement_init (struct statement *s, struct NCDConfig_statements *conf);
static void statement_free (struct statement *s);
static int process_new (struct NCDConfig_processes *conf, NCDModuleProcess *module_process, NCDValue args);
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
static struct process_statement * process_find_statement (struct process *p, size_t pos, const char *name);
static int process_resolve_name (struct process *p, size_t pos, const char *name, struct process_statement **first_ps, const char **rest);
static int process_resolve_variable (struct process *p, size_t pos, const char *varname, NCDValue *out);
static struct process_statement * process_resolve_object (struct process *p, size_t pos, const char *objname);
static void process_statement_logfunc (struct process_statement *ps);
static void process_statement_log (struct process_statement *ps, int level, const char *fmt, ...);
static void process_statement_set_error (struct process_statement *ps);
static int process_statement_resolve_argument (struct process_statement *ps, struct arg_value *arg, NCDValue *out);
static void process_statement_instance_func_event (struct process_statement *ps, int event);
static int process_statement_instance_func_getvar (struct process_statement *ps, const char *varname, NCDValue *out);
static NCDModuleInst * process_statement_instance_func_getobj (struct process_statement *ps, const char *objname);
static int process_statement_instance_func_initprocess (struct process_statement *ps, NCDModuleProcess *mp, const char *template_name, NCDValue args);
static void process_statement_instance_logfunc (struct process_statement *ps);
static void process_moduleprocess_func_event (struct process *p, int event);
static int process_moduleprocess_func_getvar (struct process *p, const char *name, NCDValue *out);
static NCDModuleInst * process_moduleprocess_func_getobj (struct process *p, const char *name);

int main (int argc, char **argv)
{
    if (argc <= 0) {
        return 1;
    }
    
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
    NCDUdevManager_Init(&umanager, &ss, &manager);
    
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
    
    // init processes list
    LinkedList2_Init(&processes);
    
    // init processes
    struct NCDConfig_processes *conf = config_ast;
    while (conf) {
        if (!conf->is_template) {
            NCDValue args;
            NCDValue_InitList(&args);
            if (!process_new(conf, NULL, args)) {
                NCDValue_Free(&args);
            }
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
    
    return 1;
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
        "        [--retry-time <ms>]\n",
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
    
    if (terminating) {
        return;
    }
    
    terminating = 1;
    
    if (LinkedList2_IsEmpty(&processes)) {
        BReactor_Quit(&ss, 1);
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

const struct NCDModule * find_module (const char *name)
{
    for (const struct NCDModuleGroup **g = ncd_modules; *g; g++) {
        for (const struct NCDModule *m = (*g)->modules; m->type; m++) {
            if (!strcmp(m->type, name)) {
                return m;
            }
        }
    }
    
    return NULL;
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

int arg_value_init_variable (struct arg_value *o, const char *variable)
{
    o->type = ARG_VALUE_TYPE_VARIABLE;
    if (!(o->variable = strdup(variable))) {
        BLog(BLOG_ERROR, "strdup failed");
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

void arg_value_free (struct arg_value *o)
{
    switch (o->type) {
        case ARG_VALUE_TYPE_STRING: {
            free(o->string);
        } break;
        
        case ARG_VALUE_TYPE_VARIABLE: {
            free(o->variable);
        } break;
        
        case ARG_VALUE_TYPE_LIST: {
            while (!LinkedList1_IsEmpty(&o->list)) {
                struct arg_list_elem *elem = UPPER_OBJECT(LinkedList1_GetFirst(&o->list), struct arg_list_elem, list_node);
                arg_value_free(&elem->value);
                LinkedList1_Remove(&o->list, &elem->list_node);
                free(elem);
            }
        } break;
        
        default: ASSERT(0);
    }
}

int build_arg_list_from_ast_list (struct arg_value *o, struct NCDConfig_list *list)
{
    arg_value_init_list(o);
    
    for (struct NCDConfig_list *c = list; c; c = c->next) {
        struct arg_value e;
        
        switch (c->type) {
            case NCDCONFIG_ARG_STRING: {
                if (!arg_value_init_string(&e, c->string)) {
                    goto fail;
                }
            } break;
            
            case NCDCONFIG_ARG_VAR: {
                char *variable = NCDConfig_concat_strings(c->var);
                if (!variable) {
                    BLog(BLOG_ERROR, "NCDConfig_concat_strings failed");
                    goto fail;
                }
                
                if (!arg_value_init_variable(&e, variable)) {
                    free(variable);
                    goto fail;
                }
                
                free(variable);
            } break;
            
            case NCDCONFIG_ARG_LIST: {
                if (!build_arg_list_from_ast_list(&e, c->list)) {
                    goto fail;
                }
            } break;
            
            default: ASSERT(0);
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

int statement_init (struct statement *s, struct NCDConfig_statements *conf)
{
    s->object_name = NULL;
    s->method_name = NULL;
    s->name = NULL;
    
    // set object name
    if (conf->objname) {
        if (!(s->object_name = NCDConfig_concat_strings(conf->objname))) {
            BLog(BLOG_ERROR, "NCDConfig_concat_strings failed");
            goto fail1;
        }
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
            goto fail1;
        }
    }
    
    // init arguments
    if (!build_arg_list_from_ast_list(&s->args, conf->args)) {
        BLog(BLOG_ERROR, "build_arg_list_from_ast_list failed");
        goto fail1;
    }
    
    return 1;
    
fail1:
    free(s->name);
    free(s->method_name);
    free(s->object_name);
    return 0;
}

void statement_free (struct statement *s)
{
    // free arguments
    arg_value_free(&s->args);
    
    // free names
    free(s->name);
    free(s->method_name);
    free(s->object_name);
}

int process_new (struct NCDConfig_processes *conf, NCDModuleProcess *module_process, NCDValue args)
{
    ASSERT(NCDValue_Type(&args) == NCDVALUE_LIST)
    
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
                                            (NCDModuleProcess_interp_func_getvar)process_moduleprocess_func_getvar,
                                            (NCDModuleProcess_interp_func_getobj)process_moduleprocess_func_getobj);
    }
    
    // set arguments
    p->args = args;
    
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
    
    // free arguments
    NCDValue_Free(&p->args);
    
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
                BReactor_Quit(&ss, 1);
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
    
    NCDModuleInst *method_object = NULL;
    char *type;
    
    // construct type
    if (!ps->s.object_name) {
        // this is a function_call(); type is "function_call"
        if (!(type = strdup(ps->s.method_name))) {
            process_statement_log(ps, BLOG_ERROR, "strdup failed");
            goto fail0;
        }
    } else {
        // this is a some.object.somewhere->method_call(); type is "base_type(some.object.somewhere)::method_call"
        
        // resolve object
        struct process_statement *obj_ps = process_resolve_object(p, p->ap, ps->s.object_name);
        if (!obj_ps) {
            process_statement_log(ps, BLOG_ERROR, "failed to resolve object %s for method call", ps->s.object_name);
            goto fail0;
        }
        ASSERT(obj_ps->state == SSTATE_ADULT)
        
        // base type defaults to type
        const char *base_type = (obj_ps->module->base_type ? obj_ps->module->base_type : obj_ps->module->type);
        
        // build type string
        if (!(type = concat_strings(3, base_type, "::", ps->s.method_name))) {
            process_statement_log(ps, BLOG_ERROR, "concat_strings failed");
            goto fail0;
        }
        
        method_object = &obj_ps->inst;
    }
    
    // find module to instantiate
    if (!(ps->module = find_module(type))) {
        process_statement_log(ps, BLOG_ERROR, "failed to find module: %s", type);
        goto fail1;
    }
    
    // resolve arguments
    if (!process_statement_resolve_argument(ps, &ps->s.args, &ps->inst_args)) {
        process_statement_log(ps, BLOG_ERROR, "failed to resolve arguments");
        goto fail1;
    }
    
    // initialize module instance
    NCDModuleInst_Init(
        &ps->inst, ps->module, method_object, &ps->inst_args, &ss, &manager, &umanager, ps,
        (NCDModuleInst_func_event)process_statement_instance_func_event,
        (NCDModuleInst_func_getvar)process_statement_instance_func_getvar,
        (NCDModuleInst_func_getobj)process_statement_instance_func_getobj,
        (NCDModuleInst_func_initprocess)process_statement_instance_func_initprocess,
        (BLog_logfunc)process_statement_instance_logfunc
    );
    
    // set statement state CHILD
    ps->state = SSTATE_CHILD;
    
    // increment AP
    p->ap++;
    
    // increment FP
    p->fp++;
    
    free(type);
    
    process_assert_pointers(p);
    return;
    
fail1:
    free(type);
fail0:
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

struct process_statement * process_find_statement (struct process *p, size_t pos, const char *name)
{
    process_assert_pointers(p);
    ASSERT(pos >= 0)
    ASSERT(pos <= process_rap(p))
    
    for (size_t i = pos; i > 0; i--) {
        struct process_statement *ps = &p->statements[i - 1];
        if (ps->s.name && !strcmp(ps->s.name, name)) {
            return ps;
        }
    }
    
    return NULL;
}

int process_resolve_name (struct process *p, size_t pos, const char *name, struct process_statement **first_ps, const char **rest)
{
    process_assert_pointers(p);
    ASSERT(pos >= 0)
    ASSERT(pos <= process_rap(p))
    ASSERT(name)
    
    char *dot = strstr(name, ".");
    if (!dot) {
        *first_ps = process_find_statement(p, pos, name);
        *rest = NULL;
    } else {
        // copy modname and terminate
        char *modname = malloc((dot - name) + 1);
        if (!modname) {
            process_log(p, BLOG_ERROR, "malloc failed");
            return 0;
        }
        memcpy(modname, name, dot - name);
        modname[dot - name] = '\0';
        
        *first_ps = process_find_statement(p, pos, modname);
        *rest = dot + 1;
        
        free(modname);
    }
    
    return 1;
}

int process_resolve_variable (struct process *p, size_t pos, const char *varname, NCDValue *out)
{
    process_assert_pointers(p);
    ASSERT(pos >= 0)
    ASSERT(pos <= process_rap(p))
    ASSERT(varname)
    
    // find referred statement and remaining name
    struct process_statement *rps;
    const char *rest;
    if (!process_resolve_name(p, pos, varname, &rps, &rest)) {
        return 0;
    }
    
    if (!rps) {
        // handle _args
        if (!strcmp(varname, "_args")) {
            if (!NCDValue_InitCopy(out, &p->args)) {
                process_log(p, BLOG_ERROR, "NCDValue_InitCopy failed");
                return 0;
            }
            
            return 1;
        }
        
        // handle _argN
        size_t len;
        uintmax_t n;
        if ((len = string_begins_with(varname, "_arg")) && parse_unsigned_integer(varname + len, &n) && n < NCDValue_ListCount(&p->args)) {
            if (!NCDValue_InitCopy(out, NCDValue_ListGet(&p->args, n))) {
                process_log(p, BLOG_ERROR, "NCDValue_InitCopy failed");
                return 0;
            }
            
            return 1;
        }
        
        // handle special variables
        if (p->module_process) {
            if (NCDModuleProcess_Interp_GetSpecialVar(p->module_process, varname, out)) {
                return 1;
            }
        }
        
        process_log(p, BLOG_ERROR, "unknown statement name in variable: %s", varname);
        return 0;
    }
    
    ASSERT(rps->state == SSTATE_ADULT)
    
    // resolve variable in referred statement
    if (!NCDModuleInst_GetVar(&rps->inst, (rest ? rest : ""), out)) {
        process_log(p, BLOG_ERROR, "referred module failed to resolve variable: %s", varname);
        return 0;
    }
    
    return 1;
}

struct process_statement * process_resolve_object (struct process *p, size_t pos, const char *objname)
{
    process_assert_pointers(p);
    ASSERT(pos >= 0)
    ASSERT(pos <= process_rap(p))
    ASSERT(objname)
    
    // find referred statement and remaining name
    struct process_statement *rps;
    const char *rest;
    if (!process_resolve_name(p, pos, objname, &rps, &rest)) {
        return NULL;
    }
    
    if (!rps) {
        // handle special objects
        if (p->module_process) {
            NCDModuleInst *inst = NCDModuleProcess_Interp_GetSpecialObj(p->module_process, objname);
            if (inst) {
                struct process_statement *res_ps = UPPER_OBJECT(inst, struct process_statement, inst);
                ASSERT(res_ps->state == SSTATE_ADULT)
                return res_ps;
            }
        }
        
        process_log(p, BLOG_ERROR, "unknown statement name in object: %s", objname);
        return NULL;
    }
    
    ASSERT(rps->state == SSTATE_ADULT)
    
    // Resolve object in referred statement. If there is no rest, resolve empty string
    // instead, or use this statement if it fails. This allows a statement to forward method
    // calls elsewhere.
    NCDModuleInst *inst = NCDModuleInst_GetObj(&rps->inst, (rest ? rest : ""));
    if (!inst) {
        if (!rest) {
            return rps;
        }
        
        process_log(p, BLOG_ERROR, "referred module failed to resolve object: %s", objname);
        return NULL;
    }
    
    struct process_statement *res_ps = UPPER_OBJECT(inst, struct process_statement, inst);
    ASSERT(res_ps->state == SSTATE_ADULT)
    
    return res_ps;
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
    
    switch (arg->type) {
        case ARG_VALUE_TYPE_STRING: {
            if (!NCDValue_InitString(out, arg->string)) {
                process_statement_log(ps, BLOG_ERROR, "NCDValue_InitString failed");
                return 0;
            }
        } break;
        
        case ARG_VALUE_TYPE_VARIABLE: {
            if (!process_resolve_variable(ps->p, ps->i, arg->variable, out)) {
                process_statement_log(ps, BLOG_ERROR, "failed to resolve variable");
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

int process_statement_instance_func_getvar (struct process_statement *ps, const char *varname, NCDValue *out)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    if (ps->i > process_rap(ps->p)) {
        process_statement_log(ps, BLOG_ERROR, "tried to resolve variable %s but it's dirty", varname);
        return 0;
    }
    
    return process_resolve_variable(ps->p, ps->i, varname, out);
}

NCDModuleInst * process_statement_instance_func_getobj (struct process_statement *ps, const char *objname)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    if (ps->i > process_rap(ps->p)) {
        process_statement_log(ps, BLOG_ERROR, "tried to resolve object %s but it's dirty", objname);
        return 0;
    }
    
    struct process_statement *rps = process_resolve_object(ps->p, ps->i, objname);
    if (!rps) {
        return NULL;
    }
    
    return &rps->inst;
}

int process_statement_instance_func_initprocess (struct process_statement *ps, NCDModuleProcess *mp, const char *template_name, NCDValue args)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    ASSERT(NCDValue_Type(&args) == NCDVALUE_LIST)
    
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
    if (!process_new(conf, mp, args)) {
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

int process_moduleprocess_func_getvar (struct process *p, const char *name, NCDValue *out)
{
    ASSERT(p->module_process)
    
    if (p->num_statements > process_rap(p)) {
        process_log(p, BLOG_ERROR, "module process tried to resolve variable %s but it's dirty", name);
        return 0;
    }
    
    return process_resolve_variable(p, p->num_statements, name, out);
}

NCDModuleInst * process_moduleprocess_func_getobj (struct process *p, const char *name)
{
    ASSERT(p->module_process)
    
    if (p->num_statements > process_rap(p)) {
        process_log(p, BLOG_ERROR, "module process tried to resolve object %s but it's dirty", name);
        return NULL;
    }
    
    struct process_statement *rps = process_resolve_object(p, p->num_statements, name);
    if (!rps) {
        return NULL;
    }
    
    return &rps->inst;
}
