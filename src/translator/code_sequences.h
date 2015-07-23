/* code_sequences.h
   
   Header file for code sequences
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#ifndef __CODE_SEQUENCE_H__
#define __CODE_SEQUENCE_H__

#include "InvokeType.h"

/* some structure declaration for avaid warning */
struct CFG;
struct _methods;
struct InstrNode;
struct InlineGraph;
struct Hjava_lang_Class;
struct ArgSet_t;
struct FuncInfo;
struct SpecializedMethod_t;

struct InstrNode *
CSeq_insert_debug_code(struct CFG *cfg,
                       struct InstrNode *p_instr,
                       struct _methods *method,
                       int pc);

void
CSeq_insert_monitorenter_code(struct CFG* cfg, struct InlineGraph* graph);

void
CSeq_insert_counting_code(struct CFG* cfg, struct SpecializedMethod_t *m);

struct InstrNode *
CSeq_create_monitorenter_code(struct CFG *cfg, 
                              struct InstrNode *p_instr, 
                              int pc,
                              struct InstrNode **first_instr, 
                              int ref_reg);
struct InstrNode *
CSeq_create_monitorexit_code(struct CFG *cfg, 
                             struct InstrNode *p_instr, 
                             int pc,
                             struct InstrNode **first_instr, 
                             int ref_reg);
struct InstrNode *
CSeq_create_array_bound_checking_code(struct CFG *cfg, 
                                      struct InstrNode *p_instr,
                                      int pc,
                                      struct InstrNode **f_instr,
                                      int array_ref_index,
                                      int array_index_index);
struct InstrNode *
CSeq_create_array_store_checking_code(struct CFG *cfg, 
                                      struct InstrNode*  p_instr,
                                      int pc, 
                                      struct InstrNode **f_instr,
                                      int array_ref_index, 
                                      int value_index);
struct InstrNode *
CSeq_create_on_FSR_invalid_trap_instruction(struct CFG *cfg, 
                                            struct InstrNode *pi, 
                                            int pc,
                                            struct InstrNode **fi);
struct InstrNode *
CSeq_create_off_FSR_invalid_trap_instruction(struct CFG *cfg, 
                                             struct InstrNode *pi, 
                                             int bytecode_pc,
                                             struct InstrNode **fi);
struct InstrNode *
CSeq_create_function_prologue_code(struct CFG *cfg, 
                                   struct InlineGraph *graph);
struct InstrNode *
CSeq_create_exception_handler_prologue_code(struct CFG *cfg, 
                                            struct InlineGraph *gr);
struct InstrNode *
CSeq_create_inline_prologue_code(struct CFG *cfg, 
                                 struct InlineGraph *graph);
struct InstrNode *
CSeq_create_finally_prologue_code(struct CFG *cfg,
				  struct FinallyBlockInfo *fbinfo,
				  int ops_top);
struct InstrNode *
CSeq_create_function_epilogue_code(struct CFG *cfg, 
                                   struct InstrNode *p_instr, 
                                   int pc, 
                                   struct InstrNode **first_instr,
				   struct _methods *method);
struct InstrNode *
CSeq_create_null_pointer_checking_code(struct CFG *cfg, 
                                       struct InstrNode *p_instr,
                                       int pc, 
                                       struct InstrNode **f_instr, 
                                       int operand_stack_index);
struct InstrNode *
CSeq_create_array_bound_checking_code(struct CFG *cfg, 
                                      struct InstrNode *p_instr,
                                      int pc, 
                                      struct InstrNode **f_instr,
                                      int array_ref_index, 
                                      int array_index_index);
struct InstrNode *
CSeq_create_array_store_checking_code(struct CFG *cfg, 
                                      struct InstrNode *p_instr,
                                      int pc, 
                                      struct InstrNode **f_instr,
                                      int array_ref_index, 
                                      int value_index);
struct InstrNode *
CSeq_create_array_size_checking_code(struct CFG *cfg, 
                                     struct InstrNode *p_instr, 
                                     int pc,
                                     struct InstrNode **first_instr,
                                     int operand_stack_index);
struct InstrNode *
CSeq_create_process_class_code(struct CFG *cfg, 
                               struct InstrNode *p_instr, 
                               int pc,
                               struct InstrNode **first_instr,
                               struct Hjava_lang_Class* class);


struct InstrNode*
CSeq_create_pseudo_invoke_code(struct CFG *cfg, struct InstrNode *p_instr, 
                               int pc, struct _methods* called_method, 
                               InvokeType type, struct FuncInfo* info, 
                               int ops_top);

struct InstrNode* 
CSeq_make_corresponding_invoke_sequence(struct CFG* cfg, 
                                        struct InstrNode* p_instr, 
                                        int pc,
                                        struct _methods* callee_method, 
                                        int ops_top,
                                        struct Hjava_lang_Class* receiver_type,
                                        struct ArgSet_t* argset,
                                        struct FuncInfo* info,
                                        int meth_idx,
                                        bool need_null_check,
                                        bool resolved);

struct InstrNode*
CSeq_create_null_returning_code(struct CFG *cfg, 
                                struct InstrNode *c_instr, 
                                struct _methods *method);

struct InstrNode*
CSeq_create_call_stat_code(struct CFG *cfg, 
                           struct InstrNode *c_instr, 
                           struct _methods *called_method, 
                           int pc);
struct InstrNode*
CSeq_create_lookupswitch_code(struct CFG* cfg, 
                              struct InstrNode* p_instr, int pc,
                              int ops_top, int num_of_pairs,
                              byte* pair_array);

#endif /* __CODE_SEQUENCE_H__ */
