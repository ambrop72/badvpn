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
#include <limits.h>

#include <misc/version.h>
#include <misc/loglevel.h>
#include <misc/offset.h>
#include <misc/read_file.h>
#include <misc/balloc.h>
#include <misc/open_standard_streams.h>
#include <misc/split_string.h>
#include <structure/LinkedList1.h>
#include <base/BLog.h>
#include <base/BLog_syslog.h>
#include <system/BReactor.h>
#include <system/BSignal.h>
#include <system/BProcess.h>
#include <udevmonitor/NCDUdevManager.h>
#include <ncd/NCDConfigParser.h>
#include <ncd/NCDModule.h>
#include <ncd/NCDModuleIndex.h>
#include <ncd/NCDSugar.h>
#include <ncd/NCDInterpProg.h>
#include <ncd/modules/modules.h>

#include "ncd.h"

#include <generated/blog_channel_ncd.h>

#define LOGGER_STDOUT 1
#define LOGGER_STDERR 2
#define LOGGER_SYSLOG 3

#define SSTATE_CHILD 1
#define SSTATE_ADULT 2
#define SSTATE_DYING 3
#define SSTATE_FORGOTTEN 4

#define PSTATE_WORKING 1
#define PSTATE_UP 2
#define PSTATE_WAITING 3
#define PSTATE_TERMINATING 4

struct process;

struct statement {
    struct process *p;
    NCDModuleInst inst;
    NCDValMem args_mem;
    char *mem;
    int mem_size;
    int i;
    int state;
};

struct process {
    NCDProcess *proc_ast;
    NCDInterpBlock *iblock;
    NCDModuleProcess *module_process;
    BTimer wait_timer;
    BPending work_job;
    LinkedList1Node list_node; // node in processes
    struct statement *statements;
    char *mem;
    int mem_size;
    int state;
    int ap;
    int fp;
    int have_error;
    int num_statements;
};

// command-line options
struct {
    int help;
    int version;
    int logger;
    char *logger_syslog_facility;
    char *logger_syslog_ident;
    int loglevel;
    int loglevels[BLOG_NUM_CHANNELS];
    char *config_file;
    int retry_time;
    int no_udev;
    char **extra_args;
    int num_extra_args;
} options;

// reactor
BReactor reactor;

// are we terminating
int terminating;
int main_exit_code;

// process manager
BProcessManager manager;

// udev manager
NCDUdevManager umanager;

// module index
NCDModuleIndex mindex;

// program AST
NCDProgram program;

// structure for efficient interpretation
NCDInterpProg iprogram;

// common module parameters
struct NCDModuleInst_params module_params;
struct NCDModuleInst_iparams module_iparams;

// processes
LinkedList1 processes;

// buffer for concatenating base_type::method_name
char method_concat_buf[200];

static void print_help (const char *name);
static void print_version (void);
static int parse_arguments (int argc, char *argv[]);
static void signal_handler (void *unused);
static void start_terminate (int exit_code);
static int process_new (NCDProcess *proc_ast, NCDInterpBlock *iblock, NCDModuleProcess *module_process);
static void process_free (struct process *p, NCDModuleProcess **out_mp);
static int process_mem_is_preallocated (struct process *p, char *mem);
static void process_start_terminating (struct process *p);
static int process_rap (struct process *p);
static void process_assert_pointers (struct process *p);
static void process_logfunc (struct process *p);
static void process_log (struct process *p, int level, const char *fmt, ...);
static void process_schedule_work (struct process *p);
static void process_work_job_handler (struct process *p);
static void process_advance (struct process *p);
static void process_wait_timer_handler (struct process *p);
static int process_find_object (struct process *p, int pos, const char *name, NCDObject *out_object);
static int process_resolve_object_expr (struct process *p, int pos, char **names, NCDObject *out_object);
static int process_resolve_variable_expr (struct process *p, int pos, char **names, NCDValMem *mem, NCDValRef *out_value);
static void statement_logfunc (struct statement *ps);
static void statement_log (struct statement *ps, int level, const char *fmt, ...);
static int statement_allocate_memory (struct statement *ps, int alloc_size, void **out_mem);
static int statement_resolve_argument (struct statement *ps, NCDInterpValue *arg, NCDValMem *mem, NCDValRef *out);
static void statement_instance_func_event (struct statement *ps, int event);
static int statement_instance_func_getobj (struct statement *ps, const char *objname, NCDObject *out_object);
static int statement_instance_func_initprocess (struct statement *ps, NCDModuleProcess *mp, const char *template_name);
static void statement_instance_logfunc (struct statement *ps);
static void statement_instance_func_interp_exit (struct statement *ps, int exit_code);
static int statement_instance_func_interp_getargs (struct statement *ps, NCDValMem *mem, NCDValRef *out_value);
static btime_t statement_instance_func_interp_getretrytime (struct statement *ps);
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
        case LOGGER_STDERR:
            BLog_InitStderr();
            break;
        case LOGGER_SYSLOG:
            if (!BLog_InitSyslog(options.logger_syslog_ident, options.logger_syslog_facility)) {
                fprintf(stderr, "Failed to initialize syslog logger\n");
                goto fail0;
            }
            break;
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
    if (!BReactor_Init(&reactor)) {
        BLog(BLOG_ERROR, "BReactor_Init failed");
        goto fail1;
    }
    
    // set not terminating
    terminating = 0;
    
    // init process manager
    if (!BProcessManager_Init(&manager, &reactor)) {
        BLog(BLOG_ERROR, "BProcessManager_Init failed");
        goto fail1a;
    }
    
    // init udev manager
    NCDUdevManager_Init(&umanager, options.no_udev, &reactor, &manager);
    
    // init module index
    if (!NCDModuleIndex_Init(&mindex)) {
        BLog(BLOG_ERROR, "NCDModuleIndex_Init failed");
        goto fail1b;
    }
    
    // add module groups to index
    for (const struct NCDModuleGroup **g = ncd_modules; *g; g++) {
        if (!NCDModuleIndex_AddGroup(&mindex, *g)) {
            BLog(BLOG_ERROR, "NCDModuleIndex_AddGroup failed");
            goto fail2;
        }
    }
    
    // setup signal handler
    if (!BSignal_Init(&reactor, signal_handler, NULL)) {
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
    if (!NCDConfigParser_Parse((char *)file, file_len, &program)) {
        BLog(BLOG_ERROR, "NCDConfigParser_Parse failed");
        free(file);
        goto fail3;
    }
    
    // fee config file memory
    free(file);
    
    // desugar
    if (!NCDSugar_Desugar(&program)) {
        BLog(BLOG_ERROR, "NCDSugar_Desugar failed");
        goto fail4;
    }
    
    // init interp program
    if (!NCDInterpProg_Init(&iprogram, &program)) {
        BLog(BLOG_ERROR, "NCDInterpProg_Init failed");
        goto fail4;
    }
    
    // init module params
    struct NCDModuleInitParams params;
    params.reactor = &reactor;
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
    module_params.func_event = (NCDModuleInst_func_event)statement_instance_func_event;
    module_params.func_getobj = (NCDModuleInst_func_getobj)statement_instance_func_getobj;
    module_params.logfunc = (BLog_logfunc)statement_instance_logfunc;
    module_iparams.reactor = &reactor;
    module_iparams.manager = &manager;
    module_iparams.umanager = &umanager;
    module_iparams.func_initprocess = (NCDModuleInst_func_initprocess)statement_instance_func_initprocess;
    module_iparams.func_interp_exit = (NCDModuleInst_func_interp_exit)statement_instance_func_interp_exit;
    module_iparams.func_interp_getargs = (NCDModuleInst_func_interp_getargs)statement_instance_func_interp_getargs;
    module_iparams.func_interp_getretrytime = (NCDModuleInst_func_interp_getretrytime)statement_instance_func_interp_getretrytime;
    
    // init processes list
    LinkedList1_Init(&processes);
    
    // init processes
    for (NCDProcess *p = NCDProgram_FirstProcess(&program); p; p = NCDProgram_NextProcess(&program, p)) {
        if (NCDProcess_IsTemplate(p)) {
            continue;
        }
        
        // find iblock
        NCDProcess *f_proc;
        NCDInterpBlock *iblock;
        int res = NCDInterpProg_FindProcess(&iprogram, NCDProcess_Name(p), &f_proc, &iblock);
        ASSERT(res)
        ASSERT(f_proc == p)
        
        if (!process_new(p, iblock, NULL)) {
            BLog(BLOG_ERROR, "failed to initialize process, exiting");
            goto fail6;
        }
    }
    
    // enter event loop
    BLog(BLOG_NOTICE, "entering event loop");
    BReactor_Exec(&reactor);
    
    ASSERT(LinkedList1_IsEmpty(&processes))
    
fail6:;
    LinkedList1Node *ln;
    while (ln = LinkedList1_GetFirst(&processes)) {
        struct process *p = UPPER_OBJECT(ln, struct process, list_node);
        NCDModuleProcess *mp;
        process_free(p, &mp);
        ASSERT(!mp)
    }
fail5:
    // free modules
    while (num_inited_modules > 0) {
        const struct NCDModuleGroup **g = &ncd_modules[num_inited_modules - 1];
        if ((*g)->func_globalfree) {
            (*g)->func_globalfree();
        }
        num_inited_modules--;
    }
    // free interp program
    NCDInterpProg_Free(&iprogram);
fail4:
    // free program AST
    NCDProgram_Free(&program);
fail3:
    // remove signal handler
    BSignal_Finish();
fail2:
    // free module index
    NCDModuleIndex_Free(&mindex);
fail1b:
    // free udev manager
    NCDUdevManager_Free(&umanager);
    
    // free process manager
    BProcessManager_Free(&manager);
fail1a:
    // free reactor
    BReactor_Free(&reactor);
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
        "        [--logger <stdout/stderr/syslog>]\n"
        "        (logger=syslog?\n"
        "            [--syslog-facility <string>]\n"
        "            [--syslog-ident <string>]\n"
        "        )\n"
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
    options.logger = LOGGER_STDERR;
    options.logger_syslog_facility = "daemon";
    options.logger_syslog_ident = argv[0];
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
            else if (!strcmp(arg2, "stderr")) {
                options.logger = LOGGER_STDERR;
            }
            else if (!strcmp(arg2, "syslog")) {
                options.logger = LOGGER_SYSLOG;
            }
            else {
                fprintf(stderr, "%s: wrong argument\n", arg);
                return 0;
            }
            i++;
        }
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
    
    if (LinkedList1_IsEmpty(&processes)) {
        BReactor_Quit(&reactor, 0);
        return;
    }
    
    // start terminating non-template processes
    for (LinkedList1Node *ln = LinkedList1_GetFirst(&processes); ln; ln = LinkedList1Node_Next(ln)) {
        struct process *p = UPPER_OBJECT(ln, struct process, list_node);
        if (p->module_process) {
            continue;
        }
        if (p->state != PSTATE_TERMINATING) {
            process_start_terminating(p);
        }
    }
}

static int process_new (NCDProcess *proc_ast, NCDInterpBlock *iblock, NCDModuleProcess *module_process)
{
    // get num statements
    size_t num_statements = NCDBlock_NumStatements(NCDProcess_Block(proc_ast));
    if (num_statements > INT_MAX) {
        BLog(BLOG_ERROR, "too many statements");
        goto fail0;
    }
    
    // calculate size of preallocated statement memory
    int mem_size = NCDInterpBlock_PreallocSize(iblock);
    if (mem_size < 0 || mem_size > SIZE_MAX) {
        BLog(BLOG_ERROR, "NCDInterpBlock_PreallocSize failed");
        goto fail0;
    }
    
    // allocate memory
    void *v_statements;
    void *v_mem;
    struct process *p = BAllocThreeArrays(1, sizeof(*p), num_statements, sizeof(p->statements[0]), mem_size, 1, &v_statements, &v_mem);
    if (!p) {
        BLog(BLOG_ERROR, "BAllocSize failed");
        goto fail0;
    }
    
    // set variables
    p->proc_ast = proc_ast;
    p->iblock = iblock;
    p->module_process = module_process;
    p->statements = v_statements;
    p->mem = v_mem;
    p->mem_size = mem_size;
    p->state = PSTATE_WORKING;
    p->ap = 0;
    p->fp = 0;
    p->have_error = 0;
    p->num_statements = num_statements;
    
    // set module process handlers
    if (p->module_process) {
        NCDModuleProcess_Interp_SetHandlers(p->module_process, p,
                                            (NCDModuleProcess_interp_func_event)process_moduleprocess_func_event,
                                            (NCDModuleProcess_interp_func_getobj)process_moduleprocess_func_getobj);
    }
    
    // init statements
    for (int i = 0; i < num_statements; i++) {
        struct statement *ps = &p->statements[i];
        ps->p = p;
        ps->i = i;
        ps->state = SSTATE_FORGOTTEN;
        ps->mem_size = NCDInterpBlock_StatementPreallocSize(iblock, i);
        ps->mem = (ps->mem_size == 0 ? NULL : p->mem + NCDInterpBlock_StatementPreallocOffset(iblock, i));
    }
    
    // init timer
    BTimer_Init(&p->wait_timer, options.retry_time, (BTimer_handler)process_wait_timer_handler, p);
    
    // init work job
    BPending_Init(&p->work_job, BReactor_PendingGroup(&reactor), (BPending_handler)process_work_job_handler, p);
    
    // insert to processes list
    LinkedList1_Append(&processes, &p->list_node);
    
    // schedule work
    BPending_Set(&p->work_job);   
    return 1;
    
fail0:
    BLog(BLOG_ERROR, "failed to initialize process %s", NCDProcess_Name(proc_ast));
    return 0;
}

void process_free (struct process *p, NCDModuleProcess **out_mp)
{
    ASSERT(p->ap == 0)
    ASSERT(p->fp == 0)
    ASSERT(out_mp)
    
    // give module process to caller so it can inform the process creator that the process has terminated
    *out_mp = p->module_process;
    
    // free statement memory
    for (int i = 0; i < p->num_statements; i++) {
        struct statement *ps = &p->statements[i];
        if (ps->mem && !process_mem_is_preallocated(p, ps->mem)) {
            free(ps->mem);
        }
    }
    
    // remove from processes list
    LinkedList1_Remove(&processes, &p->list_node);
    
    // free work job
    BPending_Free(&p->work_job);
    
    // free timer
    BReactor_RemoveTimer(&reactor, &p->wait_timer);
    
    // free strucure
    BFree(p);
}

int process_mem_is_preallocated (struct process *p, char *mem)
{
    ASSERT(mem)
    
    return (mem >= p->mem && mem < p->mem + p->mem_size);
}

void process_start_terminating (struct process *p)
{
    ASSERT(p->state != PSTATE_TERMINATING)
    
    // set terminating
    p->state = PSTATE_TERMINATING;
    
    // schedule work
    process_schedule_work(p);
}

int process_rap (struct process *p)
{
    if (p->ap > 0 && p->statements[p->ap - 1].state == SSTATE_CHILD) {
        return (p->ap - 1);
    } else {
        return p->ap;
    }
}

void process_assert_pointers (struct process *p)
{
    ASSERT(p->ap <= p->num_statements)
    ASSERT(p->fp >= p->ap)
    ASSERT(p->fp <= p->num_statements)
    
#ifndef NDEBUG
    // check AP
    for (int i = 0; i < p->ap; i++) {
        if (i == p->ap - 1) {
            ASSERT(p->statements[i].state == SSTATE_ADULT || p->statements[i].state == SSTATE_CHILD)
        } else {
            ASSERT(p->statements[i].state == SSTATE_ADULT)
        }
    }
    
    // check FP
    int fp = p->num_statements;
    while (fp > 0 && p->statements[fp - 1].state == SSTATE_FORGOTTEN) {
        fp--;
    }
    ASSERT(p->fp == fp)
#endif
}

void process_logfunc (struct process *p)
{
    BLog_Append("process %s: ", NCDProcess_Name(p->proc_ast));
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
    BReactor_RemoveTimer(&reactor, &p->wait_timer);
    
    // schedule work
    BPending_Set(&p->work_job);
}

void process_work_job_handler (struct process *p)
{
    process_assert_pointers(p);
    ASSERT(!BTimer_IsRunning(&p->wait_timer))
    
    if (p->state == PSTATE_WAITING) {
        return;
    }
    
    if (p->state == PSTATE_TERMINATING) {
        if (p->fp == 0) {
            // free process
            NCDModuleProcess *mp;
            process_free(p, &mp);
            
            // if program is terminating amd there are no more processes, exit program
            if (terminating && LinkedList1_IsEmpty(&processes)) {
                ASSERT(!mp)
                BReactor_Quit(&reactor, 0);
                return;
            }
            
            // inform the process creator that the process has terminated
            if (mp) {
                NCDModuleProcess_Interp_Terminated(mp);
                return;
            }
            
            return;
        }
        
        // order the last living statement to die, if needed
        struct statement *ps = &p->statements[p->fp - 1];
        ASSERT(ps->state != SSTATE_FORGOTTEN)
        if (ps->state != SSTATE_DYING) {
            statement_log(ps, BLOG_INFO, "killing");
            
            // set statement state DYING
            ps->state = SSTATE_DYING;
            
            // update AP
            if (p->ap > ps->i) {
                p->ap = ps->i;
            }
            
            // order it to die
            NCDModuleInst_Die(&ps->inst);
            return;
        }
        return;
    }
    
    // process was up but is no longer?
    if (p->state == PSTATE_UP && !(p->ap == process_rap(p) && p->ap == p->num_statements)) {
        // if we have module process, wait for its permission to continue
        if (p->module_process) {
            // set state waiting
            p->state = PSTATE_WAITING;
            
            // set module process down
            NCDModuleProcess_Interp_Down(p->module_process);
            return;
        }
        
        // set state working
        p->state = PSTATE_WORKING;
    }
    
    // cleaning up?
    if (p->ap < p->fp) {
        // order the last living statement to die, if needed
        struct statement *ps = &p->statements[p->fp - 1];
        if (ps->state != SSTATE_DYING) {
            statement_log(ps, BLOG_INFO, "killing");
            
            // set statement state DYING
            ps->state = SSTATE_DYING;
            
            // order it to die
            NCDModuleInst_Die(&ps->inst);
            return;
        }
        return;
    }
    
    // clean?
    if (p->ap > process_rap(p)) {
        ASSERT(p->ap > 0)
        ASSERT(p->ap <= p->num_statements)
        
        struct statement *ps = &p->statements[p->ap - 1];
        ASSERT(ps->state == SSTATE_CHILD)
        
        statement_log(ps, BLOG_INFO, "clean");
        
        // report clean
        NCDModuleInst_Clean(&ps->inst);
        return;
    }
    
    // advancing?
    if (p->ap < p->num_statements) {
        ASSERT(p->state == PSTATE_WORKING)
        struct statement *ps = &p->statements[p->ap];
        ASSERT(ps->state == SSTATE_FORGOTTEN)
        
        if (p->have_error) {
            statement_log(ps, BLOG_INFO, "waiting after error");
            
            // clear error
            p->have_error = 0;
            
            // set wait timer
            BReactor_SetTimer(&reactor, &p->wait_timer);
        } else {
            // advance
            process_advance(p);
        }
        return;
    }
    
    // have we just finished?
    if (p->state == PSTATE_WORKING) {
        process_log(p, BLOG_INFO, "victory");
        
        // set state up
        p->state = PSTATE_UP;
        
        // set module process up
        if (p->module_process) {
            NCDModuleProcess_Interp_Up(p->module_process);
            return;
        }
    }
}

void process_advance (struct process *p)
{
    process_assert_pointers(p);
    ASSERT(p->ap == p->fp)
    ASSERT(p->ap == process_rap(p))
    ASSERT(p->ap < p->num_statements)
    ASSERT(!p->have_error)
    ASSERT(!BPending_IsSet(&p->work_job))
    ASSERT(!BTimer_IsRunning(&p->wait_timer))
    ASSERT(p->state == PSTATE_WORKING)
    
    struct statement *ps = &p->statements[p->ap];
    ASSERT(ps->state == SSTATE_FORGOTTEN)
    
    statement_log(ps, BLOG_INFO, "initializing");
    
    NCDObject object;
    NCDObject *object_ptr = NULL;
    
    char **object_names = NCDInterpBlock_StatementObjNames(p->iblock, p->ap);
    const char *type = NCDInterpBlock_StatementCmdName(p->iblock, p->ap);
    
    // if this is a method-like statement, type is really "base_type(object)::method_name"
    if (object_names) {
        // get object
        if (!process_resolve_object_expr(p, p->ap, object_names, &object)) {
            goto fail0;
        }
        object_ptr = &object;
        
        // get object type
        const char *object_type = NCDObject_Type(&object);
        if (!object_type) {
            statement_log(ps, BLOG_ERROR, "cannot call method on object with no type");
            goto fail0;
        }
        
        // build type string
        int res = snprintf(method_concat_buf, sizeof(method_concat_buf), "%s::%s", object_type, type);
        if (res >= sizeof(method_concat_buf) || res < 0) {
            statement_log(ps, BLOG_ERROR, "type/method name too long");
            goto fail0;
        }
        type = method_concat_buf;
    }
    
    // find module to instantiate
    const struct NCDModule *module = NCDModuleIndex_FindModule(&mindex, type);
    if (!module) {
        statement_log(ps, BLOG_ERROR, "failed to find module: %s", type);
        goto fail0;
    }
    
    // register alloc size for future preallocations
    NCDInterpBlock_StatementBumpAllocSize(p->iblock, p->ap, module->alloc_size);
    
    // init args mem
    NCDValMem_Init(&ps->args_mem);
    
    // resolve arguments
    NCDValRef args;
    NCDInterpValue *iargs = NCDInterpBlock_StatementInterpValue(p->iblock, ps->i);
    if (!statement_resolve_argument(ps, iargs, &ps->args_mem, &args)) {
        statement_log(ps, BLOG_ERROR, "failed to resolve arguments");
        goto fail1;
    }
    
    // allocate memory
    void *mem;
    if (!statement_allocate_memory(ps, module->alloc_size, &mem)) {
        statement_log(ps, BLOG_ERROR, "failed to allocate memory");
        goto fail1;
    }
    
    // set statement state CHILD
    ps->state = SSTATE_CHILD;
    
    // increment AP
    p->ap++;
    
    // increment FP
    p->fp++;
    
    process_assert_pointers(p);
    
    // initialize module instance
    NCDModuleInst_Init(&ps->inst, module, mem, object_ptr, args, ps, &module_params, &module_iparams);
    return;
    
fail1:
    NCDValMem_Free(&ps->args_mem);
fail0:
    // set error
    p->have_error = 1;
    
    // schedule work to start the timer
    process_schedule_work(p);
}

void process_wait_timer_handler (struct process *p)
{
    process_assert_pointers(p);
    ASSERT(p->ap == p->fp)
    ASSERT(p->ap == process_rap(p))
    ASSERT(p->ap < p->num_statements)
    ASSERT(!p->have_error)
    ASSERT(!BPending_IsSet(&p->work_job))
    ASSERT(p->state == PSTATE_WORKING)
    
    process_log(p, BLOG_INFO, "retrying");
    
    // advance
    process_advance(p);
}

int process_find_object (struct process *p, int pos, const char *name, NCDObject *out_object)
{
    ASSERT(pos >= 0)
    ASSERT(pos <= p->num_statements)
    ASSERT(name)
    ASSERT(out_object)
    
    int i = NCDInterpBlock_FindStatement(p->iblock, pos, name);
    if (i >= 0) {
        struct statement *ps = &p->statements[i];
        ASSERT(i < p->num_statements)
        
        if (ps->state == SSTATE_FORGOTTEN) {
            process_log(p, BLOG_ERROR, "statement (%d) is uninitialized", i);
            return 0;
        }
        
        *out_object = NCDModuleInst_Object(&ps->inst);
        return 1;
    }
    
    if (p->module_process && NCDModuleProcess_Interp_GetSpecialObj(p->module_process, name, out_object)) {
        return 1;
    }
    
    return 0;
}

int process_resolve_object_expr (struct process *p, int pos, char **names, NCDObject *out_object)
{
    ASSERT(pos >= 0)
    ASSERT(pos <= p->num_statements)
    ASSERT(names)
    ASSERT(count_strings(names) > 0)
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
    char *name = implode_strings(names, '.');
    process_log(p, BLOG_ERROR, "failed to resolve object (%s) from position %zu", (name ? name : ""), pos);
    free(name);
    return 0;
}

int process_resolve_variable_expr (struct process *p, int pos, char **names, NCDValMem *mem, NCDValRef *out_value)
{
    ASSERT(pos >= 0)
    ASSERT(pos <= p->num_statements)
    ASSERT(names)
    ASSERT(count_strings(names) > 0)
    ASSERT(mem)
    ASSERT(out_value)
    
    NCDObject object;
    if (!process_find_object(p, pos, names[0], &object)) {
        goto fail;
    }
    
    if (!NCDObject_ResolveVarExpr(&object, names + 1, mem, out_value)) {
        goto fail;
    }
    
    return 1;
    
fail:;
    char *name = implode_strings(names, '.');
    process_log(p, BLOG_ERROR, "failed to resolve variable (%s) from position %zu", (name ? name : ""), pos);
    free(name);
    return 0;
}

void statement_logfunc (struct statement *ps)
{
    process_logfunc(ps->p);
    BLog_Append("statement %zu: ", ps->i);
}

void statement_log (struct statement *ps, int level, const char *fmt, ...)
{
    if (!BLog_WouldLog(BLOG_CURRENT_CHANNEL, level)) {
        return;
    }
    
    va_list vl;
    va_start(vl, fmt);
    BLog_LogViaFuncVarArg((BLog_logfunc)statement_logfunc, ps, BLOG_CURRENT_CHANNEL, level, fmt, vl);
    va_end(vl);
}

int statement_allocate_memory (struct statement *ps, int alloc_size, void **out_mem)
{
    ASSERT(alloc_size >= 0)
    ASSERT(out_mem)
    
    if (alloc_size == 0) {
        *out_mem = NULL;
        return 1;
    }
    
    if (alloc_size > ps->mem_size) {
        if (ps->mem && !process_mem_is_preallocated(ps->p, ps->mem)) {
            free(ps->mem);
        }
        
        if (!(ps->mem = malloc(alloc_size))) {
            statement_log(ps, BLOG_ERROR, "malloc failed");
            ps->mem_size = 0;
            return 0;
        }
        
        ps->mem_size = alloc_size;
    }
    
    *out_mem = ps->mem;
    return 1;
}

int statement_resolve_argument (struct statement *ps, NCDInterpValue *arg, NCDValMem *mem, NCDValRef *out)
{
    ASSERT(ps->i <= process_rap(ps->p))
    ASSERT(arg)
    ASSERT(mem)
    ASSERT(out)
    
    switch (arg->type) {
        case NCDVALUE_STRING: {
            *out = NCDVal_NewStringBin(mem, (uint8_t *)arg->string, arg->string_len);
            if (NCDVal_IsInvalid(*out)) {
                statement_log(ps, BLOG_ERROR, "NCDVal_NewStringBin failed");
                return 0;
            }
        } break;
        
        case NCDVALUE_VAR: {
            if (!process_resolve_variable_expr(ps->p, ps->i, arg->variable_names, mem, out)) {
                return 0;
            }
            if (NCDVal_IsInvalid(*out)) {
                return 0;
            }
        } break;
        
        case NCDVALUE_LIST: {
            *out = NCDVal_NewList(mem, arg->list_count);
            if (NCDVal_IsInvalid(*out)) {
                statement_log(ps, BLOG_ERROR, "NCDVal_NewList failed");
                return 0;
            }
            
            for (LinkedList1Node *n = LinkedList1_GetFirst(&arg->list); n; n = LinkedList1Node_Next(n)) {
                struct NCDInterpValueListElem *elem = UPPER_OBJECT(n, struct NCDInterpValueListElem, list_node);
                
                NCDValRef new_elem;
                if (!statement_resolve_argument(ps, &elem->value, mem, &new_elem)) {
                    return 0;
                }
                
                NCDVal_ListAppend(*out, new_elem);
            }
        } break;
        
        case NCDVALUE_MAP: {
            *out = NCDVal_NewMap(mem, arg->map_count);
            if (NCDVal_IsInvalid(*out)) {
                statement_log(ps, BLOG_ERROR, "NCDVal_NewMap failed");
                return 0;
            }
            
            for (LinkedList1Node *n = LinkedList1_GetFirst(&arg->maplist); n; n = LinkedList1Node_Next(n)) {
                struct NCDInterpValueMapElem *elem = UPPER_OBJECT(n, struct NCDInterpValueMapElem, maplist_node);
                
                NCDValRef new_key;
                if (!statement_resolve_argument(ps, &elem->key, mem, &new_key)) {
                    return 0;
                }
                
                NCDValRef new_val;
                if (!statement_resolve_argument(ps, &elem->val, mem, &new_val)) {
                    return 0;
                }
                
                int res = NCDVal_MapInsert(*out, new_key, new_val);
                if (!res) {
                    statement_log(ps, BLOG_ERROR, "duplicate map keys");
                    return 0;
                }
            }
        } break;
        
        default: ASSERT(0);
    }
    
    return 1;
}

void statement_instance_func_event (struct statement *ps, int event)
{
    ASSERT(ps->state == SSTATE_CHILD || ps->state == SSTATE_ADULT || ps->state == SSTATE_DYING)
    
    struct process *p = ps->p;
    process_assert_pointers(p);
    
    // schedule work
    process_schedule_work(p);
    
    switch (event) {
        case NCDMODULE_EVENT_UP: {
            ASSERT(ps->state == SSTATE_CHILD)
            
            statement_log(ps, BLOG_INFO, "up");
            
            // set state ADULT
            ps->state = SSTATE_ADULT;
        } break;
        
        case NCDMODULE_EVENT_DOWN: {
            ASSERT(ps->state == SSTATE_ADULT)
            
            statement_log(ps, BLOG_INFO, "down");
            
            // set state CHILD
            ps->state = SSTATE_CHILD;
            
            // clear error
            if (ps->i < p->ap) {
                p->have_error = 0;
            }
            
            // update AP
            if (p->ap > ps->i + 1) {
                p->ap = ps->i + 1;
            }
        } break;
        
        case NCDMODULE_EVENT_DEAD: {
            int is_error = NCDModuleInst_HaveError(&ps->inst);
            
            if (is_error) {
                statement_log(ps, BLOG_ERROR, "died with error");
            } else {
                statement_log(ps, BLOG_INFO, "died");
            }
            
            // free instance
            NCDModuleInst_Free(&ps->inst);
            
            // free arguments memory
            NCDValMem_Free(&ps->args_mem);
            
            // set state FORGOTTEN
            ps->state = SSTATE_FORGOTTEN;
            
            // set error
            if (is_error && ps->i < p->ap) {
                p->have_error = 1;
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

int statement_instance_func_getobj (struct statement *ps, const char *objname, NCDObject *out_object)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    return process_find_object(ps->p, ps->i, objname, out_object);
}

int statement_instance_func_initprocess (struct statement *ps, NCDModuleProcess *mp, const char *template_name)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    // find template
    NCDProcess *p_ast;
    NCDInterpBlock *iblock;
    if (!NCDInterpProg_FindProcess(&iprogram, template_name, &p_ast, &iblock) || !NCDProcess_IsTemplate(p_ast)) {
        statement_log(ps, BLOG_ERROR, "no template named %s", template_name);
        return 0;
    }
    
    // create process
    if (!process_new(p_ast, iblock, mp)) {
        statement_log(ps, BLOG_ERROR, "failed to create process from template %s", template_name);
        return 0;
    }
    
    statement_log(ps, BLOG_INFO, "created process from template %s", template_name);
    
    return 1;
}

void statement_instance_logfunc (struct statement *ps)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    statement_logfunc(ps);
    BLog_Append("module: ");
}

void statement_instance_func_interp_exit (struct statement *ps, int exit_code)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    start_terminate(exit_code);
}

int statement_instance_func_interp_getargs (struct statement *ps, NCDValMem *mem, NCDValRef *out_value)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    *out_value = NCDVal_NewList(mem, options.num_extra_args);
    if (NCDVal_IsInvalid(*out_value)) {
        statement_log(ps, BLOG_ERROR, "NCDVal_NewList failed");
        goto fail;
    }
    
    for (int i = 0; i < options.num_extra_args; i++) {
        NCDValRef arg = NCDVal_NewString(mem, options.extra_args[i]);
        if (NCDVal_IsInvalid(arg)) {
            statement_log(ps, BLOG_ERROR, "NCDVal_NewString failed");
            goto fail;
        }
        
        NCDVal_ListAppend(*out_value, arg);
    }
    
    return 1;
    
fail:
    *out_value = NCDVal_NewInvalid();
    return 1;
}

btime_t statement_instance_func_interp_getretrytime (struct statement *ps)
{
    ASSERT(ps->state != SSTATE_FORGOTTEN)
    
    return options.retry_time;
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
