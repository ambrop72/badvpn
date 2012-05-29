#include <stdlib.h>

#include <misc/debug.h>

#include "NCDSugar.h"

struct desugar_state {
    NCDProgram *prog;
    size_t template_name_ctr;
};

static int add_template (struct desugar_state *state, NCDBlock block, NCDValue *out_name_val);
static int desugar_block (struct desugar_state *state, NCDBlock *block);
static int desugar_if (struct desugar_state *state, NCDBlock *block, NCDStatement *stmt, NCDStatement **out_next);

static int add_template (struct desugar_state *state, NCDBlock block, NCDValue *out_name_val)
{
    char name[40];
    snprintf(name, sizeof(name), "__tmpl%zu", state->template_name_ctr);
    state->template_name_ctr++;
    
    if (!desugar_block(state, &block)) {
        NCDBlock_Free(&block);
        return 0;
    }
    
    NCDProcess proc_tmp;
    if (!NCDProcess_Init(&proc_tmp, 1, name, block)) {
        NCDBlock_Free(&block);
        return 0;
    }
    
    if (!NCDProgram_PrependProcess(state->prog, proc_tmp)) {
        NCDProcess_Free(&proc_tmp);
        return 0;
    }
    
    if (!NCDValue_InitString(out_name_val, name)) {
        return 0;
    }
    
    return 1;
}

static int desugar_block (struct desugar_state *state, NCDBlock *block)
{
    NCDStatement *stmt = NCDBlock_FirstStatement(block);
    
    while (stmt) {
        switch (NCDStatement_Type(stmt)) {
            case NCDSTATEMENT_REG: {
                stmt = NCDBlock_NextStatement(block, stmt);
            } break;
            
            case NCDSTATEMENT_IF: {
                if (!desugar_if(state, block, stmt, &stmt)) {
                    return 0;
                }
            } break;
            
            default: ASSERT(0);
        }
    }
    
    return 1;
}

static int desugar_if (struct desugar_state *state, NCDBlock *block, NCDStatement *stmt, NCDStatement **out_next)
{
    ASSERT(NCDStatement_Type(stmt) == NCDSTATEMENT_IF)
    
    NCDValue args;
    NCDValue_InitList(&args);
    
    NCDIfBlock *ifblock = NCDStatement_IfBlock(stmt);
    
    while (NCDIfBlock_FirstIf(ifblock)) {
        NCDIf ifc = NCDIfBlock_GrabIf(ifblock, NCDIfBlock_FirstIf(ifblock));
        
        NCDValue if_cond;
        NCDBlock if_block;
        NCDIf_FreeGrab(&ifc, &if_cond, &if_block);
        
        if (!NCDValue_ListAppend(&args, if_cond)) {
            NCDValue_Free(&if_cond);
            NCDBlock_Free(&if_block);
            goto fail;
        }
        
        NCDValue action_arg;
        if (!add_template(state, if_block, &action_arg)) {
            goto fail;
        }
        
        if (!NCDValue_ListAppend(&args, action_arg)) {
            NCDValue_Free(&action_arg);
            goto fail;
        }
    }
    
    if (NCDStatement_IfElse(stmt)) {
        NCDBlock else_block = NCDStatement_IfGrabElse(stmt);
        
        NCDValue action_arg;
        if (!add_template(state, else_block, &action_arg)) {
            goto fail;
        }
        
        if (!NCDValue_ListAppend(&args, action_arg)) {
            NCDValue_Free(&action_arg);
            goto fail;
        }
    }
    
    NCDStatement new_stmt;
    if (!NCDStatement_InitReg(&new_stmt, NCDStatement_Name(stmt), NULL, "embcall2_multif", args)) {
        goto fail;
    }
    
    stmt = NCDBlock_ReplaceStatement(block, stmt, new_stmt);
    
    *out_next = NCDBlock_NextStatement(block, stmt);
    return 1;
    
fail:
    NCDValue_Free(&args);
    return 0;
}

int NCDSugar_Desugar (NCDProgram *prog)
{
    struct desugar_state state;
    state.prog = prog;
    state.template_name_ctr = 0;
    
    for (NCDProcess *proc = NCDProgram_FirstProcess(prog); proc; proc = NCDProgram_NextProcess(prog, proc)) {
        if (!desugar_block(&state, NCDProcess_Block(proc))) {
            return 0;
        }
    }
    
    return 1;
}
