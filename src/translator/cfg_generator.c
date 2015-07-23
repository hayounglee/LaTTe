/* cfg_generator.c
   
   Translate bytecodes into assembly code sequences constructing CFG
   The generated assembly code is pre-register allocated SPARC
   intermediate code
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
               SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "config.h"
#include "classMethod.h"
#include "method_inlining.h"
#include "InstrNode.h"
#include "CFG.h"
#include "bytecode.h"
#include "bytecode_analysis.h"
#include "code_sequences.h"
#include "functions.h"
#include "pseudo_reg.h"
#include "lookup.h"
#include "baseClasses.h"
#include "soft.h"
#include "SpecializedMethod.h"
#include "translate.h"

#ifdef TYPE_ANALYSIS
#include "TypeAnalysis.h"
#include "TypedValue.h"
#endif /* TYPE_ANALYSIS */

#ifdef BYTECODE_PROFILE
#include "bytecode_profile.h"
#endif

#define DBG(s) 
#define FDBG(s)    
#define PDBG(s) 	
#define FDBG(s)    
#define PDBG(s) 	
#define YDBG(s)

/* static variables that should be initialized before traversing
   bytecodes */
/* currently inlining graph node */
InlineGraph *CFGGen_current_ig_node;	
static InstrNode **translated_instrs;	/* instruction arrray */
static int *ops_types;	/* operand stack type array */

/* current operand stack top */
static int ops_top;

/* for inlined exception_handling */
static void *ret_addr;
static int ret_stack_top;

#ifdef WRONG_CLASSLOADING
#include "plist.h"
extern PList *uninitialized_class_list;
#endif /* WRONG_CLASSLOADING */

static InstrNode *last_ret_translate_instr;

/* Name        : save_context, restore_context
   Description : save and restore operand stack state
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */

typedef struct Context {
    int stackTop;
    int *stackTypes;
} Context;

inline static
void
save_context(Context *ctx, int stack_top, int *types)
{
    int i;

    ctx->stackTop = stack_top;
    if (stack_top >= 0) {
	ctx->stackTypes = (int *) FMA_calloc(sizeof(int) * (stack_top + 1));
	for(i = 0; i <= stack_top; i++)
	    ctx->stackTypes[i] = types[i];
    }
}

inline static
void
restore_context(Context *ctx, int *stack_top, int *types)
{
    int i;

    *stack_top = ctx->stackTop;
    for(i = 0; i <= ctx->stackTop; i++)
	types[i] = ctx->stackTypes[i];
}

/* Name        : set_ops_type
   Description : set operand stack type
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: This function should be only used within cfg_generator and
          translate.def */
inline static
void
set_ops_type(int index, VarType type) 
{
    assert(ops_types != NULL);
    ops_types[index] = type;
}

/* Name        : get_ops_type
   Description : get operand stack type
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: This function should be only used within cfg_generator and
          translate.def */
inline static
VarType
get_ops_type(int index) 
{
    assert(ops_types != NULL);
    return ops_types[index];
}

/* Name        : set_local_variable_type
   Description : gather type information for local variable
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: The collected information is not actually type information,
          but it is number of local variable definitions within a
          method. If a local variable is defined only once, its type
          is globally unique.  */
inline static
void
set_local_variable_type(int index)
{
#ifdef TYPE_ANALYSIS
    // I wanted to gather every information including inlined methods.
    // But num_total_local_var is not easily available at this point.
    TA_Num_Of_Defs[index]++; 
#endif /* TYPE_ANALYSIS */
}

/* Name        : is_bytecode_join
   Description : Check if this bytecode is a join point or not
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
inline static
bool
is_bytecode_join(uint8 *binfo, InstrOffset pc)
{
    return binfo[pc] & METHOD_JOIN_POINT;
}


/* Name        : CFGGen_process_for_branch
   Description : handle branch instruction
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
CFGGen_process_for_branch(CFG *cfg, InstrNode *branch_instr)
{
    add_new_resolve_instr(cfg, branch_instr, NULL);
}

/* Name        : CFGGen_process_for_function_call
   Description : handle function call instruction
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
CFGGen_process_for_function_call(CFG *cfg, InstrNode* call_instr, 
				 void *func_name, 
				 int num_of_args, int *arg_vars, 
				 int num_of_rets, int *ret_vars)
{
    FuncInfo*    func_info;

    add_new_resolve_instr(cfg, call_instr, func_name);

    //
    // set call information into c_instr
    // This information will be used for the register allocation.
    //
    func_info = FuncInfo_alloc();
    FuncInfo_init(func_info, num_of_args, arg_vars, num_of_rets, ret_vars);
    Instr_SetInstrInfo(call_instr, (void*) func_info);
}


/* Name        : CFGGen_process_for_indirect_function_call
   Description : handle indirect function call instruciton
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: Currently the only difference between direct function call
          is whether add new resolve instruction or not */
void
CFGGen_process_for_indirect_function_call(CFG *cfg, InstrNode *call_instr,
					  int num_of_args, int *arg_vars,
					  int num_of_rets, int *ret_vars)
{
    FuncInfo*    func_info;

    //
    // set call information into c_instr
    // This information will be used for the register allocation.
    //
    func_info = FuncInfo_alloc();
    FuncInfo_init(func_info, num_of_args, arg_vars, num_of_rets, ret_vars);
    Instr_SetInstrInfo(call_instr, (void*) func_info);
}

/* Name        : CFGGen_process_for_return
   Description : handle return instruction
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
CFGGen_process_for_return(CFG *cfg, InstrNode* ret_instr,
			  int num_of_rets, int *ret_vars)
{
    RetInfo*    ret_info;

    ret_info = RetInfo_alloc();
    RetInfo_init(ret_info, num_of_rets, ret_vars);
    Instr_SetInstrInfo(ret_instr, (void*) ret_info);
}

/* Name        : profile_bytecode
   Description : debugging function if BYTECODE_PROFILE is turned on,
                 this function is called when every bytecode met
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: This function will be removed into bytecode_profile.c   */
void
profile_bytecode(Method* method, InstrOffset pc)
{
#ifdef BYTECODE_PROFILE
    Pbc_increase_frequency(method, pc);
#endif BYTECODE_PROFILE
}

/* Name        : CFGGen_preprocess_for_method_invocation
   Description : Check operand stack state and set operand stack state
                 according to method signature
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
CFGGen_preprocess_for_method_invocation(Method *method,
					int first_arg_idx, 
                                        int *arg_vars)
{
    char *signature;
    char *str;
    short used_words;

    assert(first_arg_idx >= -1);

    signature = Method_GetSignature(method)->data;
    used_words = 0;

    /* non-static method must have first reference argument */
    if (!Method_IsStatic(method)) {
	assert(get_ops_type(first_arg_idx) == T_REF);

        arg_vars[0] = RS(first_arg_idx);
        used_words++;
    }


    /* check operand stack state according to method signature */
    assert(signature[0] == '(');

    for(str = signature + 1; *str != ')'; str++) {
	int arg_idx = first_arg_idx + used_words;
	int array_depth = 0;

        switch (*str) {
          case 'I':
          case 'Z':
          case 'S':
          case 'B':
          case 'C':
	    /* integer type argument */
	    assert(get_ops_type(arg_idx) == T_INT);

            arg_vars[used_words] = IS(arg_idx);

            used_words++;
            break;

          case 'F':
	    /* floating point type argument */
	    assert(get_ops_type(arg_idx) == T_FLOAT);

            arg_vars[used_words] = FS(arg_idx);

            used_words++;
            break;

          case 'J':
	    /* long type argument */
	    assert(get_ops_type(arg_idx) == T_LONG);
	    assert(get_ops_type(arg_idx + 1) == T_IVOID);

            arg_vars[used_words] = LS(arg_idx);
            arg_vars[used_words + 1] = IVS(arg_idx + 1);

            used_words += 2;
            break;

          case 'D':
	    /* double type argument */
	    assert(get_ops_type(arg_idx) == T_DOUBLE);
	    assert(get_ops_type(arg_idx + 1) == T_FVOID);

            arg_vars[used_words] = DS(arg_idx);
            arg_vars[used_words+1] = FVS(arg_idx + 1);

            used_words += 2;
            break;

          case 'L':
	    /* reference type argument */
	    assert(get_ops_type(arg_idx) == T_REF);

            arg_vars[used_words] = RS(arg_idx);

            used_words++;
	    
	    /* skip redundant information */
            while(*str != ';') str++;
            break;

          case '[':
	    /* array reference type argument */
	    assert(get_ops_type(arg_idx) == T_REF);

            arg_vars[used_words] = RS(arg_idx);

            used_words++;

	    array_depth++;
	    do {
		str++;
		if (*str == 'L') while (*str != ';') str++;
		array_depth--;
		if (*str == '[') array_depth++;
	    } while(array_depth != 0);
            break;

          default:
            assert(false);
        }
    }

    //
    // decrement operand stack top
    //
    ops_top -= used_words;

}


/* Name        : CFGGen_postprocess_for_method_invocation
   Description : Set operand stack state according to return type 
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
CFGGen_postprocess_for_method_invocation(ReturnType return_type, int *ret_vars)
{
    switch(return_type) {
      case RT_NO_RETURN:
        break;

      case RT_INTEGER:
        ret_vars[0] = IS(ops_top + 1);

        set_ops_type(ops_top + 1, T_INT);

        ops_top++;
        break;

      case RT_REFERENCE:
        ret_vars[0] = RS(ops_top + 1);

        set_ops_type(ops_top + 1, T_REF);

        ops_top++;
        break;

      case RT_LONG:
        ret_vars[0] = LS(ops_top + 1);
        ret_vars[1] = IVS(ops_top + 2);

        set_ops_type(ops_top + 1, T_LONG);
        set_ops_type(ops_top + 2, T_IVOID);

        ops_top += 2;
        break;

      case RT_FLOAT:
        ret_vars[0] = FS(ops_top + 1);

        set_ops_type(ops_top + 1, T_FLOAT);

        ops_top++;
        break;

      case RT_DOUBLE:
        ret_vars[0] = DS(ops_top + 1);
        ret_vars[1] = FVS(ops_top + 2);

        set_ops_type(ops_top + 1, T_DOUBLE);
        set_ops_type(ops_top + 2, T_FVOID);

        ops_top += 2;
        break;

      default:
        assert(false);
    }
}

/* Name        : CFGGen_postprocess_for_inlined_method
   Description : Set operand stack state according to return type
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: This is another version of postprocess_for_method_invocation
          for inlined method */
void 
CFGGen_postprocess_for_inlined_method(ReturnType return_type) {
    switch (return_type) {
      case RT_NO_RETURN:
        break;

      case RT_INTEGER:
	set_ops_type(ops_top + 1, T_INT);

        ops_top++;
        break;

      case RT_REFERENCE:
        set_ops_type(ops_top + 1, T_REF);

        ops_top++;
        break;

      case RT_LONG:
	set_ops_type(ops_top + 1, T_LONG);
	set_ops_type(ops_top + 2, T_IVOID);

        ops_top += 2;
        break;

      case RT_FLOAT:
	set_ops_type(ops_top + 1, T_FLOAT);

        ops_top++;
        break;

      case RT_DOUBLE:
	set_ops_type(ops_top + 1, T_DOUBLE);
	set_ops_type(ops_top + 2, T_FVOID);

        ops_top += 2;
        break;

      default:
        assert(false);
    }
}

/* for bytecode traversal */
static bool wide;
static bool have_multi_successors;
static bool context_load_need;

static int next_pc;
static InstrNode *last_instr;

#define MAX_STACK_SIZE   1000

static int traverse_stack[MAX_STACK_SIZE];
static int traverse_stack_top;
static Context saved_contexts[MAX_STACK_SIZE];

// This is a stack of parent nodes whose top element will be linked
// with new instruction node.
// 'traverse_stack_top' is also used for this stack.
static InstrNode *parent_nodes[MAX_STACK_SIZE];

static int offset_of_0_0F[1]  = { 0x00000000 };
static int offset_of_1_0F[1]  = { 0x3f800000 };
static int offset_of_2_0F[1]  = { 0x40000000 };
static long long offset_of_0_0LF[1] = { 0x0000000000000000LL };
static long long offset_of_1_0LF[1] = { 0x3ff0000000000000LL };

/* Name        : CFGGen_process_and_verify_for_npc
   Description : For traversing bytecodes, record next pc
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
CFGGen_process_and_verify_for_npc(InstrOffset npc, InstrNode* c_instr)
{
    if (have_multi_successors) {
	traverse_stack_top++;
	save_context(&saved_contexts[traverse_stack_top], ops_top, ops_types);
	parent_nodes[traverse_stack_top] = c_instr;
        traverse_stack[traverse_stack_top] = npc;
        context_load_need = true;
    } else {
        context_load_need = false;
    }
}

/* do_translation_for definition :
       used within switch case statement
       call appropriate translate function as bytecode  */
#define do_translation_for(bytecode)					\
    case (bytecode):							\
DBG(printf("translate " #bytecode "(%d) at %d\tTOP = %d\n",		\
           bytecode, pc, ops_top););					\
        context_load_need = true;					\
PDBG(num_of_bytecode_instr++;);						\
        translate_##bytecode##(cfg, method, pc, p_instr);		\
        assert(ops_top < Method_GetStackSize(method));			\
        assert(!is_bytecode_join(binfo, pc)				\
                || translated_instrs[pc] != NULL);			\
        break;

/* do_translation_as definition :
       used within switch case statement
       expanded version for modified bytecode by interpreter  */
#define do_translation_as(bytecode, realop)				\
    case (bytecode):							\
DBG(printf("translate " #bytecode "(%d) at %d\tTOP = %d\n",		\
             bytecode, pc, ops_top););					\
        context_load_need = true;					\
        translate_##realop##(cfg, method, pc, p_instr);			\
        assert(ops_top < Method_GetStackSize(method));			\
        assert(!is_bytecode_join(binfo, pc)			\
                || translated_instrs[pc] != NULL);			\
        break;

#include "translate.def"

/* Name        : translate_bytecode
   Description : translate a bytecode into native code sequences
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
inline static
void
translate_bytecode(byte opcode, CFG *cfg, Method *method, InstrOffset pc,
		   InstrNode *p_instr)
{
    uint8 *binfo;

    have_multi_successors = false;
    binfo = Method_GetBcodeInfo(method);
    
    switch (opcode) {
        do_translation_for(NOP);
        do_translation_for(ACONST_NULL);
        do_translation_for(ICONST_M1);
        do_translation_for(ICONST_0);
        do_translation_for(ICONST_1);
        do_translation_for(ICONST_2);
        do_translation_for(ICONST_3);
        do_translation_for(ICONST_4);
        do_translation_for(ICONST_5);
        do_translation_for(LCONST_0);
        do_translation_for(LCONST_1);
        do_translation_for(FCONST_0);
        do_translation_for(FCONST_1);
        do_translation_for(FCONST_2);
        do_translation_for(DCONST_0);
        do_translation_for(DCONST_1);

        do_translation_for(BIPUSH);
        do_translation_for(SIPUSH);
        do_translation_for(LDC1);
        do_translation_for(LDC2);
        do_translation_for(LDC2W);

        do_translation_for(ILOAD);
        do_translation_for(LLOAD);
        do_translation_for(FLOAD);
        do_translation_for(DLOAD);
        do_translation_for(ALOAD);
        do_translation_for(ILOAD_0);
        do_translation_for(ILOAD_1);
        do_translation_for(ILOAD_2);
        do_translation_for(ILOAD_3);
        do_translation_for(LLOAD_0);
        do_translation_for(LLOAD_1);
        do_translation_for(LLOAD_2);
        do_translation_for(LLOAD_3);
        do_translation_for(FLOAD_0);
        do_translation_for(FLOAD_1);
        do_translation_for(FLOAD_2);
        do_translation_for(FLOAD_3);
        do_translation_for(DLOAD_0);
        do_translation_for(DLOAD_1);
        do_translation_for(DLOAD_2);
        do_translation_for(DLOAD_3);
        do_translation_for(ALOAD_0);
        do_translation_for(ALOAD_1);
        do_translation_for(ALOAD_2);
        do_translation_for(ALOAD_3);
        do_translation_for(IALOAD);
        do_translation_for(LALOAD);
        do_translation_for(FALOAD);
        do_translation_for(DALOAD);
        do_translation_for(AALOAD);
        do_translation_for(BALOAD);
        do_translation_for(CALOAD);
        do_translation_for(SALOAD);

        do_translation_for(ISTORE);
        do_translation_for(LSTORE);
        do_translation_for(FSTORE);
        do_translation_for(DSTORE);
        do_translation_for(ASTORE);
        do_translation_for(ISTORE_0);
        do_translation_for(ISTORE_1);
        do_translation_for(ISTORE_2);
        do_translation_for(ISTORE_3);
        do_translation_for(LSTORE_0);
        do_translation_for(LSTORE_1);
        do_translation_for(LSTORE_2);
        do_translation_for(LSTORE_3);
        do_translation_for(FSTORE_0);
        do_translation_for(FSTORE_1);
        do_translation_for(FSTORE_2);
        do_translation_for(FSTORE_3);
        do_translation_for(DSTORE_0);
        do_translation_for(DSTORE_1);
        do_translation_for(DSTORE_2);
        do_translation_for(DSTORE_3);
        do_translation_for(ASTORE_0);
        do_translation_for(ASTORE_1);
        do_translation_for(ASTORE_2);
        do_translation_for(ASTORE_3);
        do_translation_for(IASTORE);
        do_translation_for(LASTORE);
        do_translation_for(FASTORE);
        do_translation_for(DASTORE);
        do_translation_for(AASTORE);
        do_translation_for(BASTORE);
        do_translation_for(CASTORE);
        do_translation_for(SASTORE);

        do_translation_for(POP);
        do_translation_for(POP2);
        do_translation_for(DUP);
        do_translation_for(DUP_X1);
        do_translation_for(DUP_X2);
        do_translation_for(DUP2);
        do_translation_for(DUP2_X1);
        do_translation_for(DUP2_X2);
        do_translation_for(SWAP);

        do_translation_for(IADD);
        do_translation_for(LADD);
        do_translation_for(FADD);
        do_translation_for(DADD);
        do_translation_for(ISUB);
        do_translation_for(LSUB);
        do_translation_for(FSUB);
        do_translation_for(DSUB);
        do_translation_for(IMUL);
        do_translation_for(LMUL);
        do_translation_for(FMUL);
        do_translation_for(DMUL);
        do_translation_for(IDIV);
        do_translation_for(LDIV);
        do_translation_for(FDIV);
        do_translation_for(DDIV);
        do_translation_for(IREM);
        do_translation_for(LREM);
        do_translation_for(FREM);
        do_translation_for(DREM);
        do_translation_for(INEG);
        do_translation_for(LNEG);
        do_translation_for(FNEG);
        do_translation_for(DNEG);
        do_translation_for(ISHL);
        do_translation_for(LSHL);
        do_translation_for(ISHR);
        do_translation_for(LSHR);
        do_translation_for(IUSHR);
        do_translation_for(LUSHR);
        do_translation_for(IAND);
        do_translation_for(LAND);
        do_translation_for(IOR);
        do_translation_for(LOR);
        do_translation_for(IXOR);
        do_translation_for(LXOR);

        do_translation_for(IINC);

        do_translation_for(I2L);
        do_translation_for(I2F);
        do_translation_for(I2D);
        do_translation_for(L2I);
        do_translation_for(L2F);
        do_translation_for(L2D);
        do_translation_for(F2I);
        do_translation_for(F2L);
        do_translation_for(F2D);
        do_translation_for(D2I);
        do_translation_for(D2L);
        do_translation_for(D2F);
        do_translation_for(INT2BYTE);
        do_translation_for(INT2CHAR);
        do_translation_for(INT2SHORT);

        do_translation_for(LCMP);
        do_translation_for(FCMPL);
        do_translation_for(FCMPG);
        do_translation_for(DCMPL);
        do_translation_for(DCMPG);

        do_translation_for(IFEQ);
        do_translation_for(IFNE);
        do_translation_for(IFLT);
        do_translation_for(IFGE);
        do_translation_for(IFGT);
        do_translation_for(IFLE);
        do_translation_for(IF_ICMPEQ);
        do_translation_for(IF_ICMPNE);
        do_translation_for(IF_ICMPLT);
        do_translation_for(IF_ICMPGE);
        do_translation_for(IF_ICMPGT);
        do_translation_for(IF_ICMPLE);
        do_translation_for(IF_ACMPEQ);
        do_translation_for(IF_ACMPNE);

        do_translation_for(GOTO);
        do_translation_for(JSR);
        do_translation_for(RET);
        do_translation_for(TABLESWITCH);
        do_translation_for(LOOKUPSWITCH);

        do_translation_for(IRETURN);
        do_translation_for(LRETURN);
        do_translation_for(FRETURN);
        do_translation_for(DRETURN);
        do_translation_for(ARETURN);
        do_translation_for(RETURN);

        do_translation_for(GETSTATIC);
        do_translation_for(PUTSTATIC);
        do_translation_for(GETFIELD);
        do_translation_for(PUTFIELD);

        do_translation_for(INVOKEVIRTUAL);
        do_translation_for(INVOKESPECIAL);
        do_translation_for(INVOKESTATIC);
        do_translation_for(INVOKEINTERFACE);

        do_translation_for(NEW);
        do_translation_for(NEWARRAY);
        do_translation_for(ANEWARRAY);

        do_translation_for(ARRAYLENGTH);

        do_translation_for(ATHROW);

        do_translation_for(CHECKCAST);
        do_translation_for(INSTANCEOF);

        do_translation_for(MONITORENTER);
        do_translation_for(MONITOREXIT);

        do_translation_for(WIDE);

        do_translation_for(MULTIANEWARRAY);

        do_translation_for(IFNULL);
        do_translation_for(IFNONNULL);

        do_translation_for(GOTO_W);
        do_translation_for(JSR_W);

	/*** Internal opcodes, rewritten by the interpreter. ***/

	/* Widened opcodes.  The very next opcode should be the
           original unwidened opcode. */
	do_translation_as(ILOAD_W, WIDE);
	do_translation_as(LLOAD_W, WIDE);
	do_translation_as(FLOAD_W, WIDE);
	do_translation_as(DLOAD_W, WIDE);
	do_translation_as(ALOAD_W, WIDE);
	do_translation_as(ISTORE_W, WIDE);
	do_translation_as(LSTORE_W, WIDE);
	do_translation_as(FSTORE_W, WIDE);
	do_translation_as(DSTORE_W, WIDE);
	do_translation_as(ASTORE_W, WIDE);
	do_translation_as(RET_W, WIDE);
	do_translation_as(IINC_W, WIDE);

	/* Field access opcodes. */
	do_translation_as(GETFIELD_W, GETFIELD);
	do_translation_as(GETFIELD_S, GETFIELD);
	do_translation_as(GETFIELD_B, GETFIELD);
	do_translation_as(GETFIELD_C, GETFIELD);
	do_translation_as(GETFIELD_D, GETFIELD);
	do_translation_as(GETSTATIC_W, GETSTATIC);
	do_translation_as(GETSTATIC_S, GETSTATIC);
	do_translation_as(GETSTATIC_B, GETSTATIC);
	do_translation_as(GETSTATIC_C, GETSTATIC);
	do_translation_as(GETSTATIC_D, GETSTATIC);
	do_translation_as(PUTFIELD_W, PUTFIELD);
	do_translation_as(PUTFIELD_S, PUTFIELD);
	do_translation_as(PUTFIELD_B, PUTFIELD);
	do_translation_as(PUTFIELD_C, PUTFIELD);
	do_translation_as(PUTFIELD_D, PUTFIELD);
	do_translation_as(PUTSTATIC_W, PUTSTATIC);
	do_translation_as(PUTSTATIC_S, PUTSTATIC);
	do_translation_as(PUTSTATIC_B, PUTSTATIC);
	do_translation_as(PUTSTATIC_C, PUTSTATIC);
	do_translation_as(PUTSTATIC_D, PUTSTATIC);

      default:
        assert(false);
    }
}

/* Name        : translate_main_flow
   Description : actual traversing function called by translate_one_method
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static
void
translate_main_flow(CFG* cfg, TranslationInfo *info, InlineGraph *graph)
{
    int start_pc;
    uint8 opcode;
    Method *method;
    
    method = IG_GetMethod(graph);
    
    //
    // create some prologue code of function First, the new register
    // window is allocated with 'save' instruction.  At this phase,
    // the size of stack frame is not known, this instruction will be
    // resolved at the code generation stage. Second, the parameters
    // are copied into local variables as JVM semantics. Finally, if
    // the method is synchronized, a mutex for the object/class is
    // obtained.
    //
    if (IG_GetDepth(graph) == 0) {
        if (TI_IsFromException(info)) {
	    
            last_instr = CSeq_create_exception_handler_prologue_code(cfg,
								     graph);

	    set_ops_type(0, T_REF);
	    ops_top = 0;
        } else {
            last_instr = CSeq_create_function_prologue_code(cfg, graph);
        }
    } else {
        last_instr = CSeq_create_inline_prologue_code(cfg, graph);
    }

    if (info == NULL) {
        start_pc = 0;
    } else {
        start_pc = TI_GetStartBytecodePC(info);
    }

    traverse_stack_top++;
    save_context(&saved_contexts[traverse_stack_top], ops_top, ops_types);
    parent_nodes[traverse_stack_top] = last_instr;
    traverse_stack[traverse_stack_top] = start_pc;

    while(traverse_stack_top >= 0) {
        assert(traverse_stack_top < MAX_STACK_SIZE);

	restore_context(&saved_contexts[traverse_stack_top],
			&ops_top, ops_types);
	
	last_instr = parent_nodes[traverse_stack_top];
	next_pc = traverse_stack[traverse_stack_top--];
	
        do {
            if (translated_instrs[next_pc] != 0) {
                Instr_connect_instruction(last_instr,
					  translated_instrs[next_pc]);
                break;
            }

            opcode = BCode_get_uint8(Method_GetByteCode(method) + next_pc);

            //
            // now the code traslation is really done here.
            // The successors are push onto stack in here and
            // some kind of verification is done here.
            //
            // 'next_pc' and 'last_instr' are static global variables.
            // Their values are updated while translation.
            //
            translate_bytecode(opcode, cfg, method, next_pc, last_instr);
        } while (!context_load_need);
    }
}

static
void
translate_finally_block(CFG *cfg, int finally_block_index, Method *method)
{
    uint8 opcode;

    FinallyBlockInfo *fb_info;

    fb_info = CFG_GetFinallyBlockInfo(cfg, finally_block_index);

    // This will be set when translating 'ret'.
    last_ret_translate_instr = NULL; 

    //
    // get 'operand_stack_top'
    //
    ops_top = FBI_get_operand_stack_top_at_start(fb_info);

    //
    // create the finally block prologue code
    //
    last_instr = CSeq_create_finally_prologue_code(cfg, fb_info, ops_top);

    set_ops_type(ops_top + 1, T_REF);
    ops_top++;

    traverse_stack_top++;
    save_context(&saved_contexts[traverse_stack_top], ops_top, ops_types);
    parent_nodes[traverse_stack_top] = last_instr;
    traverse_stack[traverse_stack_top] = FBI_get_first_pc(fb_info);

    while(traverse_stack_top >= 0) {
        assert(traverse_stack_top < MAX_STACK_SIZE);

	restore_context(&saved_contexts[traverse_stack_top],
			&ops_top, ops_types);
	
        last_instr = parent_nodes[traverse_stack_top]; 
        next_pc = traverse_stack[traverse_stack_top--];

        do {
            if (translated_instrs[next_pc] != NULL) {
                Instr_connect_instruction(last_instr,
					  translated_instrs[next_pc]);
                break;
            }

            opcode = BCode_get_uint8(Method_GetByteCode(method) + next_pc);

            //
            // now the code traslation is really done here.
            // The successors are push onto stack in here and
            // some kind of verification is done here.
            //
            // 'next_pc' and 'last_instr' are static global variables.
            // Their values are updated while translation.
            //
            translate_bytecode(opcode, cfg, method, next_pc, last_instr);
        } while(!context_load_need);
    }

    // last_ret_translate_instr can be NULL
    FBI_set_last_FB_instruction(fb_info, last_ret_translate_instr);
}



/* Name        : translate_one_method
   Description : traverse a method and translate it
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static
void
translate_one_method(CFG *cfg, InlineGraph *graph, TranslationInfo *info) 
{
    int ops_size;
    int code_len;
    int i;
    Method *method;

    method = IG_GetMethod(graph);
    code_len = Method_GetByteCodeLen(method);

    Var_prepare_creation(graph);


    /* for inlined exception handling */
    if (info != NULL && TI_IsFromException(info) == true) {
        ret_addr = (void *) TI_GetReturnAddr(info);
        ret_stack_top = TI_GetReturnStackTop(info);
    } else {
        ret_addr = 0;
        ret_stack_top = 0;
    }

    wide = false;

    translated_instrs = (InstrNode **) alloca(sizeof(InstrNode *) * code_len);
    bzero(translated_instrs, sizeof(InstrNode *) * code_len);

    traverse_stack_top = -1;
    ops_top = -1;
    ops_size = Method_GetStackSize(method);

    if (ops_size > 0) {
        ops_types = (int *) alloca(sizeof(int) * ops_size);
    } else {
        ops_types = NULL;
    }

    CFG_InitFinallyBlocks(cfg);

    // translate_main_flow
    translate_main_flow(cfg, info, graph);

    // translate finally block
    for(i = 0; i < cfg->numOfFinallyBlocks; i++) {
	bzero(translated_instrs, sizeof(InstrNode *) * code_len);
        translate_finally_block(cfg, i, method);
        CFG_ConnectFinallyBlock(cfg, i);
    }
}

inline static
void
connect_translated_method(InlineGraph *inline_graph) {
    InstrNode *caller_start;
    InstrNode *caller_end;
    InstrNode *callee_start;
    InstrNode *callee_end;

    caller_start = IG_GetInstr(inline_graph);
    caller_end = Instr_GetSucc(caller_start, 0);

    assert(Instr_GetNumOfSuccs(caller_start) == 1);

    Instr_disconnect_instruction(caller_start, caller_end);

    callee_start = IG_GetHead(inline_graph);
    callee_end = IG_GetTail(inline_graph);

    Instr_connect_instruction(caller_start, callee_start);
    Instr_connect_instruction(callee_end, caller_end);
}

static
void
determine_compilation_flags(Method* meth, TranslationInfo *info)
{
    bool from_exception;
    int tr_level;
#ifdef CUSTOMIZATION
    tr_level = TI_GetSM(info) ? TI_GetSM(info)->tr_level : 0;
#else
    tr_level = meth->tr_level;
#endif;
    // if (translator is called from exception)
    //   all optimizations are determined by user-defined options, but,
    //   method inlining is disabled.
    // else if (adaptive compilation turned off 
    //         || this method is retranslated 
    //         || the method has no unresolved call site)
    //   all optimizations including method inlining are determined by
    //   user-defined options
    // else -> initial translation
    //   all optimizations included method inlining are disabled.
    from_exception = TI_IsFromException(info);

    if (from_exception) {
        The_Final_Translation_Flag = true;
        flag_no_regionopt = !The_Use_Regionopt_Option;
        flag_no_loopopt = !The_Use_Loopopt_Option;
#ifdef HANDLER_INLINE
        flag_inlining = The_Use_Method_Inline_Option;
        flag_type_analysis = The_Use_TypeAnalysis_Flag;
#else /* not HANDLER_INLINE */        
        flag_type_analysis = false;
        flag_inlining = false;
#endif /* not HANDLER_INLINE */        
    } else if (!flag_adapt
               || (tr_level > 0)
               || !Method_HaveUnresolved(meth)) {
        The_Final_Translation_Flag = true;
        flag_no_regionopt = !The_Use_Regionopt_Option;
        flag_inlining = The_Use_Method_Inline_Option;
        flag_no_loopopt = !The_Use_Loopopt_Option;
        
        // If the method has only resolved call sites or
        // is compiled to general version, then
        // don't perform type analysis.
        // -> we may lose some opportunities because there may be new
        //    instruction and the object can be propagated.
        flag_type_analysis = The_Use_TypeAnalysis_Flag;
    } else { // initial translation
        The_Final_Translation_Flag = false;
        flag_no_regionopt = true;
        flag_inlining = false;
        flag_no_loopopt = true;
        flag_type_analysis = false;
    }

    The_Need_Retranslation_Flag = !The_Final_Translation_Flag;
}

/* Name        : translate_methods
   Description : translate a method.
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: This function uses recursion.  */
static
void
translate_methods(CFG *cfg, InlineGraph *graph, TranslationInfo *info)
{
    InlineGraph *callee;
    Method *method;
    int localsz;
    int startPC;

    /* set translation start point 
       normally it is 0, but in exception handler it is not 0 */
    if (info != NULL) 
      startPC = TI_GetStartBytecodePC(info);
    else 
      startPC = 0;

    method = IG_GetMethod(graph);
    CFGGen_current_ig_node = graph;
    localsz = Method_GetLocalSize(method);

    /* analyse bytecodes */
    BA_bytecode_analysis(method);

    if (IG_GetDepth(graph) == 0) {
        // Optimization options are determined 
        // after outmost method is analyzed.
        determine_compilation_flags(method, info);
    }
    // callSiteInfoTable in SpecializedMethod has actual meaning.
    // But, callSiteInfoTable in method is used temporarily.
    // And it is set here. -walker
    // if (this method is the outmost method
    //     && not from exception
    //     && not final translation)
    //   allocate callSiteInfoTable;
    // else
    //   method->callSiteInfoTable = get_arbitrary_CSIT();
#ifdef INLINE_CACHE
#ifdef CUSTOMIZATION
    if (IG_GetDepth(graph) == 0 
        && !TI_IsFromException(info)
        && !The_Final_Translation_Flag) {
        method->callSiteInfoTable =
            TI_GetSM(info)->callSiteInfoTable = 
            (CallSiteInfo*)gc_malloc_fixed(sizeof(CallSiteInfo)*Method_GetByteCodeLen(method));
    } else {
        method->callSiteInfoTable = CSI_get_arbitrary_CSIT(method);
    }
#else 
    if (IG_GetDepth(graph) == 0 
        && !TI_IsFromException(info)
        && !The_Final_Translation_Flag) {
        method->callSiteInfoTable =
            (CallSiteInfo*)gc_malloc_fixed(sizeof(CallSiteInfo)*Method_GetByteCodeLen(method));
    }
#endif
#endif /* INLINE_CACHE */

#ifdef TYPE_ANALYSIS
    // FIXME: This value is used for CFG_PrepareTraversalFromStart.
    //        Without this we must unmark all the node in the CFG.
    //        But I think there must be a more elegant way.
    IG_SetStartInstrNum(graph, CFG_GetSizeOfList(cfg));

    /* allocate TA_Num_Of_Defs 
       note that this variable is defined in "TypeAnalysis.c" */
    if (localsz > 0) {
        int i;
        int arg_size = Method_IsStatic(method) ? Method_GetArgSize(method) :\
            Method_GetArgSize(method)+1;
        TA_Num_Of_Defs = FMA_calloc(sizeof(int) * localsz);
        for (i = 0; i < arg_size; i++) {
            TA_Num_Of_Defs[i] = 1;
        }
    } else {
        TA_Num_Of_Defs = NULL;
    }
#endif /* TYPE_ANALYSIS */
    translate_one_method(cfg, graph, info);

#ifdef TYPE_ANALYSIS
    /* perform type analysis */
    if (flag_type_analysis) {
        TA_type_analysis(graph);
    }
#endif /* TYPE_ANALYSIS */
    
    /* monitorenter code generation is delayed due to type analysis */
    if (Method_IsSynchronized(method)) {
        CSeq_insert_monitorenter_code(cfg, graph);
    }

    if (IG_GetDepth(graph) != 0) {
        connect_translated_method(graph);
    }

    for(callee = IG_GetCallee(graph); 
        callee != NULL;
        callee = IG_GetNextCallee(callee)) {
        
        translate_methods(cfg, callee, NULL);
        
    }
}

/* Name        : CFGGen_generate_CFG
   Description : CFG generation driver function
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
CFG *
CFGGen_generate_CFG(TranslationInfo *info)
{
    Method *method = TI_GetRootMethod(info);
    InlineGraph *graph = IG_init(method);
    extern CFG *cfg;
    cfg = CFG_Init(graph);

    /* set root method's variable offset */
    IG_SetLocalOffset(graph, TI_GetCallerLocalNO(info));
    IG_SetStackOffset(graph, TI_GetCallerStackNO(info));
    
    /* set ArgSet field in InlineGraph for type analysis */
#ifdef CUSTOMIZATION
    IG_SetArgSet(graph, SM_MakeArgSetFromSM(TI_GetSM(info)));
#endif
    translate_methods(cfg, graph, info);

    return cfg;
}
