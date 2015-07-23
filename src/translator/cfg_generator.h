/* cfg_generator.h
   
   Header file for cfg generation
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
               SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __CFG_GENERATOR_H__
#define __CFG_GENERATOR_H__

/*-----------------------------------------------------------------------------
                = structure of stack frame of a translated function =

             Before call                           After call
        +-----------------------+           +-----------------------+
   high |                       |           |                       |
   mem  |  caller's temps.      |           |     caller's temps.   |
        |                       |           |                       |
        +-----------------------+           +-----------------------+
        |                       |           |                       |
        |  arguments on stack.  |           |  arguments on stack.  |
        |                       |           |                       |
        +-----------------------+    FP+92->+-----------------------+
        |  6 words to save      |           |  6 words to save      |
        |  arguments passed     |           |  arguments passed     |
        |  in registers, even   |           |  in registers, even   |
        |  if not passed.       |           |  if not passed.       |
 SP+68->+-----------------------+    FP+68->+-----------------------+
        | 1 word struct addr    |           | 1 word struct addr    |
        +-----------------------+    FP+64->+-----------------------+
        |                       |           |                       |
        | 16 word reg save area |           | 16 word reg save area |
        |                       |           |                       |
    SP->+-----------------------+       FP->+-----------------------+
                                            | 2 word area for       |
                                            | float/int conversion  |
                                      FP-8->+-----------------------+
                                            |                       |
                                            |  JAVA local variables |
                                            |                       |
                             FP-8-4*lv_num->+-----------------------+
                                            |                       |
                                            |     spill area        |
                                            |                       |
             FP-8-4*lv_num-4*spill_reg_num->+-----------------------+
                                            |                       |
                                            |  arguments on stack   |
                                            |                       |
                                     SP+92->+-----------------------+
                                            |  6 words to save      |
                                            |  arguments passed     |
                                            |  in registers, even   |
   low                                      |  if not passed.       |
   memory                            SP+68->+-----------------------+
                                            | 1 word struct addr    |
                                     SP+64->+-----------------------+
                                            |                       |
                                            | 16 word reg save area |
                                            |                       |
                                        SP->+-----------------------+



-A stack size is double-word aligned to use 'ldd' and 'std' by OS.
-The stack size can be calculated by following equation.
   1) maximum num of arguments which is passed from the function <= 6
      (92 + 8 + 4 * (num of spill registers + num of local variables) + 8) & -8
   2) maximum num of arguments which is passed from the function > 6
      (92 + 8 + 4 * (max num of arguments - 6 + num of spill register
        + num of local variables) + 8) & -8
-referencing
   1) parameter after 6th : FP + 92 + 4 * (index - 6)
   2) local variable      : FP - 8 - 4 * index
   3) spill register      : FP - 8 - 4 * num of local variables - 4 * index

   */

/* some structure declaration for avaid warning */
struct _methods;
struct TranslationInfo;
struct CFG;
enum ReturnType;

extern struct InlineGraph *CFGGen_current_ig_node;

struct CFG *CFGGen_generate_CFG(struct TranslationInfo *info);

void CFGGen_process_and_verify_for_npc(InstrOffset npc, struct InstrNode* c_instr);
void CFGGen_process_for_branch(struct CFG *cfg, struct InstrNode *branch_instr);
void CFGGen_process_for_function_call(struct CFG *cfg, 
                                      struct InstrNode* call_instr, 
				      void *func_name, 
				      int num_of_args, int *arg_vars, 
				      int num_of_rets, int *ret_vars);
void CFGGen_process_for_indirect_function_call(struct CFG *cfg, 
                                               struct InstrNode *call_instr,
					       int num_of_args, int *arg_vars,
					       int num_of_rets, int *ret_vars);
void CFGGen_process_for_return(struct CFG *cfg, struct InstrNode* ret_instr,
			       int num_of_rets, int *ret_vars);
void CFGGen_preprocess_for_method_invocation(struct _methods *method,
					     int first_arg_idx, int *arg_vars);
void CFGGen_postprocess_for_method_invocation(enum ReturnType return_type, 
					     int *ret_vars);
void CFGGen_postprocess_for_inlined_method(enum ReturnType return_type);

#endif /* __CFG_GENERATOR_H__ */

