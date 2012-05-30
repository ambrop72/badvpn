/**
 * @file NCDAst.c
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

#include <stdlib.h>
#include <limits.h>

#include <misc/offset.h>

#include "NCDAst.h"

void NCDProgram_Init (NCDProgram *o)
{
    LinkedList1_Init(&o->processes_list);
}

void NCDProgram_Free (NCDProgram *o)
{
    LinkedList1Node *ln;
    while (ln = LinkedList1_GetFirst(&o->processes_list)) {
        struct ProgramProcess *e = UPPER_OBJECT(ln, struct ProgramProcess, processes_list_node);
        NCDProcess_Free(&e->p);
        LinkedList1_Remove(&o->processes_list, &e->processes_list_node);
        free(e);
    }
}

NCDProcess * NCDProgram_PrependProcess (NCDProgram *o, NCDProcess p)
{
    struct ProgramProcess *e = malloc(sizeof(*e));
    if (!e) {
        return NULL;
    }
    
    LinkedList1_Prepend(&o->processes_list, &e->processes_list_node);
    e->p = p;
    
    return &e->p;
}

NCDProcess * NCDProgram_FirstProcess (NCDProgram *o)
{
    LinkedList1Node *ln = LinkedList1_GetFirst(&o->processes_list);
    if (!ln) {
        return NULL;
    }
    
    struct ProgramProcess *e = UPPER_OBJECT(ln, struct ProgramProcess, processes_list_node);
    
    return &e->p;
}

NCDProcess * NCDProgram_NextProcess (NCDProgram *o, NCDProcess *ep)
{
    ASSERT(ep)
    
    struct ProgramProcess *cur_e = UPPER_OBJECT(ep, struct ProgramProcess, p);
    
    LinkedList1Node *ln = LinkedList1Node_Next(&cur_e->processes_list_node);
    if (!ln) {
        return NULL;
    }
    
    struct ProgramProcess *e = UPPER_OBJECT(ln, struct ProgramProcess, processes_list_node);
    
    return &e->p;
}

int NCDProcess_Init (NCDProcess *o, int is_template, const char *name, NCDBlock block)
{
    ASSERT(is_template == !!is_template)
    ASSERT(name)
    
    if (!(o->name = strdup(name))) {
        return 0;
    }
    
    o->is_template = is_template;
    o->block = block;
    
    return 1;
}

void NCDProcess_Free (NCDProcess *o)
{
    NCDBlock_Free(&o->block);
    free(o->name);
}

int NCDProcess_IsTemplate (NCDProcess *o)
{
    return o->is_template;
}

const char * NCDProcess_Name (NCDProcess *o)
{
    return o->name;
}

NCDBlock * NCDProcess_Block (NCDProcess *o)
{
    return &o->block;
}

void NCDBlock_Init (NCDBlock *o)
{
    LinkedList1_Init(&o->statements_list);
    o->count = 0;
}

void NCDBlock_Free (NCDBlock *o)
{
    LinkedList1Node *ln;
    while (ln = LinkedList1_GetFirst(&o->statements_list)) {
        struct BlockStatement *e = UPPER_OBJECT(ln, struct BlockStatement, statements_list_node);
        NCDStatement_Free(&e->s);
        LinkedList1_Remove(&o->statements_list, &e->statements_list_node);
        free(e);
    }
}

int NCDBlock_PrependStatement (NCDBlock *o, NCDStatement s)
{
    return NCDBlock_InsertStatementAfter(o, NULL, s);
}

int NCDBlock_InsertStatementAfter (NCDBlock *o, NCDStatement *after, NCDStatement s)
{
    struct BlockStatement *after_e = NULL;
    if (after) {
        after_e = UPPER_OBJECT(after, struct BlockStatement, s);
    }
    
    if (o->count == SIZE_MAX) {
        return 0;
    }
    
    struct BlockStatement *e = malloc(sizeof(*e));
    if (!e) {
        return 0;
    }
    
    if (after_e) {
        LinkedList1_InsertAfter(&o->statements_list, &e->statements_list_node, &after_e->statements_list_node);
    } else {
        LinkedList1_Prepend(&o->statements_list, &e->statements_list_node);
    }
    e->s = s;
    
    o->count++;
    
    return 1;
}

NCDStatement * NCDBlock_ReplaceStatement (NCDBlock *o, NCDStatement *es, NCDStatement s)
{
    ASSERT(es)
    
    struct BlockStatement *e = UPPER_OBJECT(es, struct BlockStatement, s);
    
    NCDStatement_Free(&e->s);
    e->s = s;
    
    return &e->s;
}

NCDStatement * NCDBlock_FirstStatement (NCDBlock *o)
{
    LinkedList1Node *ln = LinkedList1_GetFirst(&o->statements_list);
    if (!ln) {
        return NULL;
    }
    
    struct BlockStatement *e = UPPER_OBJECT(ln, struct BlockStatement, statements_list_node);
    
    return &e->s;
}

NCDStatement * NCDBlock_NextStatement (NCDBlock *o, NCDStatement *es)
{
    ASSERT(es)
    
    struct BlockStatement *cur_e = UPPER_OBJECT(es, struct BlockStatement, s);
    
    LinkedList1Node *ln = LinkedList1Node_Next(&cur_e->statements_list_node);
    if (!ln) {
        return NULL;
    }
    
    struct BlockStatement *e = UPPER_OBJECT(ln, struct BlockStatement, statements_list_node);
    
    return &e->s;
}

size_t NCDBlock_NumStatements (NCDBlock *o)
{
    return o->count;
}

int NCDStatement_InitReg (NCDStatement *o, const char *name, const char *objname, const char *cmdname, NCDValue args)
{
    ASSERT(cmdname)
    ASSERT(NCDValue_IsList(&args))
    
    o->name = NULL;
    o->reg.objname = NULL;
    o->reg.cmdname = NULL;
    
    if (name && !(o->name = strdup(name))) {
        goto fail;
    }
    
    if (objname && !(o->reg.objname = strdup(objname))) {
        goto fail;
    }
    
    if (!(o->reg.cmdname = strdup(cmdname))) {
        goto fail;
    }
    
    o->type = NCDSTATEMENT_REG;
    o->reg.args = args;
    
    return 1;
    
fail:
    free(o->name);
    free(o->reg.objname);
    free(o->reg.cmdname);
    return 0;
}

int NCDStatement_InitIf (NCDStatement *o, const char *name, NCDIfBlock ifblock)
{
    o->name = NULL;
    
    if (name && !(o->name = strdup(name))) {
        return 0;
    }
    
    o->type = NCDSTATEMENT_IF;
    o->ifc.ifblock = ifblock;
    o->ifc.have_else = 0;
    
    return 1;
}

void NCDStatement_Free (NCDStatement *o)
{
    switch (o->type) {
        case NCDSTATEMENT_REG: {
            NCDValue_Free(&o->reg.args);
            free(o->reg.cmdname);
            free(o->reg.objname);
        } break;
        
        case NCDSTATEMENT_IF: {
            if (o->ifc.have_else) {
                NCDBlock_Free(&o->ifc.else_block);
            }
            
            NCDIfBlock_Free(&o->ifc.ifblock);
        } break;
        
        default: ASSERT(0);
    }
    
    free(o->name);
}

int NCDStatement_Type (NCDStatement *o)
{
    return o->type;
}

const char * NCDStatement_Name (NCDStatement *o)
{
    return o->name;
}

const char * NCDStatement_RegObjName (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_REG)
    
    return o->reg.objname;
}

const char * NCDStatement_RegCmdName (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_REG)
    
    return o->reg.cmdname;
}

NCDValue * NCDStatement_RegArgs (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_REG)
    
    return &o->reg.args;
}

NCDIfBlock * NCDStatement_IfBlock (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_IF)
    
    return &o->ifc.ifblock;
}

void NCDStatement_IfAddElse (NCDStatement *o, NCDBlock else_block)
{
    ASSERT(o->type == NCDSTATEMENT_IF)
    ASSERT(!o->ifc.have_else)
    
    o->ifc.have_else = 1;
    o->ifc.else_block = else_block;
}

NCDBlock * NCDStatement_IfElse (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_IF)
    
    if (!o->ifc.have_else) {
        return NULL;
    }
    
    return &o->ifc.else_block;
}

NCDBlock NCDStatement_IfGrabElse (NCDStatement *o)
{
    ASSERT(o->type == NCDSTATEMENT_IF)
    ASSERT(o->ifc.have_else)
    
    o->ifc.have_else = 0;
    
    return o->ifc.else_block;
}

void NCDIfBlock_Init (NCDIfBlock *o)
{
    LinkedList1_Init(&o->ifs_list);
}

void NCDIfBlock_Free (NCDIfBlock *o)
{
    LinkedList1Node *ln;
    while (ln = LinkedList1_GetFirst(&o->ifs_list)) {
        struct IfBlockIf *e = UPPER_OBJECT(ln, struct IfBlockIf, ifs_list_node);
        NCDIf_Free(&e->ifc);
        LinkedList1_Remove(&o->ifs_list, &e->ifs_list_node);
        free(e);
    }
}

int NCDIfBlock_PrependIf (NCDIfBlock *o, NCDIf ifc)
{
    struct IfBlockIf *e = malloc(sizeof(*e));
    if (!e) {
        return 0;
    }
    
    LinkedList1_Prepend(&o->ifs_list, &e->ifs_list_node);
    e->ifc = ifc;
    
    return 1;
}

NCDIf * NCDIfBlock_FirstIf (NCDIfBlock *o)
{
    LinkedList1Node *ln = LinkedList1_GetFirst(&o->ifs_list);
    if (!ln) {
        return NULL;
    }
    
    struct IfBlockIf *e = UPPER_OBJECT(ln, struct IfBlockIf, ifs_list_node);
    
    return &e->ifc;
}

NCDIf * NCDIfBlock_NextIf (NCDIfBlock *o, NCDIf *ei)
{
    ASSERT(ei)
    
    struct IfBlockIf *cur_e = UPPER_OBJECT(ei, struct IfBlockIf, ifc);
    
    LinkedList1Node *ln = LinkedList1Node_Next(&cur_e->ifs_list_node);
    if (!ln) {
        return NULL;
    }
    
    struct IfBlockIf *e = UPPER_OBJECT(ln, struct IfBlockIf, ifs_list_node);
    
    return &e->ifc;
}

NCDIf NCDIfBlock_GrabIf (NCDIfBlock *o, NCDIf *ei)
{
    ASSERT(ei)
    
    struct IfBlockIf *e = UPPER_OBJECT(ei, struct IfBlockIf, ifc);
    
    NCDIf old_ifc = e->ifc;
    
    LinkedList1_Remove(&o->ifs_list, &e->ifs_list_node);
    free(e);
    
    return old_ifc;
}

void NCDIf_Init (NCDIf *o, NCDValue cond, NCDBlock block)
{
    o->cond = cond;
    o->block = block;
}

void NCDIf_Free (NCDIf *o)
{
    NCDValue_Free(&o->cond);
    NCDBlock_Free(&o->block);
}

void NCDIf_FreeGrab (NCDIf *o, NCDValue *out_cond, NCDBlock *out_block)
{
    *out_cond = o->cond;
    *out_block = o->block;
}

NCDValue * NCDIf_Cond (NCDIf *o)
{
    return &o->cond;
}

NCDBlock * NCDIf_Block (NCDIf *o)
{
    return &o->block;
}
