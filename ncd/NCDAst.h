/**
 * @file NCDAst.h
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

#ifndef BADVPN_NCDAST_H
#define BADVPN_NCDAST_H

#include <misc/debug.h>
#include <structure/LinkedList1.h>
#include <ncd/NCDValue.h>

typedef struct NCDProgram_s NCDProgram;
typedef struct NCDProcess_s NCDProcess;
typedef struct NCDBlock_s NCDBlock;
typedef struct NCDStatement_s NCDStatement;
typedef struct NCDIfBlock_s NCDIfBlock;
typedef struct NCDIf_s NCDIf;

struct NCDProgram_s {
    LinkedList1 processes_list;
    size_t num_processes;
};

struct NCDBlock_s {
    LinkedList1 statements_list;
    size_t count;
};

struct NCDProcess_s {
    int is_template;
    char *name;
    NCDBlock block;
};

struct NCDIfBlock_s {
    LinkedList1 ifs_list;
};

struct NCDStatement_s {
    int type;
    char *name;
    union {
        struct {
            char *objname;
            char *cmdname;
            NCDValue args;
        } reg;
        struct {
            NCDIfBlock ifblock;
            int have_else;
            NCDBlock else_block;
        } ifc;
    };
};

struct NCDIf_s {
    NCDValue cond;
    NCDBlock block;
};

struct ProgramProcess {
    LinkedList1Node processes_list_node;
    NCDProcess p;
};

struct BlockStatement {
    LinkedList1Node statements_list_node;
    NCDStatement s;
};

struct IfBlockIf {
    LinkedList1Node ifs_list_node;
    NCDIf ifc;
};

//

#define NCDSTATEMENT_REG 1
#define NCDSTATEMENT_IF 2

void NCDProgram_Init (NCDProgram *o);
void NCDProgram_Free (NCDProgram *o);
NCDProcess * NCDProgram_PrependProcess (NCDProgram *o, NCDProcess p) WARN_UNUSED;
NCDProcess * NCDProgram_FirstProcess (NCDProgram *o);
NCDProcess * NCDProgram_NextProcess (NCDProgram *o, NCDProcess *ep);
size_t NCDProgram_NumProcesses (NCDProgram *o);

int NCDProcess_Init (NCDProcess *o, int is_template, const char *name, NCDBlock block) WARN_UNUSED;
void NCDProcess_Free (NCDProcess *o);
int NCDProcess_IsTemplate (NCDProcess *o);
const char * NCDProcess_Name (NCDProcess *o);
NCDBlock * NCDProcess_Block (NCDProcess *o);

void NCDBlock_Init (NCDBlock *o);
void NCDBlock_Free (NCDBlock *o);
int NCDBlock_PrependStatement (NCDBlock *o, NCDStatement s) WARN_UNUSED;
int NCDBlock_InsertStatementAfter (NCDBlock *o, NCDStatement *after, NCDStatement s) WARN_UNUSED;
NCDStatement * NCDBlock_ReplaceStatement (NCDBlock *o, NCDStatement *es, NCDStatement s);
NCDStatement * NCDBlock_FirstStatement (NCDBlock *o);
NCDStatement * NCDBlock_NextStatement (NCDBlock *o, NCDStatement *es);
size_t NCDBlock_NumStatements (NCDBlock *o);

int NCDStatement_InitReg (NCDStatement *o, const char *name, const char *objname, const char *cmdname, NCDValue args) WARN_UNUSED;
int NCDStatement_InitIf (NCDStatement *o, const char *name, NCDIfBlock ifblock) WARN_UNUSED;
void NCDStatement_Free (NCDStatement *o);
int NCDStatement_Type (NCDStatement *o);
const char * NCDStatement_Name (NCDStatement *o);
const char * NCDStatement_RegObjName (NCDStatement *o);
const char * NCDStatement_RegCmdName (NCDStatement *o);
NCDValue * NCDStatement_RegArgs (NCDStatement *o);
NCDIfBlock * NCDStatement_IfBlock (NCDStatement *o);
void NCDStatement_IfAddElse (NCDStatement *o, NCDBlock else_block);
NCDBlock * NCDStatement_IfElse (NCDStatement *o);
NCDBlock NCDStatement_IfGrabElse (NCDStatement *o);

void NCDIfBlock_Init (NCDIfBlock *o);
void NCDIfBlock_Free (NCDIfBlock *o);
int NCDIfBlock_PrependIf (NCDIfBlock *o, NCDIf ifc) WARN_UNUSED;
NCDIf * NCDIfBlock_FirstIf (NCDIfBlock *o);
NCDIf * NCDIfBlock_NextIf (NCDIfBlock *o, NCDIf *ei);
NCDIf NCDIfBlock_GrabIf (NCDIfBlock *o, NCDIf *ei);

void NCDIf_Init (NCDIf *o, NCDValue cond, NCDBlock block);
void NCDIf_Free (NCDIf *o);
void NCDIf_FreeGrab (NCDIf *o, NCDValue *out_cond, NCDBlock *out_block);
NCDValue * NCDIf_Cond (NCDIf *o);
NCDBlock * NCDIf_Block (NCDIf *o);

#endif
