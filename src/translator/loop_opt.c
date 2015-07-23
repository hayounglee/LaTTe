/* loop_opt.c

   A simple loop optimizer which does on simple loops :
      Loop Invariant Code Motion
      Register Promotion 

   Written by: Jinpyo Park <jp@altair.snu.ac.kr>

   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#include <stdio.h>

#include "config.h"
#include "CFG.h"
#include "InstrNode.h"
#include "bit_vector.h"
#include "flags.h"
#include "reg.h"
#include "method_inlining.h"
#include "register_allocator.h"
#include "AllocStat.h"

#define DBG(s)  

/*======================================================================
  Loop detection
======================================================================*/
static InstrNode* loop_entry_instr;
typedef struct {
    InstrNode*  instr;
    int         successor_num;  /* The path id in the loop */
    word *ud_chain, *du_chain;  /* d-u, u-d chain */
    int mem_instr_array_index;  /* for mem instr to point its entry in mem_instr_array */
    bool is_moved;       
    int basic_block_id;         /* basic block id of this instruction */
} LoopInstr;
/* a post-order array of instructions */
static LoopInstr* loop_instr_array;
static int loop_instr_array_index;
static int loop_instr_array_size;
static int loop_instr_array_bv_size;
/* loop exit */
static int* loop_exit_instr_idx_array;
static int loop_exit_instr_idx_array_size;
/* loop entry */
static int loop_entry_outer_pred_num;
static InstrNode* loop_entry_outer_pred_instr;
static int loop_entry_outer_pred_instr_succ_num; 
/* basic block */
static int basic_block_num;

/* loop instruction-to-bitvector map */
static int bv_map_shift_count;  /* = 1+log_2(loop_instr_array_bv_size) */
static int bv_map_size_in_byte; 
static int bv_map_entry_size;   /* = 1<<bv_map_shift_count  */

/* Special datastructure for memory instructions */
typedef struct {
    int instr_idx;      /* index in the loop_instr_array */
    int iterator_idx;   /* index in the hash table iterator */
    word* addr_def_instr_bv;    
} MemInstr;
static MemInstr* mem_instr_array;
static int mem_instr_array_size;
static int number_of_stores;
static int number_of_loads;

/* Datastructure for address expression management */
typedef struct {
    int field1, field2; /* for SPARC addressing modes */
    Field* field;       /* Field information */
    int value_type;     /* type of value in the memory (T_INT, T_FLOAT, ...) */
    int iterator_idx;   
    word* alias_mem_expr_bv;
    int cmvar;          /* promoted variable */
} MemExpr;
static MemExpr* mem_expr_hash_table;
static int mem_expr_hash_table_size;
static int mem_expr_hash_table_bv_size;
/* mem expression iterator */
static MemExpr** mem_expr_hash_table_iterator;
static int mem_expr_hash_table_iterator_size;
static int mem_expr_hash_table_iterator_bv_size;

/* Instead of introducing the new variables for CM, let 
   loop optimization share the variables from CSE. */
extern int number_of_vn_var;

/* Global variables for loop peeling */
static InstrNode* loop_prologue_header;         /* header of the peeled loop */
static InstrNode** additional_region_header;    
static int additional_region_header_num;        /* # of additional region header */
static InstrNode** peeled_loop_exits;           
static int* peeled_loop_exits_path_num;
static int peeled_loop_exits_idx;

/*======================================================================
  constants
======================================================================*/
const int MAX_LOOP_INSTRS = 512;

/*======================================================================
  function decl.
======================================================================*/
static InstrNode* code_motion(CFG* cfg, InstrNode* loop_prologue_header);

#include "loop_opt_util.ic"
#include "loop_opt_detect.ic"
#include "loop_opt_dfa.ic"

/* 
   Name        : LoopOpt_optimize_loop
   Description : A driver function for the loop optimization.
   Maintainer  : Jinpyo Park <jp@altair.snu.ac.kr>
   Pre-condition: 
   
   Post-condition: 
   This procedure returns (1) the first instruction in the sequence
   of moved instructions (first_moved_instr) and (2) the (possibly)
   changed loop instruction (new_loop_entry_instr).

   Notes:
   A successful loop optimization may create additional regions
   resulting from loop peeling and moved instructions at loop entry
   and exits. It may screw up the region informations and is 
   so annoying problem to the register allocator which iterates
   register allocation region by region. Take a look at the trick
   used in register_allocator.c (if you are interested).
 */
int
LoopOpt_optimize_loop(CFG* cfg, InstrNode* instr, 
              InstrNode** first_moved_instr, 
              InstrNode** new_loop_entry_instr,
              InstrNode*** additional_region_headers)
{
    InstrNode* old_region_header;
    void* h_of_old_region_header;
    int i;
    bool    is_region_dummy_necessary;

    /* loop detection */
    if (!detect_loop(cfg, instr)) return -1;

    DBG({
        extern int num;
        InlineGraph*    IG_root = CFG_get_IG_root(cfg);
        Method*         method = IG_GetMethod(IG_root);
        printf("Loop is found in %s:%s, (size=#%d) (num=%d)\n", 
               Class_GetName(Method_GetDefiningClass(method))->data,
               Method_GetName(method)->data,
               loop_instr_array_size, num);
    });

    /* dataflow analysis (for symbolic registers and memory locations) */
    dataflow_analysis(cfg);
    DBG(print_loop_instr_array(););

    /* Prepare code motion */
    old_region_header = instr;

    if (Instr_IsRegionDummy(old_region_header)) {
        is_region_dummy_necessary = true;
        loop_prologue_header = Instr_create_region_dummy();
    } else {
        is_region_dummy_necessary = false;
        loop_prologue_header = Instr_create_nop();
    }
    CFG_RegisterInstr(cfg, loop_prologue_header);
    /* A meaningless setting of successor to make successive
       connection happy */
    Instr_AddSucc(loop_prologue_header, loop_prologue_header);
    
    /* Insert dummy instruction at each exit. The last-use marker will be
       inserted after the dummy instruction, while the memory-intern
       instruction of register promotion will be inserted before it. */
    for (i=0; i<loop_exit_instr_idx_array_size; i++) {
        int exit_instr_idx, exit_path_num;
        InstrNode* dummy_instr;
        InstrNode* exit_instr;
            
        dummy_instr = Instr_create_nop();
        CFG_RegisterInstr(cfg, dummy_instr);
        SET_LOOP_ID(dummy_instr);
        
        exit_instr_idx = loop_exit_instr_idx_array[i];
        exit_path_num = (loop_instr_array[exit_instr_idx].successor_num == 0) ? 1 : 0;
        /* Copy live information of exit instruction to the dummy instruction, 
           which will be a new branch target. */
        exit_instr = Instr_GetSucc(loop_instr_array[exit_instr_idx].instr, exit_path_num);
        //assert(Instr_GetLive(exit_instr));
        Instr_SetLive(dummy_instr, Instr_GetLive(exit_instr));        
        /* FIXME: Method inlining understands -9. This magic number is meaningful, 
           which I hope to be not. */
        dummy_instr->bytecodePC = -9;   
        /* Insert dummy instruction. */
        CFG_InsertInstrAsSucc(cfg, loop_instr_array[exit_instr_idx].instr,
                              exit_path_num, dummy_instr);
    }

    if (flag_looppeeling) {
        /* Initialize a temporary list of instructions to resolve. */
        init_peeled_resolve_instr_list();   
        /* Prepare an array of additional region headers and of the exits
           in the peeled loop to connect the exit edges after code motion. */
        additional_region_header_num = 0;
        peeled_loop_exits_idx = 0;
        /* loop_exit_isntr_idx_size can be zero if the loop is an infinite loop. */
        if (loop_exit_instr_idx_array_size > 0) {
            additional_region_header = 
                (InstrNode**)FMA_calloc(sizeof(InstrNode*) * loop_exit_instr_idx_array_size);
            peeled_loop_exits = 
                (InstrNode**)FMA_calloc(sizeof(InstrNode*) * loop_exit_instr_idx_array_size);
            peeled_loop_exits_path_num = 
                (int*)FMA_calloc(sizeof(int) * loop_exit_instr_idx_array_size);
        } else {
            additional_region_header = NULL;
            peeled_loop_exits = NULL;
            peeled_loop_exits_path_num = NULL;
        }
    } else {
        /* peeled_loop_exits_idx is used in connect_peeled_loop() anyway. */
        peeled_loop_exits_idx = 0;
    }
    
    if ((*new_loop_entry_instr = code_motion(cfg, loop_prologue_header)) != NULL) {
        *first_moved_instr = loop_prologue_header;
        /* Move the original h_map of loop entry to the new region header. */
        h_of_old_region_header = Instr_GetH(old_region_header);
        Instr_SetH(old_region_header, NULL);
        Instr_SetH(loop_prologue_header, h_of_old_region_header);
        /* Insert region_dummy instruction for the new region headers. */
        if (is_region_dummy_necessary) {
            for (i=0; i<additional_region_header_num; i++) {
                additional_region_header[i] = 
                    RegAlloc_insert_region_dummy(cfg, additional_region_header[i]);
            }
        }
        *additional_region_headers = additional_region_header;
        /* Finally intern the temporary list of instructions to resolve. */
        intern_peeled_resolve_instr_list(cfg);
        return additional_region_header_num;
    } else {
        return -1;
    }
}

/*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

  code motion

\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*====================================================================*/
inline static 
bool 
is_eligible_for_code_motion(InstrNode* instr)
/*====================================================================*/
{
    return (!((Instr_IsBranch(instr) || Instr_IsSetcc(instr))   /* branch and its definition */
              || (instr->format == FORMAT_2 &&                  /* marker instruction */
                  instr->fields.format_2.imm22 == 0 &&  
                  !Instr_IsUnresolved(instr)) 
              || (Reg_IsRegister(Instr_GetDestReg(instr, 0)))        /* CSE-ed instruction */
              || (Reg_IsRegister(Instr_GetSrcReg(instr, 0))          /* long manipulation */
                  && Instr_GetSrcReg(instr, 0) != g0)
              || Instr_IsRegionDummy(instr)                          /* Region dummy */
              || Instr_IsVolatile(instr)
#ifdef USE_TRAP_FOR_ARRAY_BOUND_CHECK 
              || (Instr_GetCode(instr) == TLEU )                /* TRAP instruction */
#endif
        ));
}

/*====================================================================*/
inline static 
bool
is_hoistable(int instr_idx)
/*======================================================================
  Return true if the instruction pointed by the index is loop invariant.
  An instruction is loop invariant if there is no def of it in the loop.

  In the original implementation of hoisting, a hoisted definition is 
  removed from the ud_chain of its use instructions. Then it was simple
  to check if a subsequent code is loop invariant since such a code
  will have no definition in its ud_chain. However keeping correct last
  use information is not handy in this old implementation, hence I 
  changed the implementation such that a hoisted definition is not
  removed from ud_chain of its uses. 
======================================================================*/
{
    register int j;
    word* ud_chain;
    InstrNode* instr;

    instr = loop_instr_array[instr_idx].instr;
    /* Don't move constant copy instruction - it is too cheap
       and increases the register pressure. */
    if (instr->format == FORMAT_5 && Instr_GetSrcReg(instr, 0) == g0) 
      return false;
    
    ud_chain = loop_instr_array[instr_idx].ud_chain;
    FORWARD_SCAN_BV(j, ud_chain, loop_instr_array_size, -1) {
        if (!loop_instr_array[j].is_moved) return false;
    }
    return true;
}

/*====================================================================*/
inline static 
bool
is_sinkable(int instr_idx)
/*======================================================================
  Return true if the instruction pointed by the index is loop invariant.
  An instruction is loop invariant and can be moved downward 
  if there is no use of it in the loop.

  In case instr is a store, we cannot sink it unless its address 
  expression is loop-invariant. 
======================================================================*/
{
    register int j, k;
    InstrNode* instr = loop_instr_array[instr_idx].instr;


    /* We restrict the sinkable instructions to stores. I don't 
     * think other instruction would be without use in the loop. 
     * For example, in _228_jack, only stores are sinkable. 
     */
    if (Instr_IsStore(instr)) {
        word* du_chain = loop_instr_array[instr_idx].du_chain;
        for (j=loop_instr_array_bv_size-1; j>=0; j--) {
            if (du_chain[j] != 0) return false;
        }
        /* check if the memory expression is loop-invariant */
        for (j=0; j<Instr_GetNumOfSrcRegs(instr)-1; j++) 
          /* Are you sure the value to be stored is in the last
           * source register? If not, this FOR loop is dangerous.
           */
        {   
            int src_var = Instr_GetSrcReg(instr, j);
            word* ud_chain = loop_instr_array[instr_idx].ud_chain;
            FORWARD_SCAN_BV(k, ud_chain, loop_instr_array_size, -1) {
                if (src_var == 
                    Instr_GetDestReg(loop_instr_array[k].instr, 0))
                {
                    /* memory expression is loop variant */
                    return false;
                }
            }
        }
        return true;
    } 
    return false;
}

/*====================================================================*/
inline static 
bool
is_cmvar_overflow(void)
/*====================================================================*/
{
    return (number_of_vn_var >= MAX_VN_VAR);
}

/*====================================================================*/
inline static 
int
cmvar_new(int type)
/*====================================================================*/
{
    /* Share the variables from CSE. */
    return ((is_cmvar_overflow()) ?
            -1 : Var_make_vn(type, number_of_vn_var++));
}

/*====================================================================*/
void
insert_last_use_marker_at_exits(CFG* cfg, int reg)
/*======================================================================
======================================================================*/
{
    int i;
    for (i=0; i<loop_exit_instr_idx_array_size; i++) {
        int loop_exit_idx, exit_path;
        InstrNode* marker_instr = 
            create_format6_instruction(ADD, g0, reg, g0, -1);
        Instr_SetLastUseOfSrc(marker_instr, 0);
        
        loop_exit_idx = loop_exit_instr_idx_array[i];
        exit_path = (int)(!loop_instr_array[loop_exit_idx].successor_num);
        assert(loop_instr_array[loop_exit_idx].successor_num < 2);
        assert(exit_path == 0 || exit_path == 1);
        
        CFG_InsertInstrAsSucc(
            cfg, 
            find_next_dummy_instruction(loop_instr_array[loop_exit_idx].instr, exit_path),
            0, marker_instr);
        SET_LOOP_ID(marker_instr);
    }
}

/*====================================================================*/
inline static InstrNode* 
hoist_instr(CFG* cfg, int instr_idx)
/*======================================================================
  Insert the instruction before the loop entry. Leave a copy 
  at the original location.
  
  Avoid insertion of a copy as much as possible. It will not be 
  easily coalesced. 

  Code motion is done by loop peeling naturally. Determine the 
  new destination variable and return the copy instruction
  whose source is the new destination.
======================================================================*/
{
    int new_dst_var, old_dst_var;
    InstrNode* instr;

    instr = loop_instr_array[instr_idx].instr;
    assert(Var_IsVariable(Instr_GetDestReg(instr, 0)));
    
    /* Introduce a temporary register for moved_instr. */
    old_dst_var = Instr_GetDestReg(instr, 0);
    new_dst_var = cmvar_new(Var_GetType(old_dst_var));
    assert(new_dst_var != -1);
    /* Change the instr into a copy. */
    change_instr_to_copy(instr, new_dst_var, old_dst_var);
    /* Insert last-use marker instruction at exits. */
    insert_last_use_marker_at_exits(cfg, new_dst_var);
    /* Mark as moved. */
    loop_instr_array[instr_idx].is_moved = true;

    return instr;
}

/*====================================================================*/
inline static InstrNode* 
sink_instr(CFG* cfg, InstrNode* instr, int instr_idx)
/*======================================================================
  Move the instruction after every exit. Leave one or more copies at the 
  original location according to the number of source operands.
======================================================================*/
{
    int i;
    int new_src_var, old_src_var, var_type;
    int loop_exit_idx, exit_path;
    int num_of_source_regs, num_of_source_reg_map_entry;
    InstrNode* moved_instr;  

    /* Current candidate of sinkable instruction is store. 
    * See "is_sinkable()". */
    assert(Instr_IsStore(instr));
    
    num_of_source_regs = Instr_GetNumOfSrcRegs(instr);
    assert(num_of_source_regs < 4 && num_of_source_regs >= 0);

    /* create a clone */
    moved_instr = clone_instr(cfg, instr);
    num_of_source_reg_map_entry = 0;

    /* is_sinkable() assures that memory expression is defined outside
     * of the loop. So, it is safe to keep the old name of variables
     * of the memory expression whose index is from 0 to 
     * num_of_source_regs-2. One thing you have to do for those
     * variables is to set the last-use information. */

    /* For the value to be stored, we have to create a
     * new variable for renaming since it is possible that
     * the old variable is defined after instr in the loop. */
    old_src_var = Instr_GetSrcReg(moved_instr, num_of_source_regs-1);
    var_type = Var_GetType(old_src_var);
    assert(var_type != T_DOUBLE);
    new_src_var = cmvar_new(var_type);
    assert(new_src_var != -1);
    change_instr_to_copy(instr, old_src_var, new_src_var);
    Instr_SetSrcReg(moved_instr, 
                        num_of_source_regs-1, new_src_var);
    Instr_SetLastUseOfSrc(moved_instr, num_of_source_regs-1);

    /* insert the "moved_instr" after every exit. */
    loop_exit_idx = loop_exit_instr_idx_array[0];
    exit_path = (int)(!loop_instr_array[loop_exit_idx].successor_num);
    assert(loop_instr_array[loop_exit_idx].successor_num < 2);
    assert(exit_path == 0 || exit_path == 1);
    
    CFG_InsertInstrAsSucc(cfg, loop_instr_array[loop_exit_idx].instr, 
                                    exit_path, moved_instr);
    for (i=1; i<loop_exit_instr_idx_array_size; i++) {
        /* insert clones of moved_instr */
        loop_exit_idx = loop_exit_instr_idx_array[i];
        exit_path = (int)(!loop_instr_array[loop_exit_idx].successor_num);
        assert(loop_instr_array[loop_exit_idx].successor_num < 2);
        assert(exit_path == 0 || exit_path == 1);
        
        CFG_InsertInstrAsSucc(cfg, loop_instr_array[loop_exit_idx].instr, 
                                        exit_path, clone_instr(cfg, moved_instr));
    }

    if (instr_idx > -1) {
        /* In case instr does not dominate every exit, you have to
         * insert definitions before loop entry.
         * Under current implementation dominance can be found
         * just by comparing the index of instr with the
         * index of the exit instructions.
         */
        for (i=0; i<loop_exit_instr_idx_array_size; i++) {
            if (instr_idx < loop_exit_instr_idx_array[i]) 
              /* Check dominance by index comparison. */
            {
                InstrNode* pre_def;
                /* instr is now corrupt. Use moved_instr instead. */
                assert(Instr_IsStore(moved_instr));
                if (moved_instr->format == FORMAT_7) {
                    /* store with no immediate */
                    pre_def = 
                        create_format4_instruction(
                            get_dual_load_instr_code(moved_instr->instrCode),
                            new_src_var, 
                            Instr_GetSrcReg(moved_instr, 0),
                            Instr_GetSrcReg(moved_instr, 1), -1);
                } else {
                    /* store with immediate */
                    assert(moved_instr->format == FORMAT_8);
                    pre_def = 
                        create_format5_instruction(
                            get_dual_load_instr_code(moved_instr->instrCode),
                            new_src_var,
                            Instr_GetSrcReg(moved_instr, 0),
                            moved_instr->fields.format_8.simm13, -1);
                }
                /* insert definitions before loop entry */
                CFG_InsertInstrAsSucc(
                    cfg, loop_entry_outer_pred_instr, 
                    loop_entry_outer_pred_instr_succ_num, 
                    pre_def);
                loop_entry_outer_pred_instr = pre_def;
                loop_entry_outer_pred_instr_succ_num = 0;
                return pre_def;
            }
        }
    }

    return NULL;
}

/*====================================================================*/
static InstrNode*
insert_memory_intern_instr(CFG* cfg, 
                           int load_instr_code, int store_instr_code,
                           MemExpr* mem_expr, int cmvar, bool instr_needs_map)
/*======================================================================
  Insert a load at the entry of the loop to read the promoted value.

  For loop peeling, don't intern the load instruction right here.
  Instead return the instruction to be processed by the caller.
======================================================================*/
{
    InstrNode* load_instr;
    InstrNode* store_instr;
    int loop_exit_idx, exit_path, i;

    if (IS_SIMM13(mem_expr->field2)) {
        load_instr = 
            create_format5_instruction(load_instr_code,
                                       cmvar,
                                       mem_expr->field1,
                                       mem_expr->field2,
                                       -2);
        store_instr = 
            create_format8_instruction(store_instr_code,
                                       cmvar,
                                       mem_expr->field1,
                                       mem_expr->field2,
                                       -2);
        Instr_SetLastUseOfSrc(store_instr, 1);
    } else {
        load_instr =
            create_format4_instruction(load_instr_code,
                                       cmvar,
                                       mem_expr->field1,
                                       mem_expr->field2,
                                       -2);
        store_instr = 
            create_format7_instruction(store_instr_code,
                                       cmvar,
                                       mem_expr->field1,
                                       mem_expr->field2,    
                                       -2);
        Instr_SetLastUseOfSrc(store_instr, 2);
    }
    if (instr_needs_map) Instr_SetNeedMapInfo(load_instr);
#ifndef NDEBUG
    load_instr->loop_instr_id = store_instr->loop_instr_id = 
        Instr_GetSucc(loop_entry_outer_pred_instr,
                      loop_entry_outer_pred_instr_succ_num)->loop_instr_id;
#endif
                  
    /* Insert load instruction */
    assert(Instr_GetNumOfSuccs(loop_entry_outer_pred_instr) <= 2);
    CFG_InsertInstrAsSucc(cfg, loop_entry_outer_pred_instr, 
                                    loop_entry_outer_pred_instr_succ_num, 
                                    load_instr);
    /* update insertion point */
    loop_entry_outer_pred_instr_succ_num = 0;
    loop_entry_outer_pred_instr = load_instr;

    /* Insert store instruction along the exit edge. 
       Hence it will be placed before the dummy instruction at the exit. */
    loop_exit_idx = loop_exit_instr_idx_array[0];
    exit_path = (int)(!loop_instr_array[loop_exit_idx].successor_num);
    assert(loop_instr_array[loop_exit_idx].successor_num < 2);
    assert(exit_path == 0 || exit_path == 1);
    
    CFG_InsertInstrAsSucc(cfg, loop_instr_array[loop_exit_idx].instr, 
                                    exit_path, store_instr);
    for (i=1; i<loop_exit_instr_idx_array_size; i++) {
        /* insert clones of store_instr */
        loop_exit_idx = loop_exit_instr_idx_array[i];
        exit_path = (int)(!loop_instr_array[loop_exit_idx].successor_num);
        assert(loop_instr_array[loop_exit_idx].successor_num < 2);
        assert(exit_path == 0 || exit_path == 1);
        
        CFG_InsertInstrAsSucc(cfg, loop_instr_array[loop_exit_idx].instr, 
                                        exit_path, clone_instr(cfg, store_instr));
    }
    return load_instr;
}

/*====================================================================*/
inline static InstrNode*
register_promotion(CFG* cfg, int instr_idx)
/*====================================================================*/
{
    int j, cmvar;
    InstrNode *instr, *use_instr, *def_instr, *preload_instr;
    int mem_expr_idx;
    MemExpr* mem_expr;
    word *du_chain, *ud_chain;
    
    instr = loop_instr_array[instr_idx].instr;
    if (!(Instr_IsLoad(instr) || Instr_IsStore(instr))) return NULL;
    
    mem_expr_idx = get_mem_expr_idx(instr_idx);
    mem_expr = get_mem_expr(instr_idx);
    if (mem_expr->value_type == T_INVALID) return NULL;

    /* Give up promotion if the address aliases any address. */
    if (is_aliasing_mem_expr(mem_expr)) return NULL;

    cmvar = mem_expr->cmvar;
    ud_chain = loop_instr_array[instr_idx].ud_chain;
    du_chain = loop_instr_array[instr_idx].du_chain;

    if (Instr_IsLoad(instr)) {
        /* We can promote scalar memory variable to register if
         * load has the only definition which is a store. */
        if (cmvar != 0) 
          /* This memory location has already been promoted. */
        {
            DBG({
                /* Conditions for promotion :
                * (1) (Every) def is a store.
                * (2) (Every) store definition has the same mem expr (including addr_def_instr_bv). */
                FORWARD_SCAN_BV(j, ud_chain, loop_instr_array_size, -1) { 
                    def_instr = loop_instr_array[j].instr;
                    assert((Instr_IsStore(def_instr) || Instr_IsICopy(def_instr) || Instr_IsFCopy(def_instr))
                           && compare_mem_instr_addr(instr_idx, j));
                }
                /* Check if every previous definitions are promoted appropriately. */
                FORWARD_SCAN_BV(j, ud_chain, loop_instr_array_size, instr_idx) {
                    def_instr = loop_instr_array[j].instr;
                    assert(Instr_IsICopy(def_instr) || Instr_IsFCopy(def_instr));
                    assert(get_copy_destination_register(def_instr) == cmvar);
                }
                /* Check the following so that it can be safe to 
                * insert memory instructions based on the info
                * stored in mem_expr. */
                assert(mem_expr->field1 == Instr_GetSrcReg(instr, 0) &&
                       (mem_expr->field2 == Instr_GetSrcReg(instr, 1) ||
                        instr->format == FORMAT_5));
            }); // DBG
            
            change_instr_to_copy(instr, cmvar, 
                                 get_load_destination_register(instr));
            return NULL;
        } 
        else 
          /* This memory location has not been promoted. */
        {
            int old_dst_var;
            /* Conditions for promotion :
            * (1) (Every) def is a store.
            * (2) (Every) store definition has the same mem expr (including addr_def_instr_bv). */
            FORWARD_SCAN_BV(j, ud_chain, loop_instr_array_size, -1) {
                def_instr = loop_instr_array[j].instr;
                if (!Instr_IsStore(def_instr) || !compare_mem_instr_addr(instr_idx, j))
                    return NULL;
            }

            old_dst_var = get_load_destination_register(instr);
            cmvar = cmvar_new(Var_GetType(old_dst_var));
            assert(cmvar != -1);
            mem_expr->cmvar = cmvar;
            /* Update mem_expr fields with the operands of current instruction. 
               Note that a register expression can be loop invariant after 
               previous code motion as in spec.benchmarks._201_compress.Compressor:output.
               Therefore we have to make updates here. 99/03/15 */
            mem_expr->field1 = Instr_GetSrcReg(instr, 0);
            if (instr->format != FORMAT_5) mem_expr->field2 = Instr_GetSrcReg(instr, 1);
            /* Create memory instruction at the entry and the exits. */
            preload_instr = 
                insert_memory_intern_instr(cfg, 
                                           instr->instrCode, 
                                           get_dual_store_instr_code(instr->instrCode),
                                           mem_expr, cmvar, Instr_NeedMapInfo(instr));
            change_instr_to_copy(instr, cmvar, old_dst_var);
            return preload_instr;
        }
    } else {
        /* We can promote scalar memory variable to register if 
        * memory expression of this store is promoted to a register */
        bool last_use;
        assert(Instr_IsStore(instr));

        if (cmvar != 0) 
          /* This memory location has already been promoted. */
        {
            DBG({
                /* Conditions for promotion :
                * (1) This store has no defs for mem_exprs. 
                * (2) (Every) load use has the same mem expr. */
                FORWARD_SCAN_BV(j, ud_chain, loop_instr_array_size, -1) {
                    def_instr = loop_instr_array[j].instr;
                    assert(Instr_GetDestReg(def_instr, 0) == 
                           get_store_source_register(instr));
                }
                FORWARD_SCAN_BV(j, du_chain, loop_instr_array_size, -1) {
                    use_instr = loop_instr_array[j].instr;
                    assert((Instr_IsLoad(use_instr) || Instr_IsICopy(use_instr) || Instr_IsFCopy(use_instr))
                           && compare_mem_instr_addr(instr_idx, j));
                }
                /* Check if every previous uses are promoted appropriately. */
                FORWARD_SCAN_BV(j, du_chain, loop_instr_array_size, instr_idx) {
                    use_instr = loop_instr_array[j].instr;
                    assert(Instr_IsICopy(use_instr) || Instr_IsFCopy(use_instr));
                    assert(get_copy_source_register(use_instr) == cmvar);
                }
                /* Check the following so that it can be safe to 
                * insert memory instructions based on the info 
                * stored in mem_expr. */
                assert(mem_expr->field1 == Instr_GetSrcReg(instr, 0) &&
                       (mem_expr->field2 == Instr_GetSrcReg(instr, 1) ||
                        instr->format == FORMAT_8));
            });
            last_use = is_store_source_last_use(instr);
            change_instr_to_copy(instr, get_store_source_register(instr),
                                 cmvar);
            if (last_use) 
              Instr_SetLastUseOfSrc(instr, (Instr_IsICopy(instr)) ? 0:1);
            
            return NULL;
        }
        else
          /* This memory location has not been promoted. */
        {
            int old_src_var;
            /* Conditions for promotion :
            * (1) This store has no defs. 
            * (2) (Every) load use has the same mem expr. */
            FORWARD_SCAN_BV(j, ud_chain, loop_instr_array_size, -1) {
                def_instr = loop_instr_array[j].instr;
                if (Instr_GetDestReg(def_instr, 0) != 
                    get_store_source_register(instr))
                  return NULL;
            }
            FORWARD_SCAN_BV(j, du_chain, loop_instr_array_size, -1) {
                use_instr = loop_instr_array[j].instr;
                if (!Instr_IsLoad(use_instr) || !compare_mem_instr_addr(instr_idx, j))
                  return NULL;
            }
            
            old_src_var = get_store_source_register(instr);
            cmvar = cmvar_new(Var_GetType(old_src_var));
            assert(cmvar != -1);
            mem_expr->cmvar = cmvar;
            /* Update mem_expr fields with the operands of current instruction. 
               Note that a register expression can be loop invariant after 
               previous code motion as in spec.benchmarks._201_compress.Compressor:output.
               Therefore we have to make updates here. 99/03/15 */
            mem_expr->field1 = Instr_GetSrcReg(instr, 0);
            if (instr->format != FORMAT_8) mem_expr->field2 = Instr_GetSrcReg(instr, 1);
            /* Create memory instruction at the entry and the exits. */
            preload_instr = 
                insert_memory_intern_instr(cfg, 
                                           get_dual_load_instr_code(instr->instrCode), 
                                           instr->instrCode,
                                           mem_expr, cmvar, Instr_NeedMapInfo(instr));
            last_use = is_store_source_last_use(instr);
            change_instr_to_copy(instr, old_src_var, cmvar);
            if (last_use) 
              Instr_SetLastUseOfSrc(instr, (Instr_IsICopy(instr)) ? 0:1);
            return preload_instr;
        }
    } 
    return NULL;
}

inline static InstrNode*
loop_peeling(CFG* cfg, 
             InstrNode* last_peeled_instr, 
             InstrNode* c_instr, 
             InstrNode* new_instr,
             int pred_path_num, 
             int succ_path_num)
{
    int exit_path;
    InstrNode* exit_instr;

    CFG_RegisterInstr(cfg, new_instr);

    new_instr->predecessors = NULL;
    new_instr->numOfPredecessors = 0;
    new_instr->maxNumOfPredecessors = 0;

    switch (new_instr->numOfSuccessors) {
      case 0:
        /* This is too tricky. Preload instructions don't have
           any successors because they are likely to be created
           by clone_instr() or just create_instructions().
           Hence, we have to set them to have a successor
           here. */
        new_instr->numOfSuccessors = 
            new_instr->maxNumOfSuccessors = 1;
        new_instr->successors = (InstrNode**)FMA_calloc(sizeof(InstrNode*));
        break;
      case 1:
        new_instr->successors = (InstrNode**)FMA_calloc(sizeof(InstrNode*));
        break;
      case 2:
        new_instr->successors = (InstrNode**)FMA_calloc(sizeof(InstrNode*)*2);
        exit_path = (succ_path_num == 0) ? 1 : 0;
        if (Instr_GetCode(new_instr) == BCC) {
            /* exit path is just a trap. It is cheaper to dupliacate the
               trap instruction. */
            exit_instr = create_format9_instruction(TA, 0x11, -1);
            CFG_RegisterInstr(cfg, exit_instr);
            CFG_MarkExceptionGeneratableInstr(cfg, exit_instr);
            /* Connect right away. */
            Instr_SetSucc(new_instr, exit_path, exit_instr);
            Instr_AddPred(exit_instr, new_instr);
        } else {    
            peeled_loop_exits[peeled_loop_exits_idx] = new_instr;
            peeled_loop_exits_path_num[peeled_loop_exits_idx] = exit_path;
            peeled_loop_exits_idx++;
            exit_instr = find_next_dummy_instruction(c_instr, exit_path);
            assert(!Instr_IsRegionEnd(exit_instr));
            additional_region_header[additional_region_header_num++] = exit_instr;
            /* Partly connect. */
            Instr_SetSucc(new_instr, exit_path, exit_instr);
        }
//         /* Do what is done at "process_for_branch". */
//         add_new_resolve_instr(cfg, new_instr, NULL);
        break;
      default:
        assert(false);
    }
    Instr_SetSucc(last_peeled_instr, pred_path_num, new_instr);
    Instr_AddPred( new_instr, last_peeled_instr );

    return new_instr;
}

inline static InstrNode*
loop_peeling_after_cloning(CFG* cfg, InstrNode* last_peeled_instr, 
                           InstrNode* c_instr,
                           int pred_path_num, int succ_path_num)
{
    InstrNode* new_instr;

    new_instr = (InstrNode*)FMA_calloc(sizeof(InstrNode));
    duplicate_instr(new_instr, c_instr);
    if (Instr_IsUnresolved(c_instr)) {
        add_peeled_resolve_instr(new_instr, 
                              get_resolve_data(Instr_GetResolveInfo(c_instr)));
    }
    /* Let the original instruction non-exception-generatable.
       If it raised an exception, the exception would be raised
       at the new instruction already. I think we can save time
       to record local map for the original instruction. */
#ifdef USE_TRAP_FOR_ARRAY_BOUND_CHECK 
    if ( Instr_IsExceptionGeneratable(c_instr)
         && Instr_GetCode(c_instr) != TLEU ) {
        Instr_UnsetExceptionGeneratable(c_instr);
    }
    
#else
    if (Instr_IsExceptionGeneratable(c_instr)) {
        Instr_UnsetExceptionGeneratable(c_instr);
    }
#endif
    return loop_peeling(cfg, last_peeled_instr, 
                        c_instr, new_instr, 
                        pred_path_num, succ_path_num);
}

inline static void
connect_peeled_loop(InstrNode* peeled_loop_header,
                    InstrNode* last_peeled_instr,
                    int path_num,
                    InstrNode* loop_entry_outer_pred_instr,
                    int loop_entry_outer_pred_instr_succ_num)
{
    InstrNode* old_loop_header;
    int index, i;

    /* We have to retrieve the old loop header here instead of
       using the old value because the loop header may be changed
       after code motion. Also note that old_loop_header may not
       be the region header due to preload instructions. */
    old_loop_header = Instr_SetSucc(loop_entry_outer_pred_instr, 
                                    loop_entry_outer_pred_instr_succ_num,
                                    peeled_loop_header);
    Instr_AddPred(peeled_loop_header, loop_entry_outer_pred_instr);
    index = Instr_GetPredIndex(old_loop_header, loop_entry_outer_pred_instr);
    assert(index >= 0);
    Instr_SetPred(old_loop_header, index, last_peeled_instr);
    Instr_SetSucc(last_peeled_instr, path_num, old_loop_header);

    /* Final connection of the loop exits. */
    for (i=0; i<peeled_loop_exits_idx; i++) {
        Instr_AddPred(Instr_GetSucc(peeled_loop_exits[i], 
                                    peeled_loop_exits_path_num[i]), 
                      peeled_loop_exits[i]);
    }
}

/*====================================================================*/
static InstrNode*
code_motion(CFG* cfg, InstrNode* loop_prologue_header)
/*======================================================================
  Actual code motion and register promotion is driven here. 

  returns the new loop header instruction.
======================================================================*/
{
    int i, j, k;
    word* du_chain;
    word* ud_chain;
    int src_var, dst_var;
    InstrNode* old_loop_entry_outer_pred_instr;
    int old_loop_entry_outer_pred_instr_succ_num;
    InstrNode* new_region_header = NULL;
    int number_of_hoists = 0;
    int number_of_sinks = 0;
    InstrNode* use_instr;
    int def_instr_idx;
    bool is_hoisted_value_copy, is_removed_copy;
    InstrNode* preload_instr;
    InstrNode* last_peeled_instr;
    /* Variables for loop peeling */
    int cur_instr_successor_num = 0;
    int last_instr_successor_num = 0;

    last_peeled_instr = loop_prologue_header;
    /* restore old status of loop entry */
    old_loop_entry_outer_pred_instr = loop_entry_outer_pred_instr;
    old_loop_entry_outer_pred_instr_succ_num = 
        loop_entry_outer_pred_instr_succ_num;

    DBG(
        {   
            FILE* fd = fopen("before.xvcg", "w+");
            printf("========= before code motion\n");
            print_loop_as_xvcg(fd, cfg, loop_entry_instr);
            fclose(fd);
        }
        );

    last_instr_successor_num = 0;
    for (i=loop_instr_array_size-1; i>=0; i--) {
        bool is_hoisted;
        InstrNode* instr = loop_instr_array[i].instr;
        /* I cannot keep the correct live information for the
           instruction inside the loop. Hence I'd rather clear the information. */
        Instr_SetLive(instr, NULL);
        if (Instr_IsRegionDummy(instr)) continue;
        if (flag_looppeeling) {
            cur_instr_successor_num = loop_instr_array[i].successor_num;
            /* On-the-fly creation of a peeled loop. Note that you have 
               to adjust the target variable in case hoisting happens
               in the following code motion phase. */
            last_peeled_instr = 
                loop_peeling_after_cloning(cfg, last_peeled_instr, instr, 
                                         last_instr_successor_num, 
                                         cur_instr_successor_num);
            last_instr_successor_num = cur_instr_successor_num;
        }
        if (!is_eligible_for_code_motion(instr) || is_cmvar_overflow()) continue;

        if (is_hoistable(i)) {
            if (is_icopy_for_loopopt(instr) || is_fcopy_for_loopopt(instr)) {
                is_hoisted = false;
            } else if (Instr_IsStore(instr)) {
                DBG(printf("you can move store: "););
                continue;
            } else {
                DBG(printf("moving: "); Instr_PrintToStdout(instr););
                if (!flag_looppeeling) {
                    /* Insert the cloned instruction here. */
                    last_peeled_instr = 
                        loop_peeling_after_cloning(cfg, last_peeled_instr, 
                                                   instr, 0, 0);
                }
                instr = hoist_instr(cfg, i);
                /* Why do you think you have to do the following adjustment
                   of "loop_instr_array[i].instr? It was originally done
                   in hoist_instr(), but I moved it to here because I did not want
                   to pass "last_peeled_instr" as another argument to hoist_instr(). */
                loop_instr_array[i].instr = last_peeled_instr;
                number_of_hoists++;
                is_hoisted = true;
            }
            assert(is_icopy_for_loopopt(instr) || 
                   is_fcopy_for_loopopt(instr));
            /* Remove this copy instruction if the target value is not
               used outside of the loop (i.e. no branch is a use of 
               this copy instruction.) */
            if ((is_removed_copy = !is_live_at_exit(i))) {
                CFG_RemoveInstr(cfg, instr);
                /* Let the peeled instruction removed later by
                   setting its target to "g0" in order to avoid 
                   false register pressure. */
                if (flag_looppeeling) 
                  Instr_SetDestReg(last_peeled_instr, 0, g0);
            } 

            /* Retrieve the source and target registers of this copy. */
            src_var = Instr_GetSrcReg(instr, ((is_icopy_for_loopopt(instr)) ? 0:1));
            dst_var = Instr_GetDestReg(instr, 0);
            assert(Var_IsVariable(src_var) || src_var == g0);
            is_hoisted_value_copy = (Var_GetStorageType(src_var) == ST_VN);

            /* Adjust the target of the cloned instruction in the peeled loop. */
            if (is_hoisted) Instr_SetDestReg(last_peeled_instr, 0, src_var);

            /* Propagate the source register through this copy. 
               In addition update corresponding UD/DU informations. */
            du_chain = loop_instr_array[i].du_chain;
            ud_chain = loop_instr_array[i].ud_chain;
            def_instr_idx = (is_hoisted) ? i : next_set_bit(ud_chain, loop_instr_array_size, -1);
            use_instr = NULL;
            BACKWARD_SCAN_BV(j, du_chain, i) {
                /* This loop is driven by "prev_set_bit" from i to 0. 
                   Take care if you deal with a loop with control flows. */
                use_instr = loop_instr_array[j].instr;
                BV_ClearBit(loop_instr_array[j].ud_chain, i);
                for (k=0; k<Instr_GetNumOfSrcRegs(use_instr); k++) {
                    int src = Instr_GetSrcReg(use_instr, k);
                    if (src == dst_var || src == src_var) {
                        Instr_SetSrcReg(use_instr, k, src_var);
                        use_instr->lastUsedRegs[k] = 0;
                        if (def_instr_idx != -1) {
                            BV_SetBit(loop_instr_array[def_instr_idx].du_chain, j);
                            BV_SetBit(loop_instr_array[j].ud_chain, def_instr_idx);
                        }
                    }
                }
            }
            /* Mark last use for the hoisted instruction or the last "use_instr". 
               In case instr is a renaming copy, its source which is created for
               code motion will never be a last use. */
            if (is_hoisted || is_hoisted_value_copy) {
                InstrNode* probably_last_use_instr;
                probably_last_use_instr = (is_hoisted) ?    
                    last_peeled_instr : use_instr;
                FORWARD_SCAN_BV(j, ud_chain, loop_instr_array_size, -1) {   
                    word* du_chain = loop_instr_array[j].du_chain;
                    if (is_removed_copy) BV_ClearBit(du_chain, i);
                    if (next_set_bit(du_chain, loop_instr_array_size, -1) == -1) {
                        /* There is no use of the hoisted instruction "j" inside
                           of the loop. Hence mark instr for last use. */
                        for (k=0; k<Instr_GetNumOfSrcRegs(probably_last_use_instr); k++) {
                            if (Instr_GetSrcReg(probably_last_use_instr, k) == 
                                Instr_GetDestReg(loop_instr_array[j].instr, 0))
                            {
                                Instr_SetLastUseOfSrc(probably_last_use_instr, k);
                            }
                        }
                    }
                }
            }
        } 
        else if (is_sinkable(i)) {
            /* downward code motion out of exit */
            assert(Instr_IsStore(instr));
            /* The preload instruction is needed to load the memory value
               before loop entry. Insert the preload instruction right 
               after store instruction. I think it is good for cache as well. */
            if ((preload_instr = sink_instr(cfg, instr, i)) != NULL) number_of_sinks++;
        } 
        else if ((preload_instr = register_promotion(cfg, i)) != NULL) {
            DBG(printf("promotion: "); Instr_PrintToStdout(instr););
            number_of_hoists++;
        } 
        else {
            if (Instr_IsICopy(instr)) {
                def_instr_idx = next_set_bit(loop_instr_array[i].ud_chain, 
                                             loop_instr_array_size, -1);
                assert(def_instr_idx != -1 &&
                       next_set_bit(loop_instr_array[i].ud_chain, 
                                    loop_instr_array_size, def_instr_idx == -1));
                if (def_instr_idx < i && Instr_IsIInc(loop_instr_array[def_instr_idx].instr)) 
                {
                    /* The usual sequence of iinc is as follows:
                       ...
                       iload_0                  =>  copy il0(0), ist0
                       iinc 0 1                 =>  add il0(1), 1, il0
                       DO SOMETHING WITH STACKTOP   =>  USE ist0
                       ...
                       Then coalescing il0 and ist0 will prevent il0s of add 
                       from being assigned the same register. The usual consequence
                       is a compensation copy at the following join points, even
                       on the loop back edge. So I avoid coalescing the preceeding
                       copy from being coalesced by changing the opcode into "OR". */
                    int use_instr_idx = 
                        prev_set_bit(loop_instr_array[i].du_chain, loop_instr_array_size-1);
                    assert(use_instr_idx != -1);
                    if (use_instr_idx < def_instr_idx) {
                        DBG(printf("change copy into noncopy: ");Instr_PrintToStdout(instr););
                        change_copy_into_nonmov(instr);
                    }
                }
            }
        }
    } 

    if (number_of_hoists > 0) {
        SET_LOOP_ID(loop_prologue_header);
        new_region_header = Instr_GetSucc(loop_entry_outer_pred_instr,
                                          loop_entry_outer_pred_instr_succ_num);
        /* There has been any code motion/insertion before loop entry. 
           An actual connection of peeled loop takes place. 
           Promotion may have already inserted some preloads at the loop entry.
           Hence, use old_loop_entry_... instead. */
        connect_peeled_loop(loop_prologue_header, last_peeled_instr, cur_instr_successor_num,
                            old_loop_entry_outer_pred_instr, 
                            old_loop_entry_outer_pred_instr_succ_num);
        /* Intern the loop prologue header which is a region_dummy in a list for
           later elimination. */
        if (Instr_IsRegionDummy(loop_prologue_header)) 
            RegAlloc_register_region_dummy(loop_prologue_header);
        DBG(
            {   
                FILE* fd = fopen("after.xvcg", "w+");
                printf("========= after code motion\n");
                print_loop_as_xvcg(fd, cfg, loop_prologue_header);
                fclose(fd);
            }
            );
    } else {
        /* no code has been hoisted */ 
        DBG(
            {   
                FILE* fd = fopen("after.xvcg", "w+");
                printf("========= after code motion\n");
                print_loop_as_xvcg(fd, cfg, loop_entry_instr);
                fclose(fd);
            }
            );
        new_region_header = NULL;
    }
    return new_region_header;
}


