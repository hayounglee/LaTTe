/* code_gen.c
   
   code generator for the JIT compiler
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "CFG.h"
#include "InstrNode.h"
#include "jit.h"
#include "probe.h"

#ifdef USE_CODE_ALLOCATOR
#include "mem_allocator_for_code.h"
#endif

#include "exception_info.h"
#include "method_inlining.h"
#include "translate.h"
#include "code_gen.h"


#ifdef CUSTOMIZATION
#include "SpecializedMethod.h"
#endif
#ifdef INLINE_CACHE
#include "TypeChecker.h"
#include "CallSiteInfo.h"
#endif
#ifdef METHOD_COUNT
#include "retranslate.h"
#endif
#ifdef DYNAMIC_CHA
#include "dynamic_cha.h"
#endif

static const int BlockSize = 32;


//
// for exception handling
//
#include "exception_info.h"
#include "exception_handler.h"



#define DBG(s)   
#define EDBG(s)
#define PDBG(s)	
#define CDBG(s)   
#define HDBG(s)   

extern CFG *cfg;

extern FILE*   outout;
//
// for profiling
//
PDBG(
   extern unsigned  num_of_bytecode_instr;
   static unsigned  num_of_risc_instr;
   static float     risc_per_bytecode;

   static unsigned long long total_num_of_bytecode_instr = 0;
   static unsigned long long total_num_of_risc_instr = 0;
   static float              total_risc_per_bytecode;
);

#ifdef NEW_COUNT_RISC
//
// for dynamic probing
//
static void   _CodeGen_insert_profile_code(CFG* cfg, InstrNode* instr);

static
void
CodeGen_insert_profile_code(CFG* cfg)
{
    int    i;

    CFG_PrepareTraversal(cfg);

    _CodeGen_insert_profile_code(cfg, cfg->root);

    for (i = 0; i < CFG_GetNumOfFinallyBlocks(cfg); i++) {
        FinallyBlockInfo*   fb_info = CFG_GetFinallyBlockInfo(cfg, i);

        _CodeGen_insert_profile_code(cfg, FBI_GetHeader(fb_info));
    }
}

static
void
_CodeGen_insert_profile_code( CFG* cfg, InstrNode* instr)
{
    int         i;
    InstrNode*  header;
    int         num_of_bb_instrs = 0;

    assert(instr);

    if (Instr_IsVisited(instr)) {
        return;
    }

    header = instr;

    //
    // count the number of instructions in a basic block
    //
    while (Instr_GetNumOfSuccs(instr) == 1 
           && Instr_GetNumOfPreds(Instr_GetSucc(instr, 0)) == 1) {
        num_of_bb_instrs++;
        Instr_MarkVisited(instr);
        instr = Instr_GetSucc(instr, 0);
        if (Instr_IsCall(instr)) break;
    }

    assert(CFG_IsBBEnd(cfg, instr) || Instr_IsCall(instr));
    num_of_bb_instrs++;
    Instr_MarkVisited(instr);


    //
    // If the basic block contains only one instruction, which is a delay slot of
    // an annuled branch, skip insertion.
    //

    //
    // insert probe code at the end of the basic block
    //
    if (CFG_GetRoot(cfg) == header) {
        if (num_of_bb_instrs > 1) {
            insert_probe_code(cfg, Instr_GetSucc(header, 0), num_of_bb_instrs);
        } else {
            InstrNode * nop = create_format2_instruction(SETHI, g0, 0, -1);
            CFG_InsertInstrAsSucc(cfg, header, 0, nop);
            Instr_MarkVisited(nop);
            insert_probe_code(cfg, nop, num_of_bb_instrs);
        }
    } else {
        insert_probe_code(cfg, header, num_of_bb_instrs);
    }

    for (i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
        assert(Instr_IsPred(Instr_GetSucc(instr, i), instr));

        _CodeGen_insert_profile_code(cfg, Instr_GetSucc(instr, i));
    }
}
#endif /* NEW_COUNT_RISC */

inline
static
void
CodeGen_set_stack_size(CFG *cfg) {
    InstrNode *c_instr = CFG_GetRoot(cfg);
    InstrNode *n1_instr, *n2_instr;
    int stack_size = CFG_GetStackSize(cfg);

    if (stack_size >= 0x1000) {
        
        n1_instr = create_format5_instruction(OR, g1, g1, 
                                              LO(-stack_size), 0);
        CFG_InsertInstrAsSinglePred(cfg, c_instr, n1_instr);
        n2_instr = create_format2_instruction(SETHI, g1, 
                                              HI(-stack_size), 0);
        CFG_InsertInstrAsSinglePred(cfg, n1_instr, n2_instr);

        // make root instruction as save %sp, %g1, %sp
	// instead of save %sp, offset, %sp
	// instruction format and second operand changed.
        c_instr->format = FORMAT_4;
        c_instr->fields.format_4.srcReg_2 = g1;
        cfg->root = n2_instr;
    } else {
        c_instr->fields.format_5.simm13 = - stack_size;
    }
}

/* Name        : duplicate_exception_info
   Description : increase map pool size if necessary
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: If an instruction that needs map info is duplicated, the
          duplicated instruction also needs map info.  */
inline static
void
duplicate_exception_info(CFG *cfg, InstrNode *instr)
{
    InlineGraph *graph;
    int map_size = 0;
    
    switch (Instr_NeedMapInfo(instr)) {
      case NO_NEED_MAP_INFO:
	map_size = 0;
	break;
      case NEED_MAP_INFO:
      case NEED_RET_ADDR_INFO:
        graph = Instr_GetInlineGraph(instr);
        map_size = IG_GetLocalOffset(graph) + IG_GetStackOffset(graph)
            + Method_GetLocalSize(IG_GetMethod(graph)) + MAX_TEMP_VAR;
	break;
      case NEED_LOCAL0_MAP_INFO:
	map_size = 1;
	break;
      default:
	assert(false && "control should not be here!\n");
    }

    CFG_ExpandMapPool(cfg, map_size);
}

//
// *** Stages of the code generation ***
// 1. Delay slot filling and other update
// 2. Basic Block Positioning
// 3. `ba' filling with PC assigning
//


///
/// Function Name : CodeGen_fill_delay_slot
/// Author : Yang, Byung-Sun
/// Input
///    instr - instruction node
/// Output
///    If the instruction is CTI, an instruction for its delay slot is
///    determined by this function.
/// Pre-Condition
/// Post-Condition
/// Description
///    The sufficient requirement for the predecessor to be delayed is
///        - instr is not join
///        - predecessor is not join
///        - predecessor is not CTI
///
///    A. for indirect call instruction,
///        - check data dependence of the predecessor
///        - if failed, put nop
///    B. for direct call instruction,
///        - if the sufficient requirement is met, use it
///        - Otherwise, put nop
///    C. for branch instruction (aussuming that no 'ba' will appear),
///        - if the both successors are the same and they can be delayed,
///          use them as a delayed slot
///        - if the second successor can be delayed, make it delayed and
///          set the annul bit of instr. If the successor is join, split it.
///        - if failed, put nop
///    D. for table jump,
///        - check data dependence of the predecessor
///        - if failed, put nop
///    E. for return
///        - put restore
///
///    The condition for the second successor can be annulled is
///        - it is not CTI
///        - it is not delayed.
///
inline
static
void
CodeGen_fill_delay_slot(CFG* cfg, InstrNode* instr)
{
    InstrNode*   pred;
    bool    pred_can_be_delayed;

    assert(Instr_IsCTI(instr));

    pred = Instr_GetNumOfPreds(instr) ? Instr_GetPred(instr, 0) : 0;

    pred_can_be_delayed =
        pred && !Instr_IsJoin(instr) 
        && !Instr_IsJoin(pred) && !Instr_IsCTI(pred) 
        && CFG_GetRoot(cfg) != pred; // this is added for OPTIM_INVOKESTATIC
                                               
    if (Instr_IsIndirectCall(instr) 
        || Instr_IsTableJump(instr)) {
        if (pred_can_be_delayed 
            && Instr_GetDestReg(pred, 0) != Instr_GetSrcReg(instr, 0)) {
            Instr_SetDelayed(pred);
        } else {
            InstrNode*   nop = Instr_create_nop();
            CFG_InsertInstrAsSinglePred(cfg, instr, nop);
            Instr_SetDelayed(nop);
        }

    } else if (Instr_IsDirectCall(instr)) {
#ifdef INLINE_CACHE
        // 'call' is expanded at this point to preserve the sequence
        //   add g0, dtable_index+1, g2
        //   add g0, 0, g4
        //   call
        //   nop (delay slot)
        if (Instr_IsStartOfInlineCache(instr)) {
            InstrNode * new_instr = NULL;
            int     dtable_offset = 
                Instr_GetDTableIndex(instr) * 4;

            assert(dtable_offset < (int)0x1000
                   && dtable_offset >= (int)0xfffff000);
            
            new_instr = create_format5_instruction(ADD, g2, g0, 
                                                    dtable_offset, -1);
            CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
            set_pinned_instr(new_instr);

            // modified by lordljh
            new_instr = create_format5_instruction(ADD, g4, g0, 0, -1);
            CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);

            new_instr = Instr_create_nop();
            CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
            Instr_SetDelayed(new_instr);

        } else 
#endif /* INLINE_CACHE */
        if (pred_can_be_delayed) {
            Instr_SetDelayed(pred);
        } else {
            InstrNode*   nop = Instr_create_nop();
            CFG_InsertInstrAsSinglePred(cfg, instr, nop);
            Instr_SetDelayed(nop);
        }
            
    } else if (Instr_IsBranch(instr)) {
#ifdef DYNAMIC_CHA
        if (Instr_GetCode(instr) == SPEC_INLINE)
          ; // do nothing
        else
#endif

        if (pred_can_be_delayed && !Instr_IsSetcc(pred)) {
            Instr_SetDelayed(pred);
        } else {
#ifdef NEW_COUNT_RISC
            InstrNode*   nop = Instr_create_nop();
            CFG_InsertInstrAsSinglePred(cfg, instr, nop);
            Instr_SetDelayed(nop);
#else
            InstrNode*   succ1 = Instr_GetSucc(instr, 1);

            assert(instr->instrCode != BA);

            //
            // If the succ0 and succ1 are the same, they can be delayed.
            // But it is not implemented now.
            //
            if (!Instr_IsCTI(succ1) && !Instr_IsDelayed (succ1)
#ifdef INLINE_CACHE
                && !is_pinned_instr(succ1)
#endif
                && Instr_GetNumOfSuccs(succ1) > 0) {
                InstrNode*   new_succ = Instr_GetSucc(succ1, 0);

                if (Instr_IsJoin(succ1)) {
                    // create a clone of succ1 and
                    // place it in front of instr as a delay slot
                    InstrNode*   clone = Instr_Clone(succ1);

		    /* increase map pool size if this instruction have
                       map pool */
		    duplicate_exception_info(cfg, succ1);
		    
                    Instr_disconnect_instruction(instr, succ1);
		    
                    CFG_InsertInstrAsSinglePred(cfg, instr, clone);
                    if (Instr_IsUnresolved(succ1)) {
                        add_new_resolve_instr(cfg, clone,
                                              get_resolve_data(Instr_GetResolveInfo(succ1)));
                    }
                    Instr_MarkVisited(clone);
                    Instr_SetDelayed(clone);

                    Instr_SetAnnulling(instr);
                    Instr_connect_instruction(instr, Instr_GetSucc(succ1, 0));
                } else {
                    
                    // move succ1 to the predecessor of instr
                    Instr_disconnect_instruction(instr, succ1);
                    Instr_disconnect_instruction(succ1, Instr_GetSucc(succ1, 0));
                    Instr_connect_instruction(instr, new_succ);

                    assert(Instr_GetNumOfPreds(succ1) == 0);
                    assert(Instr_GetNumOfSuccs(succ1) == 0);

                    CFG_InsertInstrAsSinglePred(cfg, instr, succ1);
                    Instr_MarkVisited(succ1);
                    Instr_SetDelayed(succ1);
                    Instr_SetAnnulling(instr);
                }
            } else {
                InstrNode*   nop = Instr_create_nop();
                CFG_InsertInstrAsSinglePred(cfg, instr, nop);
                Instr_SetDelayed(nop);
            }
#endif // NEW_COUNT_RISC
        }


    } else if (Instr_IsReturn(instr)) {
        InstrNode*  restore = create_format6_instruction(RESTORE, g0, g0, g0,
                                                          -1);
        CFG_InsertInstrAsSinglePred(cfg, instr, restore);
        Instr_SetDelayed(restore);
    } else {
        assert(0 && "control should not reach here!!!");
    }
}




///
/// Function Name : CodeGen_update_cfg
/// Author : Yang, Byung-Sun
/// Input
///    CFG - control flow graph of register allocated code
/// Output
///    The cfg is updated.
/// Pre-Condition
/// Post-Condition
/// Description
///    This function performs delay slot filling currently.
///    Other probing code might be inserted at this stage.
///    All delayed instructions are placed in front of CTIs.
///
static
void
_CodeGen_update_cfg(CFG* cfg, InstrNode* instr)
{
    int          i;

    if (Instr_IsVisited(instr)) {
        return;
    }

    Instr_MarkVisited(instr);
    if (Instr_IsCTI(instr)) {
	CodeGen_fill_delay_slot(cfg, instr);
    }
    
    for (i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
        _CodeGen_update_cfg(cfg, Instr_GetSucc(instr, i));
    }
}

static
void
CodeGen_update_cfg(CFG* cfg)
{
    CFG_PrepareTraversal(cfg);
    _CodeGen_update_cfg(cfg, cfg->root);
}


///
/// Function Name : CodeGen_position_basic_blocks
/// Author : Yang, Byung-Sun
/// Input
///     cfg - control flow graph of the register allocated code
/// Output
///     The list of basic block headers are constructued.
/// Pre-Condition
///     The taken path of the loop checking code is translated as
///     a fall-through path in the second stage. This is to make
///     the check code and loop body contiguous.
/// Post-Condition
/// Description
///     The order of basic blocks follows the reverse postorder.
///     The previous one was the preorder which is very poor
///     in the spatial locality.
///     Actually, this function will generate a list of basic block headers
///     in the postorder. If the list is access from the end, the sequence
///     will be reverse postorder.
///
///     ************* This is a kind of hacking. ******************
///     If the header of a basic block is an loop entry and
///     the exit of the block is a branch, predict the branch taken.
///     The reason the prediction is done in this function is that
///     the function considers the whole of a basic block.
///
///           --- OUTLINING by doner ---
///     Exceptional flow such as "ta" instruction had better be placed
///     at the end of the method. In order for this, exceptional_headers
///     is recorded. LaTTe considers the basic blocks listed in
///     "exceptional_headers" after those in "bb_headers".
///

static int         max_num_of_bb;    // maximum number of basic blocks
static int         num_of_bb;        // number of basic blocks
static InstrNode** bb_headers;       // list of basic block headers

#ifdef OUTLINING
#define   MAX_NUM_OF_EXCEPTIONAL_HEADERS   500
static int         num_of_ebh;      // number of exceptional basic block headers
static InstrNode*  exceptional_headers[MAX_NUM_OF_EXCEPTIONAL_HEADERS];
#endif // OUTLINING

inline
static
void
CodeGen_add_basic_block_header(CFG* cfg, InstrNode* instr)
{
    assert(num_of_bb < max_num_of_bb);
    bb_headers[num_of_bb++] = instr;
}

#ifdef OUTLINING
inline
static
void
CodeGen_add_exceptional_header(CFG* cfg, InstrNode* instr)
{
    if (num_of_ebh < MAX_NUM_OF_EXCEPTIONAL_HEADERS) {
        exceptional_headers[num_of_ebh++] = instr;
    } else {
        fprintf(stderr, "Too Many Exceptional Basic Blocks!!!\n");
        exit(1);
    }
}

inline
static
int
CodeGen_is_exceptional_header(CFG* cfg, InstrNode* header)
{
    // Currently, a basic block containing only one instruction "ta.."
    // is considered exceptional. - doner

    if (CFG_IsBBEnd(cfg, header) &&    // The block has only one instruction.
         Instr_GetCode(header) == TA) {
        return 1;
    }

    return 0;
}
#endif // OUTLINING


static
void
_CodeGen_position_basic_blocks(CFG* cfg, InstrNode* header)
{
    InstrNode*   instr = header;
    int          i;

    assert(instr != NULL && CFG_IsBBStart(cfg, header));

    if (Instr_IsVisited(header)) {
        return;
    }

    Instr_MarkVisited(header);

    while (!CFG_IsBBEnd(cfg, instr)) {
        instr = Instr_GetSucc(instr, 0);
    }

    //
    // Normally, the code checking the loop condition is taken for the iteration.
    //
    if (Instr_IsLoopHeader(header) && Instr_IsBranch(instr)) {
        Instr_PredictTaken(instr);
    }

#ifdef DFS_CODE_PLACEMENT
#ifdef OUTLINING
    if (!CodeGen_is_exceptional_header(cfg, header)) {
        CodeGen_add_basic_block_header(cfg, header);
    } else {
        CodeGen_add_exceptional_header(cfg, header);
    }
#else
    CodeGen_add_basic_block_header(cfg, header);
#endif
    // now `instr' is a basic block end.
    for (i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
        _CodeGen_position_basic_blocks(cfg, Instr_GetSucc(instr, i));
    }

#else /* not DFS_CODE_PLACEMENT */
    // now `instr' is a basic block end.
    for (i = Instr_GetNumOfSuccs(instr) - 1; i >= 0; i--) {
        _CodeGen_position_basic_blocks(cfg, Instr_GetSucc(instr, i));
    }

#ifdef OUTLINING
    if (!CodeGen_is_exceptional_header(cfg, header)) {
        CodeGen_add_basic_block_header(cfg, header);
    } else {
        CodeGen_add_exceptional_header(cfg, header);
    }
#else
    CodeGen_add_basic_block_header(cfg, header);
#endif // OUTLINING
#endif /* not DFS_CODE_PLACEMENT */
}

inline
static
void
CodeGen_position_basic_blocks(CFG* cfg)
{
    CFG_PrepareTraversal(cfg);
    _CodeGen_position_basic_blocks(cfg, cfg->root);
}

///
/// Function Name : CodeGen_assign_offset
/// Author : Yang, Byung-Sun
/// Input
///     cfg - control flow graph
/// Output
/// Pre-Condition
/// Post-Condition
/// Description
///     Each instruction is assigned its native offset and
///     "ba" is inserted.
///     Actually, the offset is remembered.
///
static
int
CodeGen_assign_offset(CFG* cfg)
{
    int    current_offset = 0;
    int    current_bb_index;

#ifdef DFS_CODE_PLACEMENT
    for (current_bb_index = 0;
         current_bb_index < num_of_bb;
         current_bb_index++) {
#else
    for (current_bb_index = num_of_bb - 1;
         current_bb_index >= 0;
         current_bb_index--) {
#endif
        InstrNode*   instr = bb_headers[current_bb_index];
        bool         delayed_prev = false;

        assert(CFG_IsBBStart(cfg, instr));
        assert(Instr_GetNativeOffset(instr) == -1);

        while (1) {
            if (Instr_GetCode(instr) == DUMMY_OP);
            else if (!Instr_IsDelayed(instr)) {
                Instr_SetNativeOffset(instr, current_offset++);

                if (delayed_prev) {
                    delayed_prev = false;
                    Instr_SetNativeOffset(Instr_GetPred(instr, 0), current_offset++);
                }
            } else {
                delayed_prev = true;
            }
            if (!CFG_IsBBEnd(cfg, instr)) {
                instr = Instr_GetSucc(instr, 0);
            } else {
                break;
            }
        }

        // Now, the bb end has been arrived.
        // If needed, insert "ba".
        if (!Instr_IsBranch(instr)) {
            if (Instr_GetNumOfSuccs(instr) == 1 &&
#ifdef DFS_CODE_PLACEMENT
                (current_bb_index == num_of_bb-1 ||
                 Instr_GetSucc(instr, 0) != bb_headers[current_bb_index+1])) {
#else
                (current_bb_index == 0 ||
                 Instr_GetSucc(instr, 0) != bb_headers[current_bb_index-1])) {
#endif
                InstrNode*   ba_instr = create_format3_instruction(BA, -1);
                CFG_InsertInstrAsSucc(cfg, instr, 0, ba_instr);

                if (!Instr_IsCTI(instr)) {
                    assert(!Instr_IsDelayed(instr));
                    Instr_SetDelayed(instr);
                    Instr_SetNativeOffset(ba_instr, current_offset -1);
                    Instr_SetNativeOffset(instr, current_offset++);
                } else {
                    Instr_SetAnnulling(ba_instr);
                    Instr_SetNativeOffset(ba_instr, current_offset++);
                }
                add_new_resolve_instr(cfg, ba_instr, NULL);
            }
        } else {
#ifdef DFS_CODE_PLACEMENT
            if (current_bb_index == num_of_bb-1 ||
                 Instr_GetSucc(instr, 0) != bb_headers[current_bb_index+1]) {
#else
            if (Instr_GetSucc(instr, 0) !=
                 bb_headers[current_bb_index-1]) {
#endif
                InstrNode*  ba_instr = create_format3_instruction(BA, -1);

                CFG_InsertInstrAsSucc(cfg, instr, 0, ba_instr);
                Instr_SetNativeOffset(ba_instr, current_offset++);
                Instr_SetAnnulling(ba_instr);
                add_new_resolve_instr(cfg, ba_instr, NULL);
            }
        }
    }

#ifdef OUTLINING
#ifdef DFS_CODE_PLACEMENT
    for (current_bb_index = 0;
         current_bb_index < num_of_ebh;
         current_bb_index++) {
#else
    for (current_bb_index = num_of_ebh - 1;
         current_bb_index >= 0;
         current_bb_index--) {
#endif
        InstrNode*   instr = exceptional_headers[current_bb_index];
        bool         delayed_prev = false;

        assert(CFG_IsBBStart(cfg, instr));
        assert(Instr_GetNativeOffset(instr) == -1);

        while (1) {
            if (!Instr_IsDelayed(instr)) {
                Instr_SetNativeOffset(instr, current_offset++);

                if (delayed_prev) {
                    delayed_prev = false;
                    Instr_SetNativeOffset(Instr_GetPred(instr, 0), current_offset++);
                }
            } else {
                delayed_prev = true;
            }
            if (!CFG_IsBBEnd(cfg, instr)) {
                instr = Instr_GetSucc(instr, 0);
            } else {
                break;
            }
        }

        // Now, the bb end has been arrived.
        // If needed, insert "ba".
        if (!Instr_IsBranch(instr)) {
            if (Instr_GetNumOfSuccs(instr) == 1) {
                InstrNode*   ba_instr = create_format3_instruction(BA, -1);
                CFG_InsertInstrAsSucc(cfg, instr, 0, ba_instr);

                if (!Instr_IsCTI(instr)) {
                    assert(!Instr_IsDelayed(instr));
                    Instr_SetDelayed(instr);
                    Instr_SetNativeOffset(ba_instr, current_offset -1);
                    Instr_SetNativeOffset(instr, current_offset++);
                } else {
                    Instr_SetAnnulling(ba_instr);
                    Instr_SetNativeOffset(ba_instr, current_offset++);
                }
                add_new_resolve_instr(cfg, ba_instr, NULL);
            }
        } else {
            InstrNode*  ba_instr = create_format3_instruction(BA, -1);

            CFG_InsertInstrAsSucc(cfg, instr, 0, ba_instr);
            Instr_SetNativeOffset(ba_instr, current_offset++);
            Instr_SetAnnulling(ba_instr);
            add_new_resolve_instr(cfg, ba_instr, NULL);
        }
    }

#endif // OUTLINING

    return current_offset*4;
}


static
void
CodeGen_initialize_data_segment(CFG*    cfg,
                                void*   text_seg,
                                void*   data_seg)
{

    int i;
    DataEntry * p = CFG_GetDataListIterator(cfg);

    while(p) {
        void * initial_value = get_initial_value(p);
        DataType type = get_data_type(p);
        int size = get_data_size(p);
        int offset = get_data_offset(p);

        switch(type) {
          case NORMAL:
            if (size <= 4 && size > 0) {
                memcpy((char *)data_seg + offset, &initial_value, size);
            } else {
                memcpy((char *)data_seg + offset, initial_value, size);
            }
            break;
          case JUMP_TABLE: {
              InstrNode* jmp_instr = ((InstrNode**) initial_value)[0];


              //
              // now, initial_value points to the jmpl.
              //
              for (i = 0; i < size / 4; i++) {
                  InstrNode*   succ;
                  int          target_addr;

                  succ = Instr_GetSucc(jmp_instr,
                                        ((int*) initial_value)[i+1]);
                  target_addr = (int) ((int*)text_seg + Instr_GetNativeOffset(succ));

                  if (Instr_IsDelayed(succ)) {
                      target_addr -= 4;
                  }

                  ((int *)((char*) data_seg + offset))[i] = target_addr;
              }
          }
	     
          break;
          case LOOKUP_TABLE:  {
              InstrNode*  jmp_instr =
                  (InstrNode*) ((int**) initial_value)[size / 8];
              int**       target_list =
                  ((int**)((char *)data_seg + offset)) + size / 8;
              int*        succ_index =
                  ((int*) initial_value) + (size / 8 + 1);


              //
              // now, the first entry of target list points to the jmpl.
              //
              memcpy((char *)data_seg + offset,
                      initial_value, size / 2);// copy key list
              for (i = 0; i < size / 8; i++) {
                  InstrNode*   succ;
                  int          target_addr;

                  succ = Instr_GetSucc(jmp_instr, succ_index[i]);
                  target_addr = (int) ((int*)text_seg +
				       Instr_GetNativeOffset(succ));

                  if (Instr_IsDelayed(succ)) {
                      target_addr -= 4;
                  }                     

                  target_list[i] = (int *) target_addr;
                      
              }
          }
          break;
          default: 
            assert(0 && "Unknown DataEntry type");
        }

        p = p->next;
    }
}



static
void
CodeGen_resolve_unknown_values(CFG *cfg, void* text_seg, void *data_seg)
{
    InstrToResolve *p = get_resolve_instr_list_iterator(cfg);

    while(p) {
        void *data = get_resolve_data(p);
        InstrNode * instr = get_resolve_instr(p);        
        int code = get_resolve_instr_name(p);
        InstrFields * fields = Instr_GetFields(instr);

        // By doing dead code elimination, need resolving code can be
        // deleted...
        if (!Instr_IsUnresolved(instr)){
            p = p->next;
            continue;
        }

        if (code == SPARC_SAVE) {   
            /* save instruction */
//             fields->format_5.simm13 = - stack_size;
        } else if ((code & MASK_BRANCH)==GROUP_IBRANCH||
                   (code & MASK_BRANCH)==GROUP_FBRANCH) {
            int path_num = ((instr->instrCode & ~ANNUL_BIT) == BA) ? 0 : 1;
            InstrNode* target = Instr_GetSucc(instr, path_num);

            if (Instr_IsDelayed(target)) {
                target = Instr_GetSucc(target, 0);
            }
            DBG(assert(!Instr_IsDelayed(target)););
            fields->format_3.disp22 = (
                Instr_GetNativeOffset(target)
                - Instr_GetNativeOffset(instr));

        } else {
	    switch(code) {
              case CALL:
                fields->format_1.disp30 =
                    ((unsigned)data -
                     (unsigned) ((int*)text_seg+Instr_GetNativeOffset(instr))) >> 2;
                break;
              case SETHI:
                // %hi(data_seg + data_offset)
                fields->format_2.imm22 = ((unsigned)data + (unsigned)data_seg) >> 10;
                break;
              case LDSH: case LDUB: case LDUH: case LD: case LDD:
              case LDF: case LDDF:
              case STB: case STH: case ST: case STD: case STF: case STDF:
              case OR:
                // %lo(data_seg + data_offst)
                assert(Instr_GetFormat(instr) == FORMAT_5
                        || Instr_GetFormat(instr) == FORMAT_8);
                fields->format_5.simm13 = ((unsigned)data + (unsigned)data_seg) &
                    0x03FF;
                break;

              case GOSUB:
                // This instruction is needless because subroutines are inlined.
                break;

              default:
                assert(0 && "CodeGen_resolve_unknown_values::unknown code type");
                break;
            }
        }

        p = p->next;
    }    
}



static
inline
uint
CodeGen_get_machine_reg_num(int r)
{
    assert(Reg_IsHW(r));

    if (Reg_GetType(r) == T_FLOAT) {
        return r - f0;
    }

    return r;
}

inline
static
uint
CodeGen_assemble_instruction(InstrNode * instr)
{
    uint ret = 0;
    int code = Instr_GetCode(instr);
    InstrFields * f = Instr_GetFields(instr);

    PDBG(num_of_risc_instr++);

    switch (Instr_GetFormat(instr)) {
      case FORMAT_1:
        ret = (0xc0000000U & code) | (0x3fffffffU & f->format_1.disp30);
        break;
      case FORMAT_2:
        assert(Reg_IsHW(f->format_2.destReg));

        ret = (0xfffc0000 & code) | (f->format_2.destReg << 25)
            | (0x0003fffff & f->format_2.imm22);
        break;
      case FORMAT_3:
        //
        // modified by doner - 98/10/16
        //
        // Now, all branches use static prediction information.
        // If the displacement is negative(backward branch),
        // the branch is predicted "TAKEN".
        // And the displacement is in 19 bits.
        //
        if (instr->fields.format_3.disp22 < 0) {
            Instr_PredictTaken(instr);
        }
        ret = (0xfff80000 & Instr_GetCode(instr)) |
            (0x7ffff & instr->fields.format_3.disp22);
        break;

      case FORMAT_4:
        assert(Reg_IsHW(f->format_4.destReg));
        assert(Reg_IsHW(f->format_4.srcReg_1));
        assert(Reg_IsHW(f->format_4.srcReg_2));

        ret = (0xc1fc3fe0 & code) | (0x00 << 13)
            | (CodeGen_get_machine_reg_num(f->format_4.destReg) << 25)
            | (CodeGen_get_machine_reg_num(f->format_4.srcReg_1) << 14)
            | (CodeGen_get_machine_reg_num(f->format_4.srcReg_2) << 0);
        break;
      case FORMAT_5:
        assert(Reg_IsHW(f->format_5.destReg));

        if (Instr_IsCondMove(instr)) {
            assert(f->format_5.simm13 <= 0x3ff 
                    && f->format_5.simm13 >= -0x400);

            ret = (code) | (0x1 << 13)
                | (f->format_5.simm13 & 0x7ff)
                | (CodeGen_get_machine_reg_num(f->format_5.destReg) << 25)
                | (f->format_5.srcReg << 11);
        } else {
            assert(Reg_IsHW(f->format_5.srcReg));
            assert(f->format_5.simm13 <= 0xfff 
                    && f->format_5.simm13 >= -0x1000);

            ret = (code) | (0x01 << 13) 
                | (f->format_5.simm13 & 0x0001fff)
                | (CodeGen_get_machine_reg_num(f->format_5.destReg) << 25)
                | (CodeGen_get_machine_reg_num(f->format_5.srcReg) << 14);
        }
        break;
      case FORMAT_6:
        assert(Reg_IsHW(f->format_6.destReg));
        assert(Reg_IsHW(f->format_6.srcReg_2));

        if (Instr_IsCondMove(instr)) {
            ret = (code)
                | (CodeGen_get_machine_reg_num(f->format_6.destReg) << 25)
                | (f->format_6.srcReg_1 << 11)
                | (CodeGen_get_machine_reg_num(f->format_6.srcReg_2) << 0);
        } else {
            assert(Reg_IsHW(f->format_6.srcReg_1));
            ret = (0xc1fc3fe0 & code) | (0x00 << 13)
                | (CodeGen_get_machine_reg_num(f->format_6.destReg) << 25)
                | (CodeGen_get_machine_reg_num(f->format_6.srcReg_1) << 14)
                | (CodeGen_get_machine_reg_num(f->format_6.srcReg_2) << 0);
        }
        break;        
      case FORMAT_7:
        assert(Reg_IsHW(f->format_7.srcReg_1));
        assert(Reg_IsHW(f->format_7.srcReg_2));
        assert(Reg_IsHW(f->format_7.srcReg_3));

        ret = (0xc1fc0000 & code) | (0x00 << 13)
            | (CodeGen_get_machine_reg_num(f->format_7.srcReg_3) << 25)
            | (CodeGen_get_machine_reg_num(f->format_7.srcReg_1) << 14)
            | (CodeGen_get_machine_reg_num(f->format_7.srcReg_2) << 0);
        break;
      case FORMAT_8:
        assert(Reg_IsHW(f->format_8.srcReg_1));
        assert(Reg_IsHW(f->format_8.srcReg_2));
        
        ret = (0xc1fc0000 & code) | (0x01 << 13)
            | (f->format_8.simm13 & 0x0001fff)
            | (CodeGen_get_machine_reg_num(f->format_8.srcReg_2) << 25)
            | (CodeGen_get_machine_reg_num(f->format_8.srcReg_1) << 14);
        break;
     case FORMAT_9:
        ret = code | instr->fields.format_9.intrpNum;
	break;

      default: 
        assert(0 && "CodeGen_assemble_instruction::unknown format");
    }

    return ret;
}




extern bool                   no_record_need;

static Method*                method;
static ExceptionInfoTable*    exception_info_table;

#ifdef INLINE_CACHE
static CallSiteInfoTable*     The_Call_Site_Info_Table;
#endif

static int *recording_map_pool;
static int recording_map_pool_index;
static int recording_map_pool_size;

static inline
void
allocate_recording_map_pool(CFG *cfg)
{
    /* accurate map pool size */
    recording_map_pool_size = CFG_GetSizeOfMapPool(cfg);

    recording_map_pool =
	gc_malloc_fixed(recording_map_pool_size * sizeof(int));
    recording_map_pool_index = 0;
}

static inline
void
copy_caller_variable_map_into_heap(MethodInstance *instance)
{
    int *var_map;
    int map_size = 0;

    if (MI_GetCaller(instance) != NULL
        && Method_GetExceptionTable(MI_GetMethod(instance)) != NULL) {
        map_size = MI_GetCallerLocalNO(instance)
            + MI_GetCallerStackNO(instance) + MAX_TEMP_VAR;
    } 
    
    var_map = recording_map_pool + recording_map_pool_index;
    
    memcpy(var_map, MI_GetCallerVarMap(instance), map_size * sizeof(int));

    MI_SetCallerVarMap(instance, var_map);
    recording_map_pool_index += map_size;
    assert(recording_map_pool_index <= recording_map_pool_size);
}


static inline
void
copy_variable_map_into_heap(InstrNode *instr)
{
    InlineGraph *graph;
    int *var_map;
    int map_size = 0;
    

    switch (Instr_NeedMapInfo(instr)) {
      case NO_NEED_MAP_INFO:
	map_size = 0;
	break;
      case NEED_MAP_INFO:
      case NEED_RET_ADDR_INFO:
	graph = Instr_GetInlineGraph(instr);
	map_size = IG_GetLocalOffset(graph) + IG_GetStackOffset(graph)
	    + Method_GetLocalSize(IG_GetMethod(graph)) + MAX_TEMP_VAR;
	break;
      case NEED_LOCAL0_MAP_INFO:
	map_size = 1;
	break;
      default:
	assert(false && "control should not be here!\n");
	
    }

    var_map = recording_map_pool + recording_map_pool_index;
    
    memcpy(var_map, instr->varMap, map_size * sizeof(int));

    instr->varMap = var_map;
    recording_map_pool_index += map_size;
    assert(recording_map_pool_index <= recording_map_pool_size);
}

static
void
CodeGen_fill_text_segment(InstrNode* instr, uint* text_seg)
{
    int   i;

    if (Instr_IsVisited(instr)) {
        return;
    }

    Instr_MarkVisited(instr);

#ifdef DYNAMIC_CHA
    if (Instr_GetCode(instr) == SPEC_INLINE) {
        *(uint*)(text_seg + Instr_GetNativeOffset(instr)) =
            assemble_nop();
        DCHA_add_speculative_call_site((Method*)instr->dchaInfo, 
                    (uint32*)(text_seg + Instr_GetNativeOffset(instr)),
                    instr->fields.format_3.disp22<<2, DCHA_SPEC_INLINE);
    } else
#endif
    if (Instr_GetCode(instr) != DUMMY_OP)
      *(uint*)(text_seg + Instr_GetNativeOffset(instr)) =
          CodeGen_assemble_instruction(instr);

    //
    // for exception handling
    // just saving information which is made during phase3.
    //
    if (Instr_NeedMapInfo(instr) != NO_NEED_MAP_INFO) {
        InlineGraph     *graph;
        MethodInstance  *instance;

        graph = Instr_GetInlineGraph(instr);
        instance = IG_GetMI(graph);
        
        assert(graph != NULL);

	copy_variable_map_into_heap(instr);

        EIT_insert(exception_info_table,
                    (uint32) ((int *)text_seg + Instr_GetNativeOffset(instr)),
                    instr->bytecodePC,
                    instr->varMap,
                    instance);
    }

#if 0
    if (Instr_IsMethodEnd(instr)) {
        InlineGraph *graph = Instr_GetInlineGraph(instr);
        InlineGraph *caller;
        MethodInstance *instance;
        InstrNode *e_instr;

        for (caller = graph; IG_GetTail(caller) == instr;
             caller = IG_GetCaller(caller)) {
            e_instr = IG_GetTail(caller);
            instance = IG_GetMI(caller);
            MI_SetRetNativePC(instance, 
                              (void *) ((int *) text_seg 
                                        + Instr_GetNativeOffset(e_instr)));
	    copy_caller_variable_map_into_heap(instance);
        }
    }
#endif 0

#ifdef INLINE_CACHE
    // to exclude exception handler
    if (The_Need_Retranslation_Flag &&
        The_Call_Site_Info_Table && Instr_IsCall(instr) && flag_adapt 
        && Instr_GetFuncInfo(instr) && Instr_GetFuncInfo(instr)->needCSIUpdate){
        CSIT_Insert(The_Call_Site_Info_Table,
                    instr->bytecodePC, 
                    (uint32)((int*)text_seg + Instr_GetNativeOffset(instr)));
    }
#endif /* INLINE_CACHE */

    for (i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
        CodeGen_fill_text_segment(Instr_GetSucc(instr, i), text_seg);
    }
}

/* fill method instance fields; caller variable map, return address */
inline static
void
CodeGen_fill_method_instance(InlineGraph *graph, uint *text_seg)
{
    InlineGraph *ngraph;
    MethodInstance *instance;
    InstrNode *enode;
    
    if (graph == NULL) return;

    if (IG_GetDepth(graph) != 0) {
        instance = IG_GetMI(graph);
        enode = IG_GetTail(graph);

        MI_SetRetNativePC(instance, (void *) ((int *) text_seg
                                              + Instr_GetNativeOffset(enode)));
        copy_caller_variable_map_into_heap(instance);
    }
    
    for(ngraph = IG_GetCallee(graph); ngraph != NULL;
        ngraph = IG_GetNextCallee(ngraph)) {
        CodeGen_fill_method_instance(ngraph, text_seg);
    }
}

inline
static
void
CodeGen_construct_text_segment(CFG* cfg, void* text_seg)
{
    CFG_PrepareTraversal(cfg);
    CodeGen_fill_text_segment(cfg->root, (uint*) text_seg);
    CodeGen_fill_method_instance(CFG_GetIGRoot(cfg), (uint *) text_seg);
}


///
/// Function Name : CodeGen_generate_code
/// Author : ramdrive
/// Input : final control flow graph
/// Output : text segment and data segment
/// Pre-Condition : register allocated and scheduled
/// Post-Condition 
/// Description
///
///     Make text and data segments.
///
///     Modified by Yang, Byung-Sun at 1997/11/19
///        text_seg and data_seg are made contiguous.
///        These segment are now pointed by 'Method' structure and
///        made available to JVM system.
///

byte *text_seg;
byte *text_seg_start;

PDBG(
inline
static
void
print_risc_instr_info()
{
    risc_per_bytecode = (float) num_of_risc_instr 
        / (float) num_of_bytecode_instr;
    
    total_num_of_bytecode_instr += num_of_bytecode_instr;
    total_num_of_risc_instr += num_of_risc_instr;
    total_risc_per_bytecode =
        (float) total_num_of_risc_instr / (float) total_num_of_bytecode_instr;
    
    fprintf(stderr, "======= %s of %s ========\n",
            method->name->data, Method_GetDefiningClass(method)->name->data);
    fprintf(stderr, 
             "\tnumber of bytecode instrs = %u\tnumber of risc instrs = %u\n",
             num_o    f_risc_instr, num_of_bytecode_instr);
    fprintf(stderr, 
             "\tRISC instr / bytecode instr = %f\n", risc_per_bytecode);
    fprintf(stderr, "\t\taccumulated number of bytecode instrs = %llu\n",
             total_num_of_bytecode_instr);
    fprintf(stderr, "\t\taccumulated number of risc instrs = %llu\n",
             total_num_of_risc_instr);
    fprintf(stderr, "\t\taverage RISC instr / bytecode instr = %f\n",
             total_risc_per_bytecode);
}
);

inline
static
void
CodeGen_calculate_stack_size(CFG *cfg)
{
    cfg->stackSize = 96 + 4 * (cfg->maxNumOfArgs - 6)
        + 4 * (cfg->spillRegNum) + 16;
    cfg->stackSize &= -8;       // align double word
}

// inline
// static
// void
// CodeGen_remove_dummy_root(CFG *cfg)
// {
//     InstrNode *dummy_root = CFG_GetRoot(cfg);
// 
//     cfg->root = Instr_GetSucc(dummy_root, 0);
//     Instr_disconnect_instruction(dummy_root, CFG_GetRoot(cfg));
//     CFG_DeregisterInstr(cfg, dummy_root);
// }

inline
static
void
CodeGen_allocate_bb_headers(CFG *cfg)
{
    max_num_of_bb = CFG_GetNumOfInstrs(cfg);
    num_of_bb = 0;
    bb_headers = (InstrNode **) FMA_calloc(sizeof(InstrNode *) *
                                         max_num_of_bb);
#ifdef OUTLINING
    num_of_ebh = 0;
#endif // OUTLINING
}

void
CodeGen_generate_code(TranslationInfo *info)
{
    int text_seg_size;
    int data_seg_size;
    byte *data_seg;
    InlineGraph *root = CFG_get_IG_root(cfg);
    MethodInstance *instance = IG_GetMI(root);
    method = IG_GetMethod(root);

    assert(cfg);


    CodeGen_update_cfg(cfg);

    allocate_recording_map_pool(cfg);

    if (TI_IsFromException(info) == false) {
        CodeGen_calculate_stack_size(cfg);

        CodeGen_set_stack_size(cfg);
    } else {
//          CodeGen_remove_dummy_root(cfg);
    }

#ifdef NEW_COUNT_RISC
    CodeGen_insert_profile_code(cfg);
#endif // NEW_COUNT_RISC

    CodeGen_allocate_bb_headers(cfg);

    CodeGen_position_basic_blocks(cfg);

    cfg->textSize = CodeGen_assign_offset(cfg);

    /* exception_info_table is global variable which is used during
       EIT_insert. */
    exception_info_table = instance->exceptionInfoTable = EIT_alloc();
    EIT_init((ExceptionInfoTable *) exception_info_table , 0);

#ifdef INLINE_CACHE
#ifdef CUSTOMIZATION
    // csit is defined for each specialized method instance
    if (!TI_IsFromException(info))
      The_Call_Site_Info_Table = TI_GetSM(info)->callSiteInfoTable;
    else
      The_Call_Site_Info_Table = NULL;
#else
    The_Call_Site_Info_Table = method->callSiteInfoTable;
#endif


    if (TI_IsFromException(info) == false) {
        text_seg_size = CFG_GetTextSegSize(cfg) + PROLOGUE_CODE_SIZE;
        if (The_Need_Retranslation_Flag)
          text_seg_size += COUNT_CODE_SIZE;
    } else {
        text_seg_size = CFG_GetTextSegSize(cfg);
    }
#else /* not INLINE_CACHE */
    text_seg_size = CFG_GetTextSegSize(cfg);
#endif /* not INLINE_CACHE */

    data_seg_size = CFG_GetDataSegSize(cfg);

    /* Allocate codes
       There exist 2 choices each, total 4 choices.
       1. whether using new code allocator
       2. whether separate text segment and data segment */
#ifdef USE_CODE_ALLOCATOR
#ifdef SPLIT_TEXT_DATA
    if (The_Final_Translation_Flag)
      text_seg = (byte *) CMA_allocate_code_block(text_seg_size );
    else
      text_seg = (byte *) (((int) gc_malloc_fixed(text_seg_size + BlockSize)
                              + BlockSize - 1) & (- BlockSize));

    data_seg = (byte *) gc_malloc_fixed(data_seg_size);
#else
    text_seg = (byte *) allocate_code_block(text_seg_size + data_seg_size
                                             + BlockSize * 2);
    data_seg = text_seg + ((text_seg_size + BlockSize) & - BlockSize);
#endif // SPLIT_TEXT_DATA

#else

#ifdef SPLIT_TEXT_DATA
    text_seg = (byte *) (((int) gc_malloc_fixed(text_seg_size + BlockSize)
                            + BlockSize - 1) & (- BlockSize));
    data_seg = (byte *) (((int) gc_malloc_fixed(data_seg_size + BlockSize)
                            + BlockSize - 1) & (- BlockSize));
#else
    text_seg = (byte *) (((int) gc_malloc_fixed(text_seg_size  + data_seg_size
                                             + BlockSize * 2)
                            + BlockSize - 1) & (- BlockSize));
    data_seg = text_seg + ((text_seg_size + BlockSize) & - BlockSize);
#endif // SPLIT_TEXT_DATA

#endif // USE_CODE_ALLOCATOR



    assert(((int) text_seg % 8) == 0 && ((int) data_seg % 8) == 0);

    MI_SetLaTTeTranslated(instance);

    assert(cfg->maxNumOfArgs >= 6);

PDBG(num_of_risc_instr = 0;)    

#ifdef INLINE_CACHE
    // made by lordljh
    // After translation, this method can be called from dispatch_method
    // and dispatch_method expect that it can find receiver type checking
    // code from the address in dtable minus PROLOGUE_CODE_SIZE
    if (TI_IsFromException(info) == false) {
#ifdef CUSTOMIZATION
        Hjava_lang_Class *checkType = TI_GetCheckType(info);
        SpecializedMethod* sm = TI_GetSM(info);
        TypeChecker* tc = SM_GetTypeChecker(sm);
        assert(sm);

        text_seg_start = text_seg + PROLOGUE_CODE_SIZE;
        SM_SetNativeCode(sm, text_seg_start);
        TI_SetNativeCode(info, (nativecode *) text_seg_start);

        // if check Type is null, then guess method->class is the most probable
        // receiver type for this method
        if (checkType == NULL) 
          checkType = method->class;

        if (tc == NULL) {
            tc = TC_make_TypeChecker(method, checkType, text_seg_start);
            SM_SetTypeChecker(sm, tc);
        } else {
            TC_SetBody(tc, text_seg_start);
        }

        TC_MakeHeader(tc, method, (unsigned*)text_seg);
        if (The_Need_Retranslation_Flag) {
            generate_count_code(TI_GetSM(info), (unsigned*)text_seg_start, 
                                (unsigned*)(text_seg_start+COUNT_CODE_SIZE));
            text_seg_start += COUNT_CODE_SIZE;
        }
#else /* not CUSTOMIZATION */
        Hjava_lang_Class *checkType = TI_GetCheckType(info);
        TypeChecker* tc;

        text_seg_start = text_seg + PROLOGUE_CODE_SIZE;
        TI_SetNativeCode(info, (nativecode *) text_seg_start);

        // if check Type is null, then guess method->class is the most probable
        // receiver type for this method
        if (checkType == NULL) 
          checkType = method->class;

        tc = TC_make_TypeChecker(method, checkType, text_seg_start);
        TC_MakeHeader(tc, method, (unsigned*)text_seg);
        if (The_Need_Retranslation_Flag) {
            generate_count_code(method, text_seg_start, 
                                text_seg_start+COUNT_CODE_SIZE);
            text_seg_start += COUNT_CODE_SIZE;
        }
#endif /* not CUSTOMIZATION */
    } else {
        text_seg_start = text_seg;
        TI_SetNativeCode(info, (nativecode *) text_seg_start);
    }
#else /* not INLINE_CACHE */
    text_seg_start = text_seg;
#ifdef CUSTOMIZATION
    TI_SetNativeCode(info, (nativecode *) text_seg_start);
    if (TI_IsFromException(info) == false)
      SM_SetNativeCode(TI_GetSM(info), text_seg_start);
    if (The_Need_Retranslation_Flag) {
        generate_count_code(TI_GetSM(info), text_seg_start
                            text_seg_start+COUNT_CODE_SIZE);
        text_seg_start += COUNT_CODE_SIZE;
    }
#else /* not CUSTOMIZATION */    
    TI_SetNativeCode(info, (nativecode *) text_seg_start);
    if (The_Need_Retranslation_Flag) {
        generate_count_code(method, text_seg_start,
                            text_seg_start+COUNT_CODE_SIZE);
        text_seg_start += COUNT_CODE_SIZE;
    }
#endif /* not CUSTOMIZATION */       
#endif /* not INLINE_CACHE */

    registerMethod((unsigned int) text_seg_start, 
                   (unsigned int) text_seg_start + CFG_GetTextSegSize(cfg),
                   instance);

    CodeGen_initialize_data_segment(cfg, text_seg_start, data_seg);

    CodeGen_resolve_unknown_values(cfg, text_seg_start, data_seg);

    CodeGen_construct_text_segment(cfg, text_seg_start);

PDBG(print_risc_instr_info();)    

    GC_WRITE(method, text_seg);


    /*
        FIXME : direct access to structure field
    */
    if (!TI_IsFromException(info)) {
        method->accflags |= ACC_TRANSLATED;
    }

    FLUSH_DCACHE(text_seg, 
                 (unsigned int) text_seg + text_seg_size + BlockSize);
}

#undef DBG
