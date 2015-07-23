/* InstrNode.c

   This is the implementation file to provide the structure of translated code.
   The intermediate code is a SPARC code which contains symbolic registers
   instead of physical registers.
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#include <assert.h>
#include <stdio.h>
#include "config.h"
#include "gtypes.h"
#include "CFG.h"
#include "pseudo_reg.h"
#include "InstrNode.h"
#include "fast_mem_allocator.h"
#include "method_inlining.h"
#include "cfg_generator.h"



#ifndef DBG
#define DBG(s) 
#endif DBG

#undef INLINE
#define INLINE
#include "InstrNode.ic"


///
/// Function Name : Instr_AddPred
/// Author : Yang, Byung-Sun
/// Input
///      c_instr - current instruction
///      pred - predecessor instruction
/// Output
///      'pred' is registered into 'c_instr's predecessor list.
/// Pre-Condition
///      Two arguments should not be NULL.
///      The fast memory allocation system must be running.
/// Post-Condition
/// Description
///      This function is to register a predecessor into the predecessor
///      list of an instruction.
///      As the number of predecessor cannot be preknown,
///      I use a kind of linked list. An element of linked list is not
///      a single pointer, but a chunk of pointers.
///      When a chunk is overflowed, the last of the chunk points to
///      the new allocated chunk. The chunk size is DEFAULT_PRED_NUM + 1
///      in words. 1 is for overflow link.
///      When a new chunk is allocated, the chunk is initialized to zero.
///      <- This can be modified in future.
///      Now, the method above is now modified.
///      The predecessor list is maintained with array.
///      When an overflow occurs, a new array whose size is twice as long as original.
///      And then the entries are copied to the new array.
///
void
Instr_AddPred(InstrNode* c_instr, InstrNode* pred)
{
    if (c_instr->numOfPredecessors < c_instr->maxNumOfPredecessors) { // no overflow
        c_instr->predecessors[c_instr->numOfPredecessors++] = pred;
    } else {			// overflow occurs
        int       new_size = c_instr->maxNumOfPredecessors * 2 + 1;
        InstrNode**     new_list =
	    FMA_allocate(sizeof(InstrNode*) * new_size);
        int       i;

        for (i = 0; i < c_instr->maxNumOfPredecessors; i++) {
            new_list[i] = c_instr->predecessors[i];
        }

        new_list[i] = pred;

        c_instr->predecessors = new_list;
        c_instr->numOfPredecessors++;
        c_instr->maxNumOfPredecessors = new_size;
    }
}

///
/// Function Name : Instr_RemovePred
/// Author : Yang, Byung-Sun
/// Input
///       c_instr - current instruction
///       pred - predecessor to remove
/// Output
///       'pred' is removed from the predecessor list of 'c_instr' and
///       'c_instr' is removed from the successor list of 'pred'.
/// Pre-Condition
///       'pred' and 'c_instr' should be connected correctly.
/// Post-Condition
/// Description
///
void
Instr_RemovePred(InstrNode* c_instr, InstrNode* pred)
{
    int    i;

    // search the predecessor list
    for (i = 0; i < c_instr->numOfPredecessors; i++) {
        if (c_instr->predecessors[i] == pred) {
            break;
        }
    }

    assert(i < c_instr->numOfPredecessors);

    // remove 'pred' from the list
    while (i < c_instr->numOfPredecessors - 1) {
        c_instr->predecessors[i] = c_instr->predecessors[i+1];
        i++;
    }
    c_instr->numOfPredecessors--;
}

bool
Instr_IsPred(InstrNode* c_instr, InstrNode* pred)
{
    int   i;

    for (i = 0; i < Instr_GetNumOfPreds(c_instr); i++) {
        if (pred == Instr_GetPred(c_instr, i)) {
            return true;
        }
    }

    return false;
}

int
Instr_GetPredIndex(InstrNode* c_instr, InstrNode* pred)
{
    int   i;

    for (i = 0; i < Instr_GetNumOfPreds(c_instr); i++) {
        if (pred == Instr_GetPred(c_instr, i)) {
            return i;
        }
    }

    return -1;			// not found
}


///
/// Function Name : Instr_AddSucc
/// Author : Yang, Byung-Sun
/// Input
///      c_instr - current instruction
///      succ - successor instruction
/// Output
///      succ is registered onto 'successors' list of c_instr.
/// Pre-Condition
///      'c_instr' and 'succ' must be existing node.
///      The fast memory allocation system must be activating.
/// Post-Condition
/// Description
///      This function is to register a successor onto the successor list of an
///      instruction.
///      As the number of successor cannot be preknown, I use a kind of linked list.
///      An element of linked list is not single pointer, but a chunk of pointers.
///      When a chunk is overflowed, the last of the chunk points to
///      the new allocated chunk. The chunk size is DEFAULT_SUCC_NUM + 1 in words.
///      1 is for overflow link.
///      When a new chunk is allocated, the chunk is initialized to zero.
///      <- This can be modified in future.
///      Now, the method above is now modified.
///      The predecessor list is maintained with array.
///      When an overflow occurs, a new array whose size is twice as long as original.
///      And then the entries are copied to the new array.
///
void
Instr_AddSucc(InstrNode* c_instr, InstrNode* succ)
{
    if (c_instr->numOfSuccessors < c_instr->maxNumOfSuccessors) { // no overflow
        c_instr->successors[c_instr->numOfSuccessors++] = succ;
    } else {			// overflow occurs
        int       new_size = c_instr->maxNumOfSuccessors * 2 + 1;
        InstrNode**     new_list = FMA_allocate(sizeof(InstrNode*) * new_size);
        int       i;

        for (i = 0; i < c_instr->maxNumOfSuccessors; i++) {
            new_list[i] = c_instr->successors[i];
        }

        new_list[i] = succ;

        c_instr->successors = new_list;
        c_instr->numOfSuccessors++;
        c_instr->maxNumOfSuccessors = new_size;
    }   
}



///
/// Function Name : Instr_RemoveSucc
/// Author : Yang, Byung-Sun
/// Input
///       c_instr - current instruction
///       succ - successor to remove
/// Output
///       'succ' is removed from the successor list of 'c_instr'.
/// Pre-Condition
///       'succ' and 'c_instr' should be connected correctly.
/// Post-Condition
/// Description
///
void
Instr_RemoveSucc(InstrNode* c_instr, InstrNode* succ)
{
    int    i;

    // search the successor list
    for (i = 0; i < c_instr->numOfSuccessors; i++) {
        if (c_instr->successors[i] == succ) {
            break;
        }
    }

    assert(i < c_instr->numOfSuccessors);

    // remove 'succ' from the list
    while (i < c_instr->numOfSuccessors - 1) {
        c_instr->successors[i] = c_instr->successors[i+1];
        i++;
    }
    c_instr->numOfSuccessors--;
}

bool
Instr_IsSucc(InstrNode* c_instr, InstrNode* succ)
{
    int   i;

    for (i = 0; i < Instr_GetNumOfSuccs(c_instr); i++) {
        if (succ == Instr_GetSucc(c_instr, i)) {
            return true;
        }
    }

    return false;
}

int
Instr_GetSuccIndex(InstrNode* c_instr, InstrNode* succ)
{
    int   i;

    for (i = 0; i < Instr_GetNumOfSuccs(c_instr); i++) {
        if (succ == Instr_GetSucc(c_instr, i)) {
            return i;
        }
    }

    return -1;
}



///
/// Function Name : create_format1_instruction
/// Author : Yang, Byung-Sun
/// Input
///      instr_code - instruction name
///      disp30 - 30 bit displacement which points to the address of a function
///      bytecode_pc - bytecode offset corresponding to the instruction
/// Output
///      A newly allocated instruction node is returned
///      whose format is FORMAT_1.
/// Pre-Condition
///      The fast memory allocation system must be activating.
/// Post-Condition
/// Description
///      A new instruction is created by this function and its fields
///      are set appropriately. The unset fields are initialized to zero.
///      FORMAT_1 is
///          op(31:30) | disp30(29:0)
///
InstrNode*
create_format1_instruction(int instr_code, int disp30, InstrOffset bytecode_pc)
{
    InstrNode*    new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode));

    Instr_Init(new_instr);

    new_instr->format = FORMAT_1;
    new_instr->instrCode = instr_code;

    // A call's implicit destination register is always 'o7'.
    new_instr->fields.format_1.destReg = o7;
    new_instr->fields.format_1.disp30 = disp30;

    new_instr->bytecodePC = bytecode_pc;

    return new_instr;
}

///
/// Function Name : create_format2_instruction
/// Author : Yang, Byung-Sun
/// Input
///      instr_code - instruction name
///      dest_reg - destination register
///      imm22 - 22 bit immediate data
///      bytecode_pc - bytecode offset corresponding to the instruction
/// Output
///      A pointer to the newly allocated instruction is returned.
/// Pre-Condition
///      The fast memory allocation system must be active.
/// Post-Condition
/// Description
///      Format 2 instruction is 'sethi'.
///      The format is
///         op(31:30) | rd(29:35) | op2(24:22) | imm22(21:0)
///
InstrNode*
create_format2_instruction(int         instr_code,
                           Reg         dest_reg,
                           int         imm22,
                           InstrOffset bytecode_pc)
{
    InstrNode*    new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode));

    Instr_Init(new_instr);

    new_instr->format = FORMAT_2;
    new_instr->instrCode = instr_code;

    new_instr->fields.format_2.destReg = dest_reg;
    new_instr->fields.format_2.imm22 = imm22;

    new_instr->bytecodePC = bytecode_pc;

    return new_instr;
}

///
/// Function Name : create_format3_instruction
/// Author : Yang, Byung-Sun
/// Input
///      instr_code - instruction name
///      bytecode_pc - bytecode offset corresponding to the instruction
/// Output
///      A new instruction in format 3 is created by this function.
/// Pre-Condition
///      The fast memory allocation system must be active.
/// Post-Condition
/// Description
///      Format 3 is for branch operation.
///      The destination is encoded in the edge of the CFG.
///      Therefore the field does not contain displacement.
///      When generating the real code, displacement field
///      should be supplemented.
///      'cond' is contained in the instr_code.
///      The format is
///          op(31:30) | a(29) | cond(28:25) | op2(24:22) | disp22(21:0)
///
InstrNode*
create_format3_instruction(int instr_code, InstrOffset bytecode_pc)
{
    InstrNode*    new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode));

    Instr_Init(new_instr);

    new_instr->format = FORMAT_3;
    new_instr->instrCode = instr_code;

    new_instr->fields.format_3.srcReg = cc0; // This is implicit in SPARC v8.

    new_instr->bytecodePC = bytecode_pc;

    return new_instr;
}

///
/// Function Name : create_format4_instruction
/// Author : Yang, Byung-Sun
/// Input
///      instr_code - instruction name
///      dest_reg - destination register
///      src_reg_1 - first source register
///      src_reg_2 - second source register
///      bytecode_pc - bytecode offset corresponding to the instruction
/// Output
///      A new instruction in format 4 is created.
/// Pre-Condition
///      The fast memory allocation system must be active.
/// Post-Condition
/// Description
///      Format 4 is for memory instructions which only uses registers.
///      The format is
///        op(31:30) | rd(29:25) | op3(24:19) | rs1(18:14) | 0 | asi(12:5) | rs2(4:0)
///
InstrNode*
create_format4_instruction(int         instr_code,
                           Reg         dest_reg,
                           Reg         src_reg_1,
                           Reg         src_reg_2,
                           InstrOffset bytecode_pc)
{
#ifdef __GNUC__
    InstrNode*    new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode)+sizeof(Field*));
#else
    InstrNode*    new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode));
#endif  //__GNUC__

    Instr_Init(new_instr);

    new_instr->format = FORMAT_4;
    new_instr->instrCode = instr_code;

    new_instr->fields.format_4.destReg = dest_reg;
    new_instr->fields.format_4.srcReg_1 = src_reg_1;
    new_instr->fields.format_4.srcReg_2 = src_reg_2;

    new_instr->bytecodePC = bytecode_pc;

    return new_instr;
}

///
/// Function Name : create_format5_instruction
/// Author : Yang, Byung-Sun
/// Input
///      instr_code - instruction name
///      dest_reg - destination register
///      src_reg - source register
///      simm13 - 13 bit signed immediate value
///      bytecode_pc - bytecode offset corresponding to the instruction
/// Output
///      A new instruction in format 5 is created.
/// Pre-Condition
///      The fast memory allocation system must be active.
/// Post-Condition
/// Description
///      Format 5 is for instructions which uses an immediate value.
///      The format 5 is
///        op(31:30) | rd(29:25) | op3(24:19) | rs1(18:14) | 1 | simm13(12:0)
///
InstrNode*
create_format5_instruction(int         instr_code,
                           Reg         dest_reg,
                           Reg         src_reg,
                           short       simm13,
                           InstrOffset bytecode_pc)
{
#ifdef __GNUC__
    InstrNode*    new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode)+sizeof(Field*));
#else
    InstrNode*    new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode));
#endif  //__GNUC__

    Instr_Init(new_instr);

    new_instr->format = FORMAT_5;
    new_instr->instrCode = instr_code;

    new_instr->fields.format_5.destReg = dest_reg;
    new_instr->fields.format_5.srcReg = src_reg;


    new_instr->fields.format_5.simm13 = simm13;
    assert(new_instr->fields.format_5.simm13 <= 0xfff 
            && new_instr->fields.format_5.simm13 >= -0x1000);

    new_instr->bytecodePC = bytecode_pc;

    return new_instr;
}

///
/// Function Name : create_format6_instruction
/// Author : Yang, Byung-Sun
/// Input
///      instr_code - instruction name
///      dest_reg - destination register
///      src_reg_1 - first source register
///      src_reg_2 - second source register
///      bytecode_pc - bytecode offset corresponding to the instruction
/// Output
///      A new instruction in format 6 is created.
/// Pre-Condition
///      The fast memory allocation system must be active.
/// Post-Condition
/// Description
///      Format 6 is for memory instructions which only uses registers.
///      The format is
///        op(31:30) | rd(29:25) | op3(24:19) | rs1(18:14) | opf(13:5) | rs2(4:0)
///      'opf' field is encoded in the instr_code.
///
InstrNode*
create_format6_instruction(int         instr_code,
                           Reg         dest_reg,
                           Reg         src_reg_1,
                           Reg         src_reg_2,
                           InstrOffset bytecode_pc)
{
    InstrNode*    new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode));

    Instr_Init(new_instr);

    new_instr->format = FORMAT_6;
    new_instr->instrCode = instr_code;

    new_instr->fields.format_6.destReg = dest_reg;
    new_instr->fields.format_6.srcReg_1 = src_reg_1;
    new_instr->fields.format_6.srcReg_2 = src_reg_2;

    new_instr->bytecodePC = bytecode_pc;

    return new_instr;
}

///
/// Function Name : create_format7_instruction
/// Author : Yang, Byung-Sun
/// Input
///       instr_code - code of store instruction
///       src_reg_3 - register from which a value is read
///       src_reg_1 - register from which a destination is computed
///       src_reg_2 - register from which a destination is computed
///       bytecode_pc - bytecode offset
/// Output
///       store instructions are now created.
/// Pre-Condition
/// Post-Condition
/// Description
///       store instructions can three source registers.
///
InstrNode*
create_format7_instruction(int         instr_code,
                           Reg         src_reg_3,
                           Reg         src_reg_1,
                           Reg         src_reg_2,
                           InstrOffset bytecode_pc)
{
#ifdef __GNUC__
    InstrNode*    new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode)+sizeof(Field*));
#else
    InstrNode*    new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode));
#endif  //__GNUC__

    Instr_Init(new_instr);

    new_instr->format = FORMAT_7;
    new_instr->instrCode = instr_code;

    new_instr->fields.format_7.srcReg_3 = src_reg_3;
    new_instr->fields.format_7.srcReg_1 = src_reg_1;
    new_instr->fields.format_7.srcReg_2 = src_reg_2;

    new_instr->bytecodePC = bytecode_pc;

    return new_instr;
}

InstrNode*
create_format8_instruction(int         instr_code,
                           Reg         src_reg_2,
                           Reg          src_reg_1,
                           short       simm13,
                           InstrOffset bytecode_pc)
{
#ifdef __GNUC__
    InstrNode*    new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode)+sizeof(Field*));
#else
    InstrNode*    new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode));
#endif  //__GNUC__

   assert(instr_code != LD && instr_code != LDD && instr_code != LDF &&
          instr_code != LDDF);

   Instr_Init(new_instr);

   new_instr->format = FORMAT_8;
   new_instr->instrCode = instr_code;

   new_instr->fields.format_8.srcReg_2 = src_reg_2;
   new_instr->fields.format_8.srcReg_1 = src_reg_1;
   new_instr->fields.format_8.simm13 = simm13;

   new_instr->bytecodePC = bytecode_pc;

   return new_instr;
}

InstrNode*
create_format9_instruction(int instr_code, short intrp_num, InstrOffset bytecode_pc)
{
    InstrNode*   new_instr = (InstrNode*) FMA_allocate(sizeof(InstrNode));

    Instr_Init(new_instr);

    new_instr->format = FORMAT_9;
    new_instr->instrCode = instr_code;
    new_instr->fields.format_9.intrpNum = intrp_num;
    new_instr->bytecodePC = bytecode_pc;

    return new_instr;
}





int
Instr_GetNumOfDestRegs(InstrNode* c_instr)
{
    switch (c_instr->format) {
      case FORMAT_1:   case FORMAT_2:
      case FORMAT_4:   case FORMAT_5:    case FORMAT_6:

        return 1;


      default:
        return 0;
    }
}

Reg
Instr_GetDestReg(InstrNode* c_instr, int index)
{
    // we can't assure for the following assertion. 
    // assert(index < Instr_GetNumOfDestRegs(c_instr)); 
    //
    // branch and store instructions have no destination register.
    // In this case, this function returns an invalid register.
    //

    switch (c_instr->format) {
      case FORMAT_1:
        return o7;

      case FORMAT_2:
        return c_instr->fields.format_2.destReg;

      case FORMAT_4:
        return c_instr->fields.format_4.destReg;

      case FORMAT_5:
        return c_instr->fields.format_5.destReg;

      case FORMAT_6:
        return c_instr->fields.format_6.destReg;



      default:
        return INVALID_REG;
    }
}

void
Instr_SetDestReg(InstrNode* c_instr, int index, Reg dest_reg)
{
    // we can't assure for the following assertion. 
    // assert(index < Instr_GetNumOfDestRegs(c_instr)); 

    switch (c_instr->format) {
      case FORMAT_1:
        c_instr->fields.format_1.destReg = dest_reg;
        return;

      case FORMAT_2:
        c_instr->fields.format_2.destReg = dest_reg;
        return;

      case FORMAT_4:
        c_instr->fields.format_4.destReg = dest_reg;
        return;

      case FORMAT_5:
        c_instr->fields.format_5.destReg = dest_reg;
        return;

      case FORMAT_6:
        c_instr->fields.format_6.destReg = dest_reg;
        return;



      default:
        assert(false);
    }
}

int
Instr_GetNumOfSrcRegs(InstrNode* c_instr)
{
    switch (c_instr->format) {
      case FORMAT_4:
        return 2;

      case FORMAT_5:
        return 1;

      case FORMAT_6:
        return 2;

      case FORMAT_7:
        return 3;

      case FORMAT_8:
        return 2;



      default:
        return 0;
    }
}


///
/// Function Name : Instr_GetSrcReg
/// Author : Yang, Byung-Sun
/// Input
///      c_instr - current instruction
///      index - register index
/// Output
///      This function returns the 'index'th source register of c_instr.
/// Pre-Condition
///      index must start from 0.
/// Post-Condition
///      If c_instr contains no 'index'th source register, it returns INVALID_REG.
/// Description
///
Reg
Instr_GetSrcReg(InstrNode* c_instr, int index)
{
    switch (c_instr->format) {
      case FORMAT_4:
        switch (index) {
          case 0:
            return c_instr->fields.format_4.srcReg_1;

          case 1:
            return c_instr->fields.format_4.srcReg_2;

          default:
            return INVALID_REG;
        }

      case FORMAT_5:
        switch (index) {
          case 0:
            return c_instr->fields.format_5.srcReg;

          default:
            return (Reg) INVALID_REG;
        }

      case FORMAT_6:
        switch (index) {
          case 0:
            return c_instr->fields.format_6.srcReg_1;

          case 1:
            return c_instr->fields.format_6.srcReg_2;

          default:
            return INVALID_REG;
        }

      case FORMAT_7:
        switch (index) {
          case 0:
            return c_instr->fields.format_7.srcReg_1;

          case 1:
            return c_instr->fields.format_7.srcReg_2;

          case 2:
            return c_instr->fields.format_7.srcReg_3;

          default:
            return INVALID_REG;
        }

      case FORMAT_8:
        switch (index) {
          case 0:
            return c_instr->fields.format_8.srcReg_1;

          case 1:
            return c_instr->fields.format_8.srcReg_2;

          default:
            return INVALID_REG;
        }




      default:
        return INVALID_REG;
    }
}

void
Instr_SetSrcReg(InstrNode* c_instr, int index, Reg src_reg)
{
    switch (c_instr->format) {
      case FORMAT_4:
        switch (index) {
          case 0:
            c_instr->fields.format_4.srcReg_1 = src_reg;
            return;

          case 1:
            c_instr->fields.format_4.srcReg_2 = src_reg;
            return;

          default:
            return;
        }

      case FORMAT_5:
        switch (index) {
          case 0:
            c_instr->fields.format_5.srcReg = src_reg;
            return;

          default:
            return;
        }

      case FORMAT_6:
        switch (index) {
          case 0:
            c_instr->fields.format_6.srcReg_1 = src_reg;
            return;

          case 1:
            c_instr->fields.format_6.srcReg_2 = src_reg;
            return;

          default:
            return;
        }

      case FORMAT_7:
        switch (index) {
          case 0:
            c_instr->fields.format_7.srcReg_1 = src_reg;
            return;

          case 1:
            c_instr->fields.format_7.srcReg_2 = src_reg;
            return;

          case 2:
            c_instr->fields.format_7.srcReg_3 = src_reg;
            return;

          default:
            return;
        }

      case FORMAT_8:
        switch (index) {
          case 0:
            c_instr->fields.format_8.srcReg_1 = src_reg;
            return;

          case 1:
            c_instr->fields.format_8.srcReg_2 = src_reg;
            return;

          default:
            return;
        }



      default:
        return;
    }
}


///
/// Function Name : Instr_SetLastUseOfSrc
/// Author : Yang, Byung-Sun
/// Input
///      c_instr - current instruction
///      index - source register index
/// Output
///      The first souce register is predicated as last use.
/// Pre-Condition
///      I assume that for an instruction only two source registers
///      is needed to be considered.
///      An index starts from 0.
/// Post-Condition
/// Description
///      On constructing a control flow graph, a symbolic register
///      corresponding to stack entry is last used when it is used
///      as a source. So we can know if the register is used last time
///      roughly at translate stage 2 without expensive data flow analysis.
///      When c_instr has no 'index'th source register, this function
///      returns silently.
///
void
Instr_SetLastUseOfSrc(InstrNode* c_instr, int index)
{
    switch (c_instr->format) {
      case FORMAT_4:
        if (index == 0) {
            c_instr->lastUsedRegs[0] = c_instr->fields.format_4.srcReg_1;
        } else if (index == 1) {
            c_instr->lastUsedRegs[1] = c_instr->fields.format_4.srcReg_2;
        } else {
            assert(false);
        }
        return;

      case FORMAT_5:
        if (index == 0) {
            c_instr->lastUsedRegs[0] = c_instr->fields.format_5.srcReg;
        } else {
            assert(false);
        }
        return;

      case FORMAT_6:
        if (index == 0) {
            c_instr->lastUsedRegs[0] = c_instr->fields.format_6.srcReg_1;
        } else if (index == 1) {
            c_instr->lastUsedRegs[1] = c_instr->fields.format_6.srcReg_2;
        } else {
            assert(false);
        }
        return;

      case FORMAT_7:
        switch (index) {
          case 0:
            c_instr->lastUsedRegs[0] = c_instr->fields.format_7.srcReg_1;
            return;

          case 1:
            c_instr->lastUsedRegs[1] = c_instr->fields.format_7.srcReg_2;
            return;

          case 2:
            c_instr->lastUsedRegs[2] = c_instr->fields.format_7.srcReg_3;
            return;

          default:
            assert(false);
            return;
        }

      case FORMAT_8:
        switch (index) {
          case 0:
            c_instr->lastUsedRegs[0] = c_instr->fields.format_8.srcReg_1;
            return;

          case 1:
            c_instr->lastUsedRegs[1] = c_instr->fields.format_8.srcReg_2;
            return;

          default:
            assert(false);
            return;
        }



      default:
        assert(false);
    }
}


bool
Instr_IsCTI(InstrNode* c_instr)
{

    if (Instr_IsIBranch(c_instr)||Instr_IsFBranch(c_instr)) {
        return true;
    } else {
        switch(Instr_GetCode(c_instr)) {
          case CALL: 
          case GOSUB: 
          case JMPL:
            return true;
        }
    }
    return false;
}

bool
Instr_IsMethodEnd(InstrNode *instr) 
{
    InlineGraph *graph = instr->graph;
    
    if (graph == NULL) return false;

    if (graph->tail == instr) return true;
    else return false;
}

bool
Instr_IsMethodStart(InstrNode *instr) 
{
    InlineGraph *graph = instr->graph;
    
    if (graph == NULL) return false;

    if (graph->head == instr) return true;
    else return false;
}


void
FuncInfo_init(FuncInfo* func_info,
              int       num_of_args,
              int*      arg_vars,
              int       num_of_rets,
              int*      ret_vars)
{
    int    i;

    func_info->argInfo.numOfArgs = num_of_args;
    if (num_of_args > 0) {
        func_info->argInfo.argVars = FMA_calloc(sizeof(int) * num_of_args);
    }

    for (i = 0; i < num_of_args; i++) {
        func_info->argInfo.argVars[i] = arg_vars[i];
    }

    func_info->retInfo.numOfRets = num_of_rets;
    for (i = 0; i < num_of_rets; i++) {
        func_info->retInfo.retVars[i] = ret_vars[i];
    }
}
/* Name        : Instr_make_func_info
   Description : make FuncInfo instance which will be attached to call instruction.
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
FuncInfo*
Instr_make_func_info(Method* meth, int ops_top, int arg_size)
{
    FuncInfo* info;
    int *arg_vars = (int *) alloca(arg_size * sizeof(int));
    int ret_vars[2];
    int return_type = Method_GetRetType(meth);
    int ret_size = Method_GetRetSize(meth);
    int first_arg_idx = ops_top - arg_size + 1;

    CFGGen_preprocess_for_method_invocation(meth, first_arg_idx, arg_vars);
    
    CFGGen_postprocess_for_method_invocation(return_type, ret_vars);

    info = FuncInfo_alloc();
    FuncInfo_init(info, arg_size, arg_vars, ret_size, ret_vars);

    return info;
}

///
/// Function Name : Instr_create_store
/// Author : Yang, Byung-Sun
/// Input
///      from_reg - physical register to be spilled
///      to_reg - spill register which is converted to a stack entry
/// Output
///      A spill instruction will be generated.
/// Pre-Condition
///      to_reg's type should be ST_SPILL.
/// Post-Condition
/// Description
///      The area for local variables spilled when exception occurs, is also
///      spill area and the number of spill register is given considering this
///      area. So 'store' instruction is as follows.
///            st from_reg, [fp - 8 - 4 * reg_num]
///
InstrNode*
Instr_create_store(Reg from_reg, Reg to_reg)
{
    InstrNode*    new_instr;
    int           spill_index = CFG_get_var_number(to_reg);

    // create a store instruction
    assert(Reg_IsHW(from_reg) && Reg_IsSpill(to_reg));

    new_instr = create_format8_instruction(((from_reg < f0) ? ST : STF),
                                            from_reg,
                                            fp,
                                            - 8 - 4 * spill_index,
                                            -1);

    return new_instr;
}


///
/// Function Name : Instr_create_load
/// Author : Yang, Byung-Sun
/// Input
///      from_reg - spill register from which to be loaded
///      to_reg - destination register
/// Output
///      A despill instruction is generated.
/// Pre-Condition
///      from_reg's type should be ST_SPILL and to_reg's type a physical register.
/// Post-Condition
/// Description
///      The despill instruction is
///            ld [fp - 8 - 4 * reg_num], to_reg
///
InstrNode*
Instr_create_load(Reg from_reg, Reg to_reg)
{
    InstrNode*   new_instr;
    int          spill_index = CFG_get_var_number(from_reg);


    // create a load instruction
    assert(Reg_IsSpill(from_reg) && Reg_IsHW(to_reg));
    new_instr = create_format5_instruction(((to_reg < f0) ? LD : LDF),
                                            to_reg,
                                            fp,
                                            -8 - 4 * spill_index,
                                            -1);

    return new_instr;
}



/////////////////////////////////////////////////////////////////////
///// TYPE ANALYSIS specific
/////////////////////////////////////////////////////////////////////
#ifdef TYPE_ANALYSIS

#include "CFG.h"
extern CFG* cfg;

void
Instr_ChangeCallTarget(InstrNode* c_instr, void* new_target)
{
#ifdef CUSTOMIZATION
    assert(c_instr->instrCode == CALL || c_instr->instrCode == INVOKE);
    assert(new_target != 0 && (int)new_target % 4 == 0); 

    c_instr->instrCode = CALL;
    c_instr->fields.format_1.disp30 = (int)new_target; 

    if (c_instr->resolveEntry == NULL){
        add_new_resolve_instr(cfg, c_instr, new_target);
    } else {
        c_instr->resolveEntry->dataToResolve = new_target; 
        c_instr->resolveEntry->instrCode = CALL; 
    }

#else
    assert(Instr_IsCall(c_instr)); 
    assert(new_target != 0 && (int)new_target % 4 == 0); 

    c_instr->fields.format_1.disp30 = (int)new_target; 
    c_instr->resolveEntry->dataToResolve = new_target; 
#endif /* CUSTOMIZATION */
}

Var
Instr_GetReceiverReferenceVariable(InstrNode* c_instr)
{
    Var obj_ref_var = INVALID_REG; 
    assert(Instr_IsStartOfInvokeSequence(c_instr));

    if (Instr_IsCall(c_instr)) {
        if (Instr_GetNumOfArgs(c_instr) == 0) return obj_ref_var;

        obj_ref_var = Instr_GetArg(c_instr, 0);
//        assert(get_var_type(obj_ref_var) == T_REF); 
    } else {
        // ld-ld-jmpl...
        assert(Instr_IsLoad(c_instr)); 
        assert(0);    // FIXME: currently
    }

    return obj_ref_var; 
}

#endif /* TYPE_ANALYSIS */



//
//
//
//
typedef struct {
    int opcode;
    char * name;
} OpcodeName;

OpcodeName SPARCOpcodeNames[] = {

    { SPARC_SAVE, "save" },
    { RESTORE, "restore" },

    { CALL, "call" },
    { SETHI, "sethi" },
    { GOSUB, "gosub" },
#ifdef CUSTOMIZATION
    { INVOKE, "invoke" },
#endif

    { JMPL, "jmpl" },

    { BA,  "ba" },
    { BN,  "bn" },
    { BNE, "bne" },
    { BE, "be" },
    { BG, "bg" },
    { BLE, "ble" },
    { BGE, "bge" },
    { BL, "bl" },
    { BGU, "bgu" },
    { BLEU, "bleu" },
    { BCC, "bcc" },
    { BCS, "bcs" },
    { BPOS, "bpos" },
    { BNEG, "bneg" },
    { BVC, "bvc" },
    { BVS, "bvs" },

    { BA|ANNUL_BIT,  "ba,a" },
    { BN|ANNUL_BIT,  "bn,a" },
    { BNE|ANNUL_BIT, "bne,a" },
    { BE|ANNUL_BIT, "be,a" },
    { BG|ANNUL_BIT, "bg,a" },
    { BLE|ANNUL_BIT, "ble,a" },
    { BGE|ANNUL_BIT, "bge,a" },
    { BL|ANNUL_BIT, "bl,a" },
    { BGU|ANNUL_BIT, "bgu,a" },
    { BLEU|ANNUL_BIT, "bleu,a" },
    { BCC|ANNUL_BIT, "bcc,a" },
    { BCS|ANNUL_BIT, "bcs,a" },
    { BPOS|ANNUL_BIT, "bpos,a" },
    { BNEG|ANNUL_BIT, "bneg,a" },
    { BVC|ANNUL_BIT, "bvc,a" },
    { BVS|ANNUL_BIT, "bvs,a" },

    { ADD, "add" },
    { ADDCC, "addcc" },
    { ADDX, "addx" },
    { ADDXCC, "addxcc" },
    { SUB, "sub" },
    { SUBCC, "subcc" },
    { SUBX, "subx" },
    { SUBXCC, "subxcc" },
    { UMUL, "umul" },
    { SMUL, "smul" },
    { UMULCC, "umulcc" },
    { SMULCC, "smulcc" },
    { MULX, "mulx" },
    { UDIV, "udiv" },
    { SDIV, "sdiv" },
    { UDIVCC, "udivcc" },
    { SDIVCC, "sdivcc" },
    { UDIVX, "udivx" },
    { SDIVX, "sdivx" },

    { AND, "and" },
    { ANDCC, "andcc" },
    { ANDN, "andn" },
    { ANDNCC, "andncc" },
    { OR, "or" },
    { ORCC, "orcc" },
    { ORN, "orn" },
    { ORNCC, "orncc" },
    { XOR, "xor" },
    { XORCC, "xorcc" },
    { XNOR, "xnor" },
    { XNORCC, "xnorcc" },

    { SLL, "sll" },
    { SRL, "srl" },
    { SRA, "sra" },
    { SLLX, "sllx" },
    { SRLX, "srlx" },
    { SRAX, "srax" },

    { LDSB, "ldsb" },
    { LDSH, "ldsh" },
    { LDUB, "ldub" },
    { LDUH, "lduh" },
    { LD, "ld" },
    { LDD, "ldd" },
    { LDF, "ldf" },
    { LDDF, "lddf" },
    { LDFSR, "ldfsr" },
    { STB, "stb" },
    { STH, "sth" },
    { ST, "st" },
    { STD, "std" },
    { STF, "stf" },
    { STDF, "stdf" },
    { SPARC_SWAP, "swap" },

    { FITOS, "fitos" },
    { FITOD, "fitod" },
    { FXTOS, "fxtos" },
    { FXTOD, "fxtod" },
    { FSTOI, "fstoi" },
    { FSTOX, "fstox" },
    { FDTOI, "fdtoi" },
    { FDTOX, "fdtox" },
    { FSTOD, "fstod" },
    { FDTOS, "fdtos" },

    { FMOVS, "fmovs" },
    { FMOVD, "fmovd" },
    { FNEGS, "fnegs" },
    { FNEGD, "fnegd" },

    { FADDS, "fadds" },
    { FADDD, "faddd" },
    { FSUBS, "fsubs" },
    { FSUBD, "fsubd" },
    { FMULS, "fmuls" },
    { FMULD, "fmuld" },
    { FDIVS, "fdivs" },
    { FDIVD, "fdivd" },

    { FCMPS, "fcmps" },
    { FCMPD, "fcmpd" },

    { TA, "ta" },
    { TN, "tn" },
    { TNE, "tne" },
    { TE, "te" },
    { TG, "tg" },
    { TLE, "tle" },
    { TGE, "tge" },
    { TL, "tl" },
    { TGU, "tgu" },
    { TLEU, "tleu" },
    { TCC, "tcc" },
    { TCS, "tcs" },
    { TPOS, "tpos" },
    { TNEG, "tneg" },
    { TVC, "tvc" },
    { TVS, "tvs" },

    { FBA, "fba" },
    { FBN, "fbn" },
    { FBU, "fbu" },
    { FBUG,"fbug" },
    { FBL, "fbl" },
    { FBUL,"fbul" },
    { FBLG,"fblg" },
    { FBNE,"fbne" },
    { FBE, "fbe" },
    { FBUE,"fbue" },
    { FBGE,"fbge" },
    { FBUGE,"fbuge" },
    { FBLE, "fble" },
    { FBULE,"fbule" },
    { FBO, "fbo" },
    { FBG, "fbg" },

    { FBA|ANNUL_BIT, "fba,a" },
    { FBN|ANNUL_BIT, "fbn,a" },
    { FBU|ANNUL_BIT, "fbu,a" },
    { FBUG|ANNUL_BIT,"fbug,a" },
    { FBL|ANNUL_BIT, "fbl,a" },
    { FBUL|ANNUL_BIT,"fbul,a" },
    { FBLG|ANNUL_BIT,"fblg,a" },
    { FBNE|ANNUL_BIT,"fbne,a" },
    { FBE|ANNUL_BIT, "fbe,a" },
    { FBUE|ANNUL_BIT,"fbue,a" },
    { FBGE|ANNUL_BIT,"fbge,a" },
    { FBUGE|ANNUL_BIT,"fbuge,a" },
    { FBLE|ANNUL_BIT, "fble,a" },
    { FBULE|ANNUL_BIT,"fbule,a" },
    { FBO|ANNUL_BIT, "fbo,a" },
    { FBG|ANNUL_BIT, "fbg,a" },

    { WRY, "wry" },
    { RDY, "rdy" },

    { DUMMY_OP, "dummy" },
    

    { 0, NULL }
};

char*
Instr_GetOpName(InstrNode* instr)
{
    static char buf[256];
    OpcodeName * array = SPARCOpcodeNames;

    while(array->name) {
        if (array->opcode == instr->instrCode) {
            return array->name;
        }
            
        array++;
    } 

    sprintf(buf,"(unknown 0x%x)",instr->instrCode);
    return buf;
}


///
/// Function Name : Instr_Print
/// Author : ramdrive
/// Input : Instruction to be printed. FILE pointer to which
///         send the message. A tail character which will 
///         followed by message. Generally tail is '\n' or '\0'.
/// Output : No side effect. print out the contents of InstrNode.
/// Pre-Condition : instr != NULL
/// Post-Condition : nothing
/// Description
///     For debugging support.
///

void Instr_Print(InstrNode* instr, FILE * fp, char tail)
{
    char*   name;
    extern byte* text_seg_start;

    void*   native_pc;

    native_pc = ((int *) text_seg_start) + Instr_GetNativeOffset(instr);

    assert(instr);

    name = Instr_GetOpName(instr);

    switch(instr->format) {
      case FORMAT_1:
        if (instr->nativeOffset != -1) {
            fprintf(fp, "%8p(%4d): %s %x",
                     native_pc, instr->bytecodePC, name,
                     instr->fields.format_1.disp30);
        } else {
            fprintf(fp, "%4d: %s %x",
                     instr->bytecodePC, name,
                     instr->fields.format_1.disp30);
        }
        if (Instr_IsCall(instr)) {
            int i;
            fprintf(fp,"(");
            for(i = 0; i < Instr_GetNumOfArgs(instr); i++) {
                fprintf(fp, "%s ", Var_get_var_reg_name(Instr_GetArg(instr, i)));
            }
            fprintf(fp,")->(");
            for(i = 0; i < Instr_GetNumOfRets(instr); i++) {
                fprintf(fp,"%s ", Var_get_var_reg_name(Instr_GetRet(instr, i)));
            }
            fprintf(fp,")");
        }
        fprintf(fp,"%c", tail);
        break;

      case FORMAT_2:
        if (instr->nativeOffset != -1) {
            fprintf(fp, "%8p(%4d): %s 0x%x, %s%c",
                     native_pc, instr->bytecodePC, name,
                     instr->fields.format_2.imm22,
                     Var_get_var_reg_name(instr->fields.format_2.destReg), tail);
        } else {
            fprintf(fp, "%4d: %s 0x%x,%s%c",
                    instr->bytecodePC, name,
                    instr->fields.format_2.imm22, 
                    Var_get_var_reg_name(instr->fields.format_2.destReg), tail);
        }
        break;
       
      case FORMAT_3:
        if (instr->nativeOffset != -1) {
            fprintf(fp, "%8p(%4d): %s (%x) %c",
                     native_pc, instr->bytecodePC, name, 
                     (unsigned int) (native_pc + ((instr->fields.format_3.disp22)<<2)), tail);
        } else {
            fprintf(fp, "%4d: %s %c",
                     instr->bytecodePC, name, tail);
        }
        break;

      case FORMAT_4:
        if (instr->nativeOffset != -1) {
            fprintf(fp, "%8p(%4d): %s %s(%i),%s(%i),%s%c",
                     native_pc, instr->bytecodePC, name,
                     Var_get_var_reg_name(instr->fields.format_4.srcReg_1),
                     Instr_IsLastUse(instr, instr->fields.format_4.srcReg_1),
                     Var_get_var_reg_name(instr->fields.format_4.srcReg_2),
                     Instr_IsLastUse(instr, instr->fields.format_4.srcReg_2),
                     Var_get_var_reg_name(instr->fields.format_4.destReg), tail);
        } else {
            fprintf(fp, "%4d: %s %s(%i),%s(%i),%s%c",
                     instr->bytecodePC, name, 
                     Var_get_var_reg_name(instr->fields.format_4.srcReg_1),
                     Instr_IsLastUse(instr, instr->fields.format_4.srcReg_1),
                     Var_get_var_reg_name(instr->fields.format_4.srcReg_2),
                     Instr_IsLastUse(instr, instr->fields.format_4.srcReg_2),
                     Var_get_var_reg_name(instr->fields.format_4.destReg), tail);
        }
        break;

      case FORMAT_5:
        if (instr->nativeOffset != -1) {
            fprintf(fp, "%8p(%4d): %s %s(%i),",
                     native_pc, instr->bytecodePC, name,
                     Var_get_var_reg_name(instr->fields.format_5.srcReg),
                     Instr_IsLastUse(instr, instr->fields.format_5.srcReg));
            if (instr->fields.format_5.simm13 >= 0) {
                fprintf(fp, "0x%x,%s",
                         instr->fields.format_5.simm13,
                         Var_get_var_reg_name(instr->fields.format_5.destReg));
            } else {
                fprintf(fp, "-%d,%s",
                         -(instr->fields.format_5.simm13),
                         Var_get_var_reg_name(instr->fields.format_5.destReg));
            }
        } else {
            fprintf(fp, "%4d: %s %s(%i),",
                    instr->bytecodePC, name,
                    Var_get_var_reg_name(instr->fields.format_5.srcReg),   
                    Instr_IsLastUse(instr, instr->fields.format_5.srcReg));
            if (instr->fields.format_5.simm13 >= 0) {
                fprintf(fp, "0x%x,%s",
                        instr->fields.format_5.simm13,
                        Var_get_var_reg_name(instr->fields.format_5.destReg));
            } else {
                fprintf(fp, "-%d,%s",
                        -(instr->fields.format_5.simm13),
                        Var_get_var_reg_name(instr->fields.format_5.destReg));
            }
        }
        if (Instr_IsCall(instr) || Instr_IsReturn(instr)) {
            int i;
            if (! Instr_IsReturn(instr)) {
                fprintf(fp,"(");
                for(i = 0; i < Instr_GetNumOfArgs(instr); i++) {
                    fprintf(fp, "%s ", Var_get_var_reg_name(Instr_GetArg(instr, i)));
                }           
                fprintf(fp,")->");
            }
            fprintf(fp,"(");
            for(i = 0; i < Instr_GetNumOfRets(instr); i++) {
                fprintf(fp,"%s ", Var_get_var_reg_name(Instr_GetRet(instr, i)));
            }
            fprintf(fp,")");
        }
        fprintf(fp,"%c", tail);

        break;

      case FORMAT_6:
        if (instr->nativeOffset != -1) {
            fprintf(fp, "%8p(%4d): %s %s(%i),%s(%i),%s",
                     native_pc, instr->bytecodePC, name,
                     Var_get_var_reg_name(instr->fields.format_6.srcReg_1),
                     Instr_IsLastUse(instr, instr->fields.format_6.srcReg_1),
                     Var_get_var_reg_name(instr->fields.format_6.srcReg_2),
                     Instr_IsLastUse(instr, instr->fields.format_6.srcReg_2),
                     Var_get_var_reg_name(instr->fields.format_6.destReg));
        } else {
            fprintf(fp, "%4d: %s %s(%i),%s(%i),%s",
                     instr->bytecodePC, name, 
                     Var_get_var_reg_name(instr->fields.format_6.srcReg_1),
                     Instr_IsLastUse(instr, instr->fields.format_6.srcReg_1),
                     Var_get_var_reg_name(instr->fields.format_6.srcReg_2),
                     Instr_IsLastUse(instr, instr->fields.format_6.srcReg_2),
                     Var_get_var_reg_name(instr->fields.format_6.destReg));
        }
        if (Instr_GetCode(instr) == JMPL && Instr_GetNumOfSuccs(instr) <= 1) {
            int i;
//           if (! Instr_IsReturn(instr) ) {
            if (Instr_IsCall(instr)) {
                fprintf(fp,"(");
                for(i = 0; i < Instr_GetNumOfArgs(instr); i++) {
                    fprintf(fp, "%s ", Var_get_var_reg_name(Instr_GetArg(instr, i)));
                }           
                fprintf(fp,")->");
            }
            fprintf(fp,"(");
            for(i = 0; i < Instr_GetNumOfRets(instr); i++) {
                fprintf(fp,"%s ", Var_get_var_reg_name(Instr_GetRet(instr, i)));
            }
            fprintf(fp,")");
        }
        fprintf(fp,"%c", tail);

        break;

      case FORMAT_7:
        if (instr->nativeOffset != -1) {
            fprintf(fp, "%8p(%4d): %s %s(%i), [%s(%i)+%s(%i)]%c",
                     native_pc, instr->bytecodePC, name,
                     Var_get_var_reg_name(instr->fields.format_7.srcReg_3),
                     Instr_IsLastUse(instr, instr->fields.format_7.srcReg_3),
                     Var_get_var_reg_name(instr->fields.format_7.srcReg_1),
                     Instr_IsLastUse(instr, instr->fields.format_7.srcReg_1),
                     Var_get_var_reg_name(instr->fields.format_7.srcReg_2),
                     Instr_IsLastUse(instr, instr->fields.format_7.srcReg_2),
                     tail);
        } else {
            fprintf(fp, "%4d: %s %s(%i), [%s(%i)+%s(%i)]%c",
                     instr->bytecodePC, name,
                     Var_get_var_reg_name(instr->fields.format_7.srcReg_3),
                     Instr_IsLastUse(instr, instr->fields.format_7.srcReg_3),
                     Var_get_var_reg_name(instr->fields.format_7.srcReg_1),
                     Instr_IsLastUse(instr, instr->fields.format_7.srcReg_1),
                     Var_get_var_reg_name(instr->fields.format_7.srcReg_2),
                     Instr_IsLastUse(instr, instr->fields.format_7.srcReg_2),
                     tail);
        }
        break;

      case FORMAT_8:
        if (instr->nativeOffset != -1) {
            fprintf(fp, "%8p(%4d): %s %s(%i), [%s(%i)",
                     native_pc, instr->bytecodePC, name,
                     Var_get_var_reg_name(instr->fields.format_8.srcReg_2),
                     Instr_IsLastUse(instr, instr->fields.format_8.srcReg_2),
                     Var_get_var_reg_name(instr->fields.format_8.srcReg_1),
                     Instr_IsLastUse(instr, instr->fields.format_8.srcReg_1));
            if (instr->fields.format_8.simm13 >= 0) {
                fprintf(fp, "+0x%x]%c",
                         instr->fields.format_8.simm13,
                         tail);
            } else {
                fprintf(fp, "-%d]%c",
                         -(instr->fields.format_8.simm13),
                         tail);
            }
        } else {
            fprintf(fp, "%4d: %s %s(%i), [%s(%i)", 
                     instr->bytecodePC, name,
                     Var_get_var_reg_name(instr->fields.format_8.srcReg_2),
                     Instr_IsLastUse(instr, instr->fields.format_8.srcReg_2),
                     Var_get_var_reg_name(instr->fields.format_8.srcReg_1),
                     Instr_IsLastUse(instr, instr->fields.format_8.srcReg_1));
            if (instr->fields.format_8.simm13 >= 0) {
                fprintf(fp, "+0x%x]%c",
                        instr->fields.format_8.simm13, tail);
            } else {
                fprintf(fp, "-%d]%c",
                        -(instr->fields.format_8.simm13), tail);
            }
        }
        break;

      case FORMAT_9:
        if (instr->nativeOffset != -1) {
            fprintf(fp, "%8p(%4d): %s %d%c",
                     native_pc, instr->bytecodePC, name,
                     instr->fields.format_9.intrpNum, tail);
        } else {
            fprintf(fp, "%4d: %s %d%c",
                     instr->bytecodePC, name,
                     instr->fields.format_9.intrpNum, tail);
        }
        break;



      default:
        assert(0 && "unknown instruction format");
    }    

}


void
Instr_PrintInXvcg(InstrNode* instr, FILE* fp)
{
    int i;
    InstrNode* header;

    assert(instr);

    if (Instr_IsVisited(instr)) return;

    Instr_MarkVisited(instr);
    
    fprintf(fp, "    node: { title: \"%p\" label: \"", instr);
    fprintf(fp, "[%8p] ", instr);
    Instr_Print(instr, fp,' ');
    
    header = instr;
    /* scan the basic block */
    while (Instr_GetNumOfSuccs(instr) == 1&&
           Instr_GetNumOfPreds(Instr_GetSucc(instr,0)) == 1)
    {
        instr = Instr_GetSucc(instr, 0);
        fprintf(fp, "\n");
        fprintf(fp, "[%8p] ", instr);
        Instr_Print(instr, fp, ' ');
        Instr_MarkVisited(instr);
    }

    fprintf(fp, "\" }\n");

    for(i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
        assert(Instr_IsPred(Instr_GetSucc(instr, i), instr));
        fprintf(fp, "    edge: { thickness: %d"
                " sourcename: \"%p\" targetname: \"%p\" }\n",3+ i * 2, header,
                Instr_GetSucc(instr, i));
	Instr_PrintInXvcg(Instr_GetSucc(instr, i), fp);
    }
}


#undef DBG

