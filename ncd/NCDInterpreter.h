/**
 * @file NCDInterpreter.h
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

#ifndef BADVPN_NCD_INTERPRETER_H
#define BADVPN_NCD_INTERPRETER_H

#include <stddef.h>

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <system/BTime.h>
#include <system/BReactor.h>
#include <ncd/NCDStringIndex.h>
#include <ncd/NCDMethodIndex.h>
#include <ncd/NCDModuleIndex.h>
#include <ncd/NCDAst.h>
#include <ncd/NCDPlaceholderDb.h>
#include <ncd/NCDInterpProg.h>
#include <ncd/NCDModule.h>
#include <structure/LinkedList1.h>

#ifndef BADVPN_NO_PROCESS
#include <system/BProcess.h>
#endif
#ifndef BADVPN_NO_UDEV
#include <udevmonitor/NCDUdevManager.h>
#endif
#ifndef BADVPN_NO_RANDOM
#include <random/BRandom2.h>
#endif

typedef void (*NCDInterpreter_handler_finished) (void *user, int exit_code);

struct NCDInterpreter_params {
    // callbacks
    NCDInterpreter_handler_finished handler_finished;
    void *user;
    
    // options
    btime_t retry_time;
    char **extra_args;
    int num_extra_args;
    
    // possibly shared resources
    BReactor *reactor;
#ifndef BADVPN_NO_PROCESS
    BProcessManager *manager;
#endif
#ifndef BADVPN_NO_UDEV
    NCDUdevManager *umanager;
#endif
#ifndef BADVPN_NO_RANDOM
    BRandom2 *random2;
#endif
};

typedef struct {
    // parameters
    struct NCDInterpreter_params params;
    
    // are we terminating
    int terminating;
    int main_exit_code;

    // string index
    NCDStringIndex string_index;

    // method index
    NCDMethodIndex method_index;

    // module index
    NCDModuleIndex mindex;

    // program AST
    NCDProgram program;

    // placeholder database
    NCDPlaceholderDb placeholder_db;

    // structure for efficient interpretation
    NCDInterpProg iprogram;

    // common module parameters
    struct NCDModuleInst_params module_params;
    struct NCDModuleInst_iparams module_iparams;
    
    // number of modules we have found and inited
    size_t num_inited_modules;
    
    // processes
    LinkedList1 processes;
    
    DebugObject d_obj;
} NCDInterpreter;

int NCDInterpreter_Init (NCDInterpreter *o, const char *program, size_t program_len, struct NCDInterpreter_params params) WARN_UNUSED;
void NCDInterpreter_Free (NCDInterpreter *o);
void NCDInterpreter_RequestShutdown (NCDInterpreter *o, int exit_code);

#endif
