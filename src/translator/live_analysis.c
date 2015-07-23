/* live_analysis.c
   
   Implementation of live analysis & dead code elimination
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <macros.h>

#include "config.h"

#include "reg.h"

#include "CFG.h"
#include "InstrNode.h"
#include "bit_vector.h"

#include "sma.h"
#include "AllocStat.h"
#include "fast_mem_allocator.h"
#include "method_inlining.h"

extern int num;
extern CFG* cfg;

static SMA_Session * session;
static int bitvec_size;

inline static void
set_live_except_vn_vars(word *live) 
{
    int i;
    for (i = 0; i < CFG_GetNumOfLocalVars(cfg) +
            CFG_GetNumOfStackVars(cfg) + 11;i++) {
        BV_SetBit(live, i);
    } 
}

inline static void
kill_vn_vars(word *live) 
{
    int i = CFG_GetNumOfLocalVars(cfg) 
        + CFG_GetNumOfStackVars(cfg) + 11;

    for (; i < CFG_GetNumOfVars(cfg); i++) {
        BV_ClearBit(live, i);
    }
}

inline static void
initialize_live_bitvector_from_h(word *live, AllocStat *h) 
{
    int i;

    BV_ClearAll(live, bitvec_size);
    for (i = 0; i < CFG_GetNumOfVars(cfg); i++) {
        if (Reg_IsRegister(AllocStat_Find(h, i))) {
            BV_SetBit(live, i);
        } else {
            BV_ClearBit(live, i);
        }
    }
}

extern AllocStat *RegAlloc_Exception_Returning_Map;
extern int RegAlloc_Exception_Caller_Local_NO;
extern int RegAlloc_Exception_Caller_Stack_NO;

inline static void
update_live_information(word *live, InstrNode *inst) 
{
    int i;
    int dest_v = Instr_GetDestReg(inst, 0);
    InlineGraph *graph;
    int depth;
    int local_offset;
    int stack_offset;
    int local_no;

    // First step: marking killed variables as dead
    //
    // We mark destination register of the instruction as dead.
    // For call, we must mark its return variables as dead additionally.
    // and we must mark its argument variables as live additionally.
#ifdef UPDATE_CONSTANT_PROPAGATION
    if (Var_IsVariable(dest_v)
         && !Instr_IsCondMove(inst)) {
#else
    if (Var_IsVariable(dest_v)) {
#endif /* UPDATE_CONSTANT_PROPAGATION */
        // Mark the destination local variable as dead.
        BV_ClearBit(live, CFG_get_var_number(dest_v));
    }
    if (Instr_IsCall(inst)) {
        // Mark the return variable as dead.
        for (i = 0; i < Instr_GetNumOfRets(inst); i++) {
            assert(Var_IsVariable(Instr_GetRet(inst,i)));
            BV_ClearBit(live, CFG_get_var_number(Instr_GetRet(inst, i)));
        }
        for (i = 0;i < Instr_GetNumOfArgs(inst); i++) {
            assert (Var_IsVariable(Instr_GetArg(inst,i)));
            BV_SetBit(live, CFG_get_var_number(Instr_GetArg(inst,i)));
        }
    }


    // Second step: marking used variables as live
    // We mark source registers of the instruction as live
    // For return, we mark its return variables as live.

    // if the instruction is nop, it's source registers which are marked 
    // as 'last use' are removed from live set
    //  -> It's because our simple live analysis is not correct.
    if (Instr_IsNop(inst)) {
        for (i = 0; i < 3 ; i++) {
            int v = inst->lastUsedRegs[i];
            if (Var_IsVariable(v))
              BV_ClearBit(live, CFG_get_var_number(v));
        }
    } else {
        for (i = 0; i < Instr_GetNumOfSrcRegs(inst); i++) {
            int v = Instr_GetSrcReg(inst, i);
            if (Var_IsVariable(v) && ! BV_IsSet(live, CFG_get_var_number(v))) {
                // The source variable is dead after this inst.
                // Mark the source variable as last use
                // and also mark it as live.
                Instr_SetLastUseOfSrc(inst, i);
                BV_SetBit(live, CFG_get_var_number(v));
            }
        }
    }

    if (Instr_IsReturn(inst)) {
        // At return, mark all variables as dead
        BV_ClearAll(live, bitvec_size);
        for (i = 0; i < Instr_GetNumOfRets(inst); i++) {
            BV_SetBit(live, CFG_get_var_number(Instr_GetRet(inst, i)));
            assert(Var_IsVariable(Instr_GetRet(inst,i)));
        }
    }

/*      if (Instr_GetNumOfSuccs(inst) == 0  */
/*          && RegAlloc_Exception_Returning_Map != NULL) { */
    if (Instr_IsExceptionReturn(inst)) {
        assert(RegAlloc_Exception_Returning_Map != NULL);
        
        BV_ClearAll(live, bitvec_size);
        for (i = 0; i < RegAlloc_Exception_Caller_Local_NO; i++) {
            BV_SetBit(live, i);
        }
        for (i = CFG_GetNumOfLocalVars(cfg); 
             i < CFG_GetNumOfLocalVars(cfg) 
                 + RegAlloc_Exception_Caller_Stack_NO; i++) {
            BV_SetBit(live, i);
        }
    }

    // Third step: mark additional used variables as live
    // We consider exception-generatable instruction.
    // Above exception-generatable instr., all local variables
    // are live
    switch (Instr_NeedMapInfo(inst)) {
      case NO_NEED_MAP_INFO:
        break;
      case NEED_RET_ADDR_INFO:
      case NEED_MAP_INFO:
        graph = Instr_GetInlineGraph(inst);
        assert(graph != NULL);
        depth = IG_GetDepth(graph);
        local_offset = IG_GetLocalOffset(graph);
        stack_offset = IG_GetStackOffset(graph);
        local_no = Method_GetLocalSize(IG_GetMethod(graph));
        // current & callers' local variables should be live.
        // callers' local variables should be live.
        for(i = 0; i < local_offset + local_no; i++) {
            BV_SetBit(live, i);
        }
        // callers' stack variables should be live.
        for (i = CFG_GetNumOfLocalVars(cfg); 
             i < CFG_GetNumOfLocalVars(cfg) + stack_offset;
             i++) {
            BV_SetBit(live, i);
        }
        break;
      case NEED_LOCAL0_MAP_INFO:
        graph = Instr_GetInlineGraph(inst);
        assert(graph != NULL);
        depth = IG_GetDepth(graph);
        local_offset = IG_GetLocalOffset(graph);
        stack_offset = IG_GetStackOffset(graph);
        // callers' local variables should be live.
        // current local variable 0 should be live.
        // callers' local variables should be live.
        for (i = 0; i < local_offset + 1; i++) {
            BV_SetBit(live, i);
        }
        // callers' stack variables should be live.
        for (i = CFG_GetNumOfLocalVars(cfg); 
             i < CFG_GetNumOfLocalVars(cfg) + stack_offset;
             i++) {
            BV_SetBit(live, i);
        }

        break;
    }
}



static void
one_pass_live_analysis(InstrNode *instr, word* live)  
{
    int i;

    Instr_MarkVisited(instr);

    if (Instr_GetLive(instr)) {
        BV_AND(live, Instr_GetLive(instr), bitvec_size);
    }
    
    if (Instr_GetNumOfSuccs(instr) == 1) {
        InstrNode * succ = Instr_GetSucc(instr, 0);
        if (Instr_IsVisited(succ)) {
            if (Instr_GetLive(succ)) {
                BV_Copy(live, Instr_GetLive(succ), bitvec_size);
            } else {
                set_live_except_vn_vars(live);
            }
        } else {
            one_pass_live_analysis(Instr_GetSucc(instr, 0), live);
        }
    } else {
        for (i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
            InstrNode * succ = Instr_GetSucc(instr, i);
            if (Instr_IsVisited(succ)) {
                if (Instr_GetLive(succ)) {
                    BV_OR(live, Instr_GetLive(succ), bitvec_size);
                } else {
                    set_live_except_vn_vars(live);
                }
            } else {
                word live2[bitvec_size];
                word * lv;
                BV_ClearAll(live2, bitvec_size);

                one_pass_live_analysis(succ, live2);
                
                // save liveness information at the basic block header
                lv = FMA_calloc(bitvec_size * sizeof(word));
                BV_Copy(lv, live2, bitvec_size);
//                Instr_SetLive(instr, lv);
                Instr_SetLive(succ, lv);
                
                // merge liveness information from the successors
                BV_OR(live, live2, bitvec_size);
            }
        }
    }

    update_live_information(live, instr);
}


/* Name        : LA_live_analysis
   Description : perform live analysis for the whole CFG
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
      CFG_PrepareTraversal(cfg) should be called just before calling.
   Post-condition:
      All instructions are marked as visited.
   Notes:  */
void 
LA_live_analysis(InstrNode * root) 
{
    word *live;
    bitvec_size = (CFG_GetNumOfVars(cfg) - 1) / 32 + 1;

    live = FMA_calloc(sizeof(word) * bitvec_size);
    BV_ClearAll(live, bitvec_size);

    one_pass_live_analysis(root, live);

    Instr_SetLive(root, live);
}


static void
dead_code_elimination(InstrNode *instr, word* live) 
{
    int i;

    if (Instr_GetNumOfSuccs(instr) == 1) {
        InstrNode * succ = Instr_GetSucc(instr, 0);
        if (Instr_IsRegionEnd(succ)) {
            if (Instr_GetH(succ)) {
                initialize_live_bitvector_from_h(live, Instr_GetH(succ));
            } else {
                BV_SetAll(live, bitvec_size);
                kill_vn_vars(live);
            }
        } else {
            dead_code_elimination(Instr_GetSucc(instr, 0), live);
        }
    } else if (Instr_GetNumOfSuccs(instr) > 1) {
        for (i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
            InstrNode * succ = Instr_GetSucc(instr, i);
            if (Instr_IsRegionEnd(succ)) {
                word live2[bitvec_size];
                if (Instr_GetH(succ)) {
                    initialize_live_bitvector_from_h(live2, Instr_GetH(succ));
                } else {
                    BV_SetAll(live2, bitvec_size);
                    kill_vn_vars(live2);
                }
                BV_OR(live, live2, bitvec_size);
            } else {
                word * lv = SMA_Alloc(session, sizeof(word) * bitvec_size);
                BV_ClearAll(lv, bitvec_size);

                dead_code_elimination(succ, lv);

                Instr_SetLive(succ, lv);
                
                // merge liveness information from the successors
                BV_OR(live, lv, bitvec_size);
            }
        }
    } else {
        // no successors
        if (Instr_GetH(instr)) {
            initialize_live_bitvector_from_h(live, Instr_GetH(instr));
        } else {
            if (Instr_IsExceptionReturn(instr)) {
/*              if (RegAlloc_Exception_Returning_Map != NULL) { */
		AllocStat *map = RegAlloc_Exception_Returning_Map;
                assert(RegAlloc_Exception_Returning_Map != NULL);
                initialize_live_bitvector_from_h(live, map);
            } else {
                BV_ClearAll(live, bitvec_size);
            }
        }
    }

    // Delete dead instruction
    // If the destination variable is dead, generally it is dead instruction.
    // But, if the destination variable is FVOID type, we must check
    // FDOUBLE type variable.
    if (Var_IsVariable(Instr_GetDestReg(instr,0)) 
        && !BV_IsSet(live, CFG_get_var_number(Instr_GetDestReg(instr, 0)))
        && !((Var_GetType(Instr_GetDestReg(instr,0)) == T_FVOID)
             && BV_IsSet(live, CFG_get_var_number(Instr_GetDestReg(instr, 0))-1))){
        // possibly dead instruction
        if(!Instr_IsExceptionGeneratable(instr) &&
            instr->instrCode != SDIV && instr->instrCode != UDIV &&
            instr->instrCode != SMUL && instr->instrCode != UMUL &&
            instr->instrCode != WRY) {

            instr->format = FORMAT_6;
            instr->instrCode = ADD;
            instr->fields.format_6.destReg = g0;

            /* Without the following initialization of source registers,
               an out-of-range register may be delivered to loop optimization.
               For example, "add g0,0xa,ist57" will be modified to 
               "add g0,lv131072,g0". lv131072 is out of range, thus it
               causes a malfunction of loop optimization. */
            instr->fields.format_6.srcReg_1 = g0;
            instr->fields.format_6.srcReg_2 = g0;

            Instr_UnsetUnresolved(instr);
            
        }
    }

    update_live_information(live, instr);
}


/* Name        : LA_eliminate_dead_code_in_region
   Description : delete any dead code in the region
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
void 
LA_eliminate_dead_code_in_region(SMA_Session *ses, InstrNode * header) 
{
    word *lv;
    bitvec_size = (CFG_GetNumOfVars(cfg) - 1) / 32 + 1;

    session = ses;
    
    if (Instr_GetLive(header)) {
        lv = Instr_GetLive(header);
    } else {
        lv = FMA_calloc(sizeof(word) * bitvec_size);
        BV_ClearAll(lv, bitvec_size);
        Instr_SetLive(header, lv);
    }
    dead_code_elimination(header, lv);
}

/* Name        : LA_attach_live_info_for_inlining
   Description : make variables used within inlined method dead at exit
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
LA_attach_live_info_for_inlining(CFG *cfg, InlineGraph *graph)
{
    int variable_no;
    int local_no, stack_no;
    int caller_local_no, caller_stack_no;
    int bitvec_size;
    word *lv;
    int i;
    
    variable_no = CFG_GetNumOfVars(cfg);
    local_no = CFG_GetNumOfLocalVars(cfg);
    stack_no = CFG_GetNumOfStackVars(cfg);
    caller_local_no = IG_GetLocalOffset(graph);
    caller_stack_no = IG_GetStackOffset(graph);
    
    bitvec_size = (variable_no - 1) / 32 + 1;
    lv = FMA_calloc(sizeof(word) * bitvec_size);
    
    BV_SetAll(lv, bitvec_size);

    for(i = caller_local_no; i < local_no; i++) BV_ClearBit(lv, i);
    for(i = caller_stack_no + local_no; i < local_no + stack_no; i++)
	BV_ClearBit(lv, i);
    
    Instr_SetLive(IG_GetTail(graph), lv);
}
