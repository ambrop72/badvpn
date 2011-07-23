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
#include <structure/LinkedList2.h>
#include <base/BLog.h>
#include <system/BReactor.h>
#include <system/BSignal.h>
#include <system/BConnection.h>
#include <system/BProcess.h>
#include <udevmonitor/NCDUdevManager.h>
#include <ncdconfig/NCDConfigParser.h>
#include <ncd/NCDModule.h>
#include <ncd/modules/modules.h>

#ifndef BADVPN_USE_WINAPI
#include <base/BLog_syslog.h>
#endif

#include <ncd/ncd.h>

#include <generated/blog_channel_ncd.h>

#define LOGGER_STDOUT 1
#define LOGGER_SYSLOG 2

#define SSTATE_CHILD 1
#define SSTATE_ADULT 2
#define SSTATE_DYING 3
#define SSTATE_FORGOTTEN 4

struct statement {
    char *object_name;
    char *method_name;
    struct argument_elem *first_arg;
    char *name;
};

struct argument_elem {
    int is_var;
    union {
        char *var;
        NCDValue val;
    };
    struct argument_elem *next_arg;
};

struct process {
    NCDModuleProcess *module_process;
    NCDValue args;
    char *name;
    size_t num_statements;
    struct process_statement *statements;
    int terminating;
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
    char *type;
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
struct NCDConfig_interfaces *config_ast;

// processes
LinkedList2 processes;

static void print_help (const char *name);
static void print_version (void);
static int parse_arguments (int argc, char *argv[]);
static void signal_handler (void *unused);
static const struct NCDModule * find_module (const char *name);
static int statement_init (struct statement *s, struct NCDConfig_statements *conf);
static void statement_free (struct statement *s);
static void statement_free_args (struct statement *s);
static int process_new (struct NCDConfig_interfaces *conf, NCDModuleProcess *module_process, NCDValue args);
static void process_free (struct process *p);
static void process_start_teminating (struct process *p);
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
static void process_statement_instance_handler_event (struct process_statement *ps, int event);
static int process_statement_instance_handler_getvar (struct process_statement *ps, const char *varname, NCDValue *out);
static NCDModuleInst * process_statement_instance_handler_getobj (struct process_statement *ps, const char *objname);
static int process_statement_instance_handler_initprocess (struct process_statement *ps, NCDModuleProcess *mp, const char *template_name, NCDValue args);
static void process_statement_instance_logfunc (struct process_statement *ps);
static void process_moduleprocess_handler_die (struct process *p);

int main (int argc, char **argv)
{
    if (argc <= 0) {
        return 1;
    }
    
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
    struct NCDConfig_interfaces *conf = config_ast;
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
    NCDConfig_free_interfaces(config_ast);
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
    
    // start terminating all processes
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &processes);
    LinkedList2Node *n;
    while (n = LinkedList2Iterator_Next(&it)) {
        struct process *p = UPPER_OBJECT(n, struct process, list_node);
        process_start_teminating(p);
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

int statement_init (struct statement *s, struct NCDConfig_statements *conf)
{
    s->object_name = NULL;
    s->method_name = NULL;
    s->name = NULL;
    s->first_arg = NULL;
    
    // set object name
    if (conf->objname) {
        if (!(s->object_name = NCDConfig_concat_strings(conf->objname))) {
            BLog(BLOG_ERROR, "NCDConfig_concat_strings failed");
            goto fail;
        }
    }
    
    // set method name
    if (!(s->method_name = NCDConfig_concat_strings(conf->names))) {
        BLog(BLOG_ERROR, "NCDConfig_concat_strings failed");
        goto fail;
    }
    
    // init name
    if (conf->name) {
        if (!(s->name = strdup(conf->name))) {
            BLog(BLOG_ERROR, "strdup failed");
            goto fail;
        }
    }
    
    // init arguments
    struct argument_elem **prevptr = &s->first_arg;
    struct NCDConfig_arguments *arg = conf->args;
    while (arg) {
        struct argument_elem *e = malloc(sizeof(*e));
        if (!e) {
            BLog(BLOG_ERROR, "malloc failed");
            goto loop_fail0;
        }
        
        switch (arg->type) {
            case NCDCONFIG_ARG_STRING: {
                if (!NCDValue_InitString(&e->val, arg->string)) {
                    BLog(BLOG_ERROR, "NCDValue_InitString failed");
                    goto loop_fail1;
                }
                
                e->is_var = 0;
            } break;
            
            case NCDCONFIG_ARG_VAR: {
                if (!(e->var = NCDConfig_concat_strings(arg->var))) {
                    BLog(BLOG_ERROR, "NCDConfig_concat_strings failed");
                    goto loop_fail1;
                }
                
                e->is_var = 1;
            } break;
            
            default:
                ASSERT(0);
        }
        
        *prevptr = e;
        e->next_arg = NULL;
        prevptr = &e->next_arg;
        
        arg = arg->next;
        continue;
        
    loop_fail1:
        free(e);
    loop_fail0:
        goto fail;
    }
    
    return 1;
    
fail:
    statement_free_args(s);
    free(s->name);
    free(s->method_name);
    free(s->object_name);
    return 0;
}

void statement_free (struct statement *s)
{
    // free arguments
    statement_free_args(s);
    
    // free names
    free(s->name);
    free(s->method_name);
    free(s->object_name);
}

void statement_free_args (struct statement *s)
{
    struct argument_elem *e = s->first_arg;
    while (e) {
        if (e->is_var) {
            free(e->var);
        } else {
            NCDValue_Free(&e->val);
        }
        struct argument_elem *n = e->next_arg;
        free(e);
        e = n;
    }
}

int process_new (struct NCDConfig_interfaces *conf, NCDModuleProcess *module_process, NCDValue args)
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
        NCDModuleProcess_Interp_SetHandlers(p->module_process, p, (NCDModuleProcess_interp_handler_die)process_moduleprocess_handler_die);
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
    
    // set not terminating
    p->terminating = 0;
    
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
    
    // inform module process that the process is dead
    if (p->module_process) {
        NCDModuleProcess_Interp_Dead(p->module_process);
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

void process_start_teminating (struct process *p)
{
    if (p->terminating) {
        return;
    }
    
    // set terminating
    p->terminating = 1;
    
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
    
    if (p->terminating) {
        if (p->fp == 0) {
            // finished retreating
            process_free(p);
            
            // if program is terminating amd there are no more processes, exit program
            if (terminating && LinkedList2_IsEmpty(&processes)) {
                BReactor_Quit(&ss, 1);
            }
        } else {
            // order the last living statement to die, if needed
            struct process_statement *ps = &p->statements[p->fp - 1];
            ASSERT(ps->state != SSTATE_FORGOTTEN)
            if (ps->state != SSTATE_DYING) {
                process_statement_log(ps, BLOG_INFO, "killing");
                
                // order it to die
                NCDModuleInst_Event(&ps->inst, NCDMODULE_TOEVENT_DIE);
                
                // set statement state DYING
                ps->state = SSTATE_DYING;
                
                // update AP
                if (p->ap > ps->i) {
                    p->ap = ps->i;
                }
            }
            
            process_assert_pointers(p);
        }
        
        return;
    }
    
    if (p->ap == p->fp) {
        if (p->ap == process_rap(p)) {
            if (p->ap == p->num_statements) {
                // all statements are up
                process_log(p, BLOG_INFO, "victory");
            } else {
                struct process_statement *ps = &p->statements[p->ap];
                ASSERT(ps->state == SSTATE_FORGOTTEN)
                
                // clear expired error
                if (ps->have_error && ps->error_until <= btime_gettime()) {
                    ps->have_error = 0;
                }
                
                if (ps->have_error) {
                    // next statement has error, wait
                    process_statement_log(ps, BLOG_INFO, "waiting after error");
                    BReactor_SetTimerAbsolute(&ss, &p->wait_timer, ps->error_until);
                } else {
                    // schedule advance
                    BPending_Set(&p->advance_job);
                }
            }
        } else {
            ASSERT(p->ap > 0)
            ASSERT(p->ap <= p->num_statements)
            
            struct process_statement *ps = &p->statements[p->ap - 1];
            ASSERT(ps->state == SSTATE_CHILD)
            
            process_statement_log(ps, BLOG_INFO, "clean");
            
            // report clean
            NCDModuleInst_Event(&ps->inst, NCDMODULE_TOEVENT_CLEAN);
        }
    } else {
        // order the last living statement to die, if needed
        struct process_statement *ps = &p->statements[p->fp - 1];
        if (ps->state != SSTATE_DYING) {
            process_statement_log(ps, BLOG_INFO, "killing");
            
            // order it to die
            NCDModuleInst_Event(&ps->inst, NCDMODULE_TOEVENT_DIE);
            
            // set statement state DYING
            ps->state = SSTATE_DYING;
        }
    }
    
    process_assert_pointers(p);
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
    ASSERT(!p->terminating)
    
    struct process_statement *ps = &p->statements[p->ap];
    ASSERT(ps->state == SSTATE_FORGOTTEN)
    
    process_statement_log(ps, BLOG_INFO, "initializing");
    
    NCDModuleInst *method_object = NULL;
    
    // construct type
    if (!ps->s.object_name) {
        // this is a function_call(); type is "function_call"
        if (!(ps->type = strdup(ps->s.method_name))) {
            process_statement_log(ps, BLOG_ERROR, "strdup failed");
            goto fail0;
        }
    } else {
        // this is a some.object.somewhere->method_call(); type is "typeof(some.object.somewhere)::method_call"
        
        // resolve object
        struct process_statement *obj_ps = process_resolve_object(p, p->ap, ps->s.object_name);
        if (!obj_ps) {
            process_statement_log(ps, BLOG_ERROR, "failed to resolve object %s for method call", ps->s.object_name);
            goto fail0;
        }
        ASSERT(obj_ps->state == SSTATE_ADULT)
        
        // build type string
        if (!(ps->type = concat_strings(3, obj_ps->type, "::", ps->s.method_name))) {
            process_statement_log(ps, BLOG_ERROR, "concat_strings failed");
            goto fail0;
        }
        
        method_object = &obj_ps->inst;
    }
    
    // find module to instantiate
    const struct NCDModule *module = find_module(ps->type);
    if (!module) {
        process_statement_log(ps, BLOG_ERROR, "failed to find module: %s", ps->type);
        goto fail1;
    }
    
    // init arguments list
    NCDValue_InitList(&ps->inst_args);
    
    // build arguments
    struct argument_elem *arg = ps->s.first_arg;
    while (arg) {
        // resolve argument
        NCDValue v;
        if (arg->is_var) {
            if (!process_resolve_variable(p, p->ap, arg->var, &v)) {
                process_statement_log(ps, BLOG_ERROR, "failed to resolve variable");
                goto fail2;
            }
        } else {
            if (!NCDValue_InitCopy(&v, &arg->val)) {
                process_statement_log(ps, BLOG_ERROR, "NCDValue_InitCopy failed");
                goto fail2;
            }
        }
        
        // move to list
        if (!NCDValue_ListAppend(&ps->inst_args, v)) {
            process_statement_log(ps, BLOG_ERROR, "NCDValue_ListAppend failed");
            NCDValue_Free(&v);
            goto fail2;
        }
        
        arg = arg->next_arg;
    }
    
    // initialize module instance
    NCDModuleInst_Init(
        &ps->inst, module, method_object, &ps->inst_args, &ss, &manager, &umanager, ps,
        (NCDModule_handler_event)process_statement_instance_handler_event,
        (NCDModule_handler_getvar)process_statement_instance_handler_getvar,
        (NCDModule_handler_getobj)process_statement_instance_handler_getobj,
        (NCDModule_handler_initprocess)process_statement_instance_handler_initprocess,
        (BLog_logfunc)process_statement_instance_logfunc
    );
    
    // set statement state CHILD
    ps->state = SSTATE_CHILD;
    
    // increment AP
    p->ap++;
    
    // increment FP
    p->fp++;
    
    process_assert_pointers(p);
    return;
    
fail2:
    NCDValue_Free(&ps->inst_args);
fail1:
    free(ps->type);
fail0:
    // mark error
    process_statement_set_error(ps);
    
    // schedule work to start the timer
    BPending_Set(&p->work_job);
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
    ASSERT(!p->terminating)
    
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
        process_log(p, BLOG_ERROR, "unknown statement name in object: %s", objname);
        return NULL;
    }
    
    ASSERT(rps->state == SSTATE_ADULT)
    
    if (!rest) {
        // this is the target
        return rps;
    }
    
    // resolve object in referred statement
    NCDModuleInst *inst = NCDModuleInst_GetObj(&rps->inst, rest);
    if (!inst) {
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

void process_statement_instance_handler_event (struct process_statement *ps, int event)
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
            
            // free type
            free(ps->type);
            
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

int process_statement_instance_handler_getvar (struct process_statement *ps, const char *varname, NCDValue *out)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    if (ps->i > process_rap(ps->p)) {
        process_statement_log(ps, BLOG_ERROR, "tried to resolve variable %s but it's dirty", varname);
        return 0;
    }
    
    return process_resolve_variable(ps->p, ps->i, varname, out);
}

NCDModuleInst * process_statement_instance_handler_getobj (struct process_statement *ps, const char *objname)
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

int process_statement_instance_handler_initprocess (struct process_statement *ps, NCDModuleProcess *mp, const char *template_name, NCDValue args)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    ASSERT(NCDValue_Type(&args) == NCDVALUE_LIST)
    
    // find template
    struct NCDConfig_interfaces *conf = config_ast;
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

void process_moduleprocess_handler_die (struct process *p)
{
    process_log(p, BLOG_INFO, "process termination requested");
    
    process_start_teminating(p);
}
