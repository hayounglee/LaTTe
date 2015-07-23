/* reg_alloc_util.h
   
   declarations of utility functions for register allocation
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#ifndef __REG_ALLOC_UTIL_H__
#define __REG_ALLOC_UTIL_H__

#include "InstrNode.h"
#include "reg.h"
#include "AllocStat.h"

InstrNode*     RegAlloc_generate_copy(InstrNode* instr, int type, int from_r, int to_r);
InstrNode*     RegAlloc_copy_register(AllocStat* h, InstrNode* instr, int from_r, int to_r);

int            RegAlloc_get_spill_offset(Reg r);
InstrNode*     RegAlloc_generate_spill(InstrNode* instr, int type, int from_r, int to_r);
InstrNode*     RegAlloc_spill_register(AllocStat* h, InstrNode* instr, int from_r);

InstrNode*     RegAlloc_generate_promotion(InstrNode* instr, int type, int from_r, int to_r);
InstrNode*     RegAlloc_promote_to_register(AllocStat* h, InstrNode* instr, int v, int to_r);

void           RegAlloc_generate_general_copy(InstrNode* instr, int type,
					       int from_r, int to_r);

void           RegAlloc_guarantee_register_free(AllocStat* h, InstrNode* instr, int from_r);
void           RegAlloc_guarantee_double_registers_free(AllocStat* h,
						InstrNode* instr, int from_r);

int            RegAlloc_spill_non_src_register(AllocStat*  h,
					      InstrNode* instr,
					      VarType    type);

void           RegAlloc_convert_long_to_double(AllocStat* h, int from_i_r, int to_f_r,
				    InstrNode* succ);
void           RegAlloc_convert_int_to_float(AllocStat* h, int from_i_r, int to_f_r,
				  InstrNode* succ);
void           RegAlloc_load_double(int offset, int to_f_r, InstrNode* succ);
void           RegAllloc_load_float(int offset, int to_f_r, InstrNode* succ);

#endif // !__REG_ALLOC_UTIL_H__
