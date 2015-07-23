/* code_sequences.c
   
   Generating some assembly code sequences during cfg generation
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#include "classMethod.h"
#include "SPARC_instr.h"
#include "CFG.h"
#include "InstrNode.h"
#include "cfg_generator.h"
#include "pseudo_reg.h"
#include "SpecializedMethod.h"
#include "retranslate.h"
#include "method_inlining.h"
#include "locks.h"
#include "class_inclusion_test.h"
#include "translator_driver.h"
#include "stat.h"
#include "functions.h"
#include "bytecode.h"
#ifdef TYPE_ANALYSIS
#include "TypeAnalysis.h"
#endif

#ifdef BYTECODE_PROFILE
#include "bytecode_profile.h"
#endif

#undef INLINE
#define INLINE
#include "ArgSet.ic"


#define LDBG(s)
#define YDBG(s)

/* miscellaneous debugging functions */
static
void
print_bytecode( Method* method, InstrOffset pc )
{
#ifdef BYTECODE_PROFILE
	Pbc_increase_frequency(method, pc);
#endif

YDBG(
	num_of_executed_bytecode_instrs++;
)
}

InstrNode *
CSeq_insert_debug_code(CFG *cfg, InstrNode *p_instr, Method *method, int pc)
{
    InstrNode *c_instr = p_instr;
    int arg_vars[2];

    //
    // sethi %hi(method), it7
    // or    it7, %lo(method), rt8
    // add   g0, pc, it9
    // call  print_bytecode (o0: rt8, o1: it9, ret: void)
    //
    arg_vars[0] = RT(8);
    arg_vars[1] = IT(9);

    APPEND_INSTR2(SETHI, IT(7), HI(method));
    APPEND_INSTR5(OR, RT(8), IT(7), LO(method));
    Instr_SetLastUseOfSrc(c_instr, 0);

    if (pc <= 0xfff) {
	APPEND_INSTR5(ADD, IT(9), g0, pc);
    } else {
	/* use "sethi" & "or" for large pc value */
        APPEND_INSTR2(SETHI, IT(9), HI(pc));
        APPEND_INSTR5(OR, IT(9), IT(9), LO(pc));
        Instr_SetLastUseOfSrc(c_instr, 0);
    }

    APPEND_INSTR1(CALL, (int) print_bytecode);
    CFGGen_process_for_function_call(cfg, c_instr, print_bytecode, 
				     2, arg_vars, 0, NULL);

    return c_instr;
}

void 
print_monitorenter(Hjava_lang_Object *obj)
{
    printf("monitorenter for %p, lock_info = %x\n", obj, obj->lock_info);
}

void 
print_monitorexit(Hjava_lang_Object *obj)
{
    printf("monitorexit for %p, lock_info = %x\n", obj, obj->lock_info);
}

inline static
InstrNode *
debug_monitorenter(CFG *cfg, InstrNode *p_instr, int ref_reg)
{
    InstrNode *c_instr = p_instr;
    int arg_vars[1];
    int pc = -1;

    arg_vars[0] = IT(0);

    APPEND_INSTR6(ADD, arg_vars[0], ref_reg, g0);
    APPEND_INSTR1(CALL, (int) print_monitorenter);
    CFGGen_process_for_function_call(cfg, c_instr, (void *) print_monitorenter,
                                     1, arg_vars, 0, NULL);

    return c_instr;
}

inline static
InstrNode *
debug_monitorexit(CFG *cfg, InstrNode *p_instr, int ref_reg)
{
    //
    //  add  ref_reg, g0, it0
    //  call print_monitorexit
    //
    InstrNode *c_instr = p_instr;
    int arg_vars[1];
    int pc = -1;

    arg_vars[0] = IT(0);

    APPEND_INSTR6(ADD, IT(0), ref_reg, g0);
    APPEND_INSTR1(CALL, (int) print_monitorexit);
    CFGGen_process_for_function_call(cfg, c_instr, (void *) print_monitorexit,
				     1, arg_vars, 0, NULL);

    return c_instr;
}

void
print_method(Method *method) {
    assert(method != NULL);

    fprintf(stderr, "%s %s %s method\n", method->class->name->data,
	    method->name->data, method->signature->data);
}

/* Name        : CSeq_create_monitorenter_code
   Description : `monitorenter' code sequence is generated.
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
InstrNode *
CSeq_create_monitorenter_code(CFG *cfg, InstrNode *p_instr, int pc,
			      InstrNode **first_instr, int ref_reg)
{
    //
    //        sethi %hi(&currentThreadID), it1
    //        lduh  [it1+%lo(&currentThreadID)], it6
    //        add   ref_reg, g0, rt7
    // START:
    //        lduh  [ref_reg+4], it0
    //        subcc it0, 1, g0		// obj->semaphore == 0 ?
    //                                	// obj->has_waiters can be set.
    //        bgu   SECOND_PROBABLE_CASE
    // MOST_PROBABLE_CASE:
    //        add   it0, 2, it3
    //        sll   it3, 16, it4
    //        or    it4, it6, it5	// 0x20000(0x30000) | currentThreadID
    //        st    it5, [ref_reg+4]	// obj->lock_info = 
    //					//	(1, no_waiter|waiter, 
    //					//	 currentThreadID)
    //        goto  EXIT
    // SECOND_PROBABLE_CASE:
    //        lduh  [ref_reg+6], it3
    //        subcc it3, it6, g0	// obj->owner == currentThreadID ?
    //        bne   LEAST_PROBABLE_CASE
    //        add   it0, 2, it3
    //        sth   it3, [ref_reg+4]	// obj->semaphore++
    //        goto  EXIT
    // LEAST_PROBABLE_CASE:
    //        add   it0, g0, g0		// it0 is now dead.
    //        call  wait_in_queue (o0: ref_reg)
    //        add   rt7, g0, ref_reg
    //        goto  START
    // EXIT:
    //        add   it6, g0, g0
    //        add   rt7, g0, g0
    extern unsigned short     currentThreadID;

    InstrNode *c_instr = p_instr;
    InstrNode *start_instr;
    InstrNode *branch_instr1;
    InstrNode *branch_instr2;
    InstrNode *join_instr;
    InstrNode *end_instr;

    int arg_vars[1];

    APPEND_INSTR2(SETHI, IT(1), HI(&currentThreadID));
    if (first_instr != NULL) *first_instr = c_instr;
    APPEND_INSTR5(LDUH, IT(6), IT(1), LO(&currentThreadID));
    Instr_SetLastUseOfSrc(c_instr, 0);

    /* insert debugging code */
    LDBG(c_instr = debug_monitorenter(cfg, c_instr, ref_reg);)

    // START:
    APPEND_INSTR5(LDUH, IT(0), ref_reg, 4);
    start_instr = c_instr;
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);


    APPEND_INSTR5(SUBCC, g0, IT(0), 1);
    APPEND_INSTR3(BGU);
    branch_instr1 = c_instr;
    CFGGen_process_for_branch(cfg, branch_instr1);


    // MOST_PROBABLE_CASE:
    APPEND_INSTR5(ADD, IT(3), IT(0), 2);
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR5(SLL, IT(4), IT(3), 16);
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR6(OR, IT(5), IT(4), IT(6));
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR8(ST, IT(5), ref_reg, 4);
    Instr_SetLastUseOfSrc(c_instr, 0);
    Instr_SetLastUseOfSrc(c_instr, 1);

    // EXIT:
    APPEND_INSTR6(ADD, g0, IT(6), g0);
    join_instr = end_instr = c_instr;
    Instr_SetLastUseOfSrc(end_instr, 0);

    // SECOND_PROBABLE_CASE:
    c_instr = branch_instr1;
    APPEND_INSTR5(LDUH, IT(3), ref_reg, 6);


    APPEND_INSTR6(SUBCC, g0, IT(3), IT(6));
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR3(BNE);
    branch_instr2 = c_instr;
    CFGGen_process_for_branch(cfg, branch_instr2);


    APPEND_INSTR5(ADD, IT(3), IT(0), 2);
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR8(STH, IT(3), ref_reg, 4);
    Instr_SetLastUseOfSrc(c_instr, 0);
    Instr_SetLastUseOfSrc(c_instr, 1);

    Instr_connect_instruction(c_instr, join_instr);

    // LEAST_PROBABLE_CASE:
    c_instr = branch_instr2;
    APPEND_INSTR6(ADD, g0, IT(0), g0);
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR6(ADD, RT(7), ref_reg, g0);
    APPEND_INSTR1(CALL, (int) wait_in_queue);

    arg_vars[0] = RT(7);

    CFGGen_process_for_function_call(cfg, c_instr, (void*) wait_in_queue,
                                     1, arg_vars, 0, NULL);

    Instr_connect_instruction(c_instr, start_instr);

    return end_instr;
}

/* Name        : CSeq_insert_monitorenter_code
   Description : insert monitorenter code after root instr. of the InlineGraph
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
CSeq_insert_monitorenter_code(CFG* cfg, InlineGraph* graph)
{
    InstrNode *c_instr, *succ;
    Method* method = IG_GetMethod(graph);
    int ref_reg = RT(8);
    int pc = -1;

    c_instr = IG_GetHead(graph);
    succ = Instr_GetSucc(c_instr, 0);

    /* monitorenter code should be inserted after argument passing code
       for inlined methods */
    if (IG_GetDepth(graph) != 0)
        for(; Instr_GetBytecodePC(succ) == -7;
            c_instr = succ, succ = Instr_GetSucc(c_instr, 0))
          assert(Instr_GetNumOfSuccs(c_instr) == 1);

    assert(Instr_GetNumOfPreds(succ) == 1);
    Instr_disconnect_instruction(c_instr, succ);

    if (Method_IsStatic(method)) {
        Hjava_lang_Class* class = Method_GetDefiningClass(method);
        APPEND_INSTR2(SETHI, RT(0), HI(class));
        APPEND_INSTR5(OR, ref_reg, RT(0), LO(class));
        Instr_SetLastUseOfSrc(c_instr, 0);
    } else {
        APPEND_INSTR6(ADD, ref_reg, RL(0), g0);
    }

    c_instr = CSeq_create_monitorenter_code(cfg, c_instr, pc, NULL, ref_reg);

    Instr_connect_instruction(c_instr, succ);
}

/* Name        : CSeq_create_monitorexit_code
   Description : `monitorexit' code sequence is generated
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
InstrNode *
CSeq_create_monitorexit_code(CFG *cfg, InstrNode *p_instr, int pc,
			     InstrNode **first_instr, int ref_reg)
{
    //
    //    lduh   [ref_reg+4], it0
    //    sub    it0, 2, it1
    //    sth    it1, [ref_reg+4]  <--- obj->semaphore--
    //    subcc  it1, 1, g0   <--- (obj->semaphore != 0 || !obj->has_waiters) ?
    //    be     LESS_PROBABLE_CASE
    // MORE_PROBABLE_CASE:
    //    add    ref_reg, g0, g0 <--- ref_reg is now dead.
    //    goto   EXIT
    // LESS_PROBABLE_CASE:
    //    call   wakeup_waiter (o0:ref_reg)
    //    goto   EXIT
    // EXIT:
    //    add    g0, g0, g0
    //
    //=========================>
    // When a monitorexit is performed on a unlocked object,
    // "IllegalMonitorStateException" must be thrown.
    // For this, a check must be inserted at the first block.
    //    lduh   [ref_reg+4], it0
    //    subcc  it0, 2, it1
    //    bcs    THROW_EXCEPTION
    //    sth    it1, [ref_reg+4]  <--- obj->semaphore--
    //    subcc  it1, 1, g0   <--- (obj->semaphore != 0 || !obj->has_waiters) ?
    //    be     LESS_PROBABLE_CASE
    //
    //  THROW_EXCEPTION
    //    ta     0x15
    //
    
    InstrNode *c_instr = p_instr;
    InstrNode *branch_instr;
    InstrNode *end_instr;

#ifdef PRECISE_MONITOREXIT
    InstrNode *check_branch;
#endif // PRECISE_MONITOREXIT

    int           arg_vars[1];

    APPEND_INSTR5(LDUH, IT(0), ref_reg, 4);
    if (first_instr != NULL) *first_instr = c_instr;
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);

#ifdef PRECISE_MONITOREXIT
    APPEND_INSTR5(SUBCC, IT(1), IT(0), 2);
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR3(BCS);
    check_branch = c_instr;
    CFGGen_process_for_branch(cfg, check_branch);

#else
    APPEND_INSTR5(
	SUB, IT(1), IT(0), 2);
    Instr_SetLastUseOfSrc(c_instr, 0);
#endif // PRECISE_MONITOREXIT

    APPEND_INSTR8(STH, IT(1), ref_reg, 4);

    LDBG(c_instr = debug_monitorexit(cfg, c_instr, ref_reg););


    APPEND_INSTR5(SUBCC, g0, IT(1), 1);
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR3(BE);
    branch_instr = c_instr;
    CFGGen_process_for_branch(cfg, branch_instr);


    //
    // MORE_PROBABLE_CASE:
    //
    APPEND_INSTR6(ADD, g0, ref_reg, g0);
    Instr_SetLastUseOfSrc(c_instr, 0);

    //
    // EXIT:
    //
    APPEND_NOP();
    end_instr = c_instr;

    //
    // LESS_PROBABLE_CASE:
    //
    c_instr = branch_instr;
    APPEND_INSTR1(CALL, (int) wakeup_waiter);

    arg_vars[0] = ref_reg;
    CFGGen_process_for_function_call(cfg, c_instr, (void *) wakeup_waiter,
				     1, arg_vars, 0, NULL);

    Instr_connect_instruction(c_instr, end_instr);

#ifdef PRECISE_MONITOREXIT
    //
    // THROW_EXCEPTION:
    //
    c_instr = register_format9_instruction(cfg, check_branch, pc,
                                           TA, 0x15);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);
#endif // PRECISE_MONITOREXIT

    return end_instr;
}





static unsigned FSR_ON = 0x08000000;
static unsigned FSR_OFF = 0;

/* Name        : CSeq_create_on_FSR_invalid_trap_instruction
   Description : To fully support conversion from floating point
                 number to integer number, FSR_invalid trap must be
                 set before conversion instruction
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: fsr register value is loaded from FSR_ON  */
InstrNode * 
CSeq_create_on_FSR_invalid_trap_instruction(CFG *cfg, InstrNode *p_instr, 
					    int pc, InstrNode **first_instr)
{
    //
    //  sethi  rt0, %hi(&FSR_ON) 
    //  ld     [rt0 + %lo(&FSR_ON)], %fsr
    //
    InstrNode *c_instr = p_instr;

    
    APPEND_INSTR2(SETHI, RT(0), HI(&FSR_ON));
    if (first_instr != NULL) *first_instr = c_instr;
    APPEND_INSTR5(LDFSR, g0, RT(0), LO(&FSR_ON));
    Instr_SetLastUseOfSrc(c_instr, 0);

    return c_instr;
}

/* Name        : CSeq_create_off_FSR_invalid_trap_instruction
   Description : To fully support conversion from floating point
                 number to integer number, FSR_invalid trap must be
                 set before conversion instruction
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: fsr register value is loaded from FSR_OFF  */
InstrNode * 
CSeq_create_off_FSR_invalid_trap_instruction(CFG *cfg, InstrNode *p_instr, 
					     int pc, InstrNode **first_instr)
{
    //
    // sethi  rt0, %hi(&FSR_OFF)
    // ld     [rt0 + %lo(&FSR_OFF)], %fsr
    //
    InstrNode *c_instr = p_instr;

    APPEND_INSTR2(SETHI, RT(0), HI(&FSR_OFF));
    if (first_instr != NULL) *first_instr = c_instr;
    APPEND_INSTR5(LDFSR, g0, RT(0), LO(&FSR_OFF));
    Instr_SetLastUseOfSrc(c_instr, 0);

    return c_instr;
}

/* Name        : CSeq_create_function_prologue_code
   Description : creating function prologue codes including argument
                 passing codes
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: monitor code and counting code is inserted later  */
InstrNode *
CSeq_create_function_prologue_code(CFG *cfg, InlineGraph *graph)
{
    //
    // save sp, 0, sp
    //
    InstrNode *c_instr;
    Method *method = IG_GetMethod(graph);
    int *param_vars = NULL;
    int param_no, param_idx;
    char *signature;
    char *str;
    int pc = -3;

    /* first instrudction */
    c_instr = create_format5_instruction(SPARC_SAVE, SP, SP, 0, -3);
    CFG_SetRoot(cfg, c_instr);
    CFG_RegisterInstr(cfg, c_instr);

    /* for exception handling */
    IG_SetHead(graph, c_instr);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);

    /* for register counting code
       There is an assumption that the successor of root instruction
       has only one predecessor */
    APPEND_NOP();

    param_no = Method_IsStatic(method)
	? Method_GetArgSize(method) : Method_GetArgSize(method) + 1;

    if (param_no > 0)
	param_vars = FMA_calloc(param_no * sizeof(int));

    param_idx = 0;

    if (!Method_IsStatic(method)) {
	param_vars[0] = RL(0);
	param_idx++;
    }

    signature = Method_GetSignature(method)->data;
    
    assert(signature[0] == '(');

    for (str = signature + 1; *str != ')'; str++) {
	int array_depth = 0;

        switch (*str) {
          case 'I':
          case 'Z':
          case 'S':
          case 'B':
          case 'C':
	    /* integer type argument */
	    param_vars[param_idx] = IL(param_idx);
	    param_idx++;
            break;

          case 'F':
	    /* floating point type argument */
	    param_vars[param_idx] = FL(param_idx);
	    param_idx++;
            break;

          case 'J':
	    /* long type argument */
	    param_vars[param_idx] = LL(param_idx);
	    param_vars[param_idx + 1] = IVL(param_idx + 1);
	    param_idx += 2;
            break;

          case 'D':
	    /* double type argument */
	    param_vars[param_idx] = DL(param_idx);
	    param_vars[param_idx + 1] = FVL(param_idx + 1);
	    param_idx += 2;
            break;

          case 'L':
	    /* reference type */
	    param_vars[param_idx] = RL(param_idx);
	    param_idx++;

	    /* skip redundant information */
            while(*str != ';') str++;
            break;

          case '[':
	    /* array reference type argument */
	    param_vars[param_idx] = RL(param_idx);
	    param_idx++;

	    /* skip redundant information */
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

    assert(param_no == param_idx);
    
    cfg->numOfParams = param_no;
    cfg->paramVars = param_vars;

    assert( *str == ')' );
    switch (*++str) {
      case 'V':
        cfg->retType = RT_NO_RETURN;
        break;

      case 'I':  case 'Z':  case 'S':
      case 'B':  case 'C':
        cfg->retType = RT_INTEGER;
        break;

      case 'J':
        cfg->retType = RT_LONG;
        break;

      case 'F':
        cfg->retType = RT_FLOAT;
        break;

      case 'D':
        cfg->retType = RT_DOUBLE;
        break;

      case '[':  case 'L':
        cfg->retType = RT_REFERENCE;
        break;

      default:
        assert( false && "Strange Return Type!" );
    }

    if (flag_called_method) {
	//
	// This is for debugging.
	//
	// sethi   %hi(method), rt0
	// or      rt0, %lo(method), rt0
	// call    print_method (o0: rt0, ret: void)
	//
	int arg_vars[1];
	int pc = -1;

	arg_vars[0] = RT(0);

	APPEND_INSTR2(SETHI, RT(0), HI(method));
	APPEND_INSTR5(OR, RT(0), RT(0), LO(method));
	Instr_SetLastUseOfSrc(c_instr, 0);
	APPEND_INSTR1(CALL, (int) print_method);
	CFGGen_process_for_function_call(cfg, c_instr, print_method, 
                                         1, arg_vars, 0, NULL);
    }

    return c_instr;
}
/* Name        : CSeq_create_exception_handler_prologue_code
   Description : create exception handler prologue codes
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
InstrNode *
CSeq_create_exception_handler_prologue_code(CFG *cfg, InlineGraph *graph)
{
    //
    // save  sp, 0, sp		// dummy instruction
    // add   g2, g0, rs0
    //
    InstrNode *c_instr = NULL;
    int pc = -1;

    // This is a dummy root for exception handler
    c_instr = create_format1_instruction(DUMMY_OP, 0, -3);
    CFG_SetRoot(cfg, c_instr);
    CFG_RegisterInstr(cfg, c_instr);

    /* for exception handling */
    IG_SetHead(graph, c_instr);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);

    APPEND_INSTR6(ADD, RS(0), g2, g0);

    return c_instr;
}

/* Name        : CSeq_create_inline_prologue_code
   Description : create inlining prologue codes
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
InstrNode *
CSeq_create_inline_prologue_code(CFG *cfg, InlineGraph *graph) 
{
    //
    // nop
    // ... method body ...
    // nop
    //
    InstrNode *c_instr, *e_instr;
    Method *method = IG_GetMethod(graph);
    short used_words = 0;
    char *signature = Method_GetSignature(method)->data;
    char *str;
    int stack_offset;
    int pc = -7;

    // set first and last instructions
    c_instr = create_format6_instruction(ADD, g0, g0, g0, -7);
    CFG_RegisterInstr(cfg, c_instr);

    IG_SetHead(graph, c_instr);

    /* for monitorenter code
       There is an assumption that the successor of root instruction
       has only one predecessor */
    APPEND_NOP();

    e_instr = create_format6_instruction(ADD, g0, g0, g0, -9);
    CFG_RegisterInstr(cfg, e_instr);

    IG_SetTail(graph, e_instr);

    CFG_MarkExceptionGeneratableInstr(cfg, e_instr);


    // copy parameters into local variables

    // offset to access caller's stack
    stack_offset = IG_GetStackTop(graph) - Var_offset_of_stack_var;


    /* If the method is not static, the first parameter of the method
       is the implicit 'this' reference. */
    if (!Method_IsStatic(method)) {
        //
        // The first parameter variable is `rl0'.
        //
	APPEND_INSTR6(ADD, RL(used_words), RS(stack_offset + used_words), g0);
        Instr_SetLastUseOfSrc(c_instr, 0);

	used_words++;
    }

    /* The form of method signature is as follows:
       (ParameterDescriptor*)ReturnDescriptor */
    
    assert(signature[0] == '(');

    for (str = signature + 1; *str != ')'; str++) {
	int caller_idx = stack_offset + used_words;
	int array_depth = 0;

        switch (*str) {
          case 'I':
          case 'Z':
          case 'S':
          case 'B':
          case 'C':
	    /* integer type argument */
	    APPEND_INSTR6(ADD, IL(used_words), IS(caller_idx), g0);
            Instr_SetLastUseOfSrc(c_instr, 0);

            used_words++;
            break;

          case 'F':
	    /* floating point type argument */
	    APPEND_INSTR6(FMOVS, FL(used_words), g0, FS(caller_idx));
            Instr_SetLastUseOfSrc(c_instr, 0);

            used_words++;
            break;

          case 'J':
	    /* long type argument */
	    APPEND_INSTR6(ADD, LL(used_words), LS(caller_idx), g0);
            Instr_SetLastUseOfSrc(c_instr, 0);
	    APPEND_INSTR6(ADD, IVL(used_words + 1), IVS(caller_idx + 1), g0);
            Instr_SetLastUseOfSrc(c_instr, 0);

            used_words += 2;
            break;

          case 'D':
	    /* double type argument */
	    APPEND_INSTR6(FMOVD, DL(used_words), g0, DS(caller_idx));
            Instr_SetLastUseOfSrc(c_instr, 0);

            used_words += 2;
            break;

          case 'L':
	    /* reference type */
	    APPEND_INSTR6(ADD, RL(used_words), RS(caller_idx), g0);
            Instr_SetLastUseOfSrc(c_instr, 0);

            used_words++;

	    /* skip redundant information */
            while(*str != ';') str++;
            break;

          case '[':
	    /* array reference type argument */
	    APPEND_INSTR6(ADD, RL(used_words), RS(caller_idx), g0);
            Instr_SetLastUseOfSrc(c_instr, 0);

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
    // 'used_words' represents the number of input parameter.
    //
    return c_instr;
}

/* Name        : CSeq_create_finally_prologue_code
   Description : create finally block prologue codes
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
InstrNode *
CSeq_create_finally_prologue_code(CFG *cfg, FinallyBlockInfo *fbinfo,
				  int ops_top)
{
    InstrNode *c_instr;
    int pc = -1;
    
    c_instr = create_format6_instruction(ADD, g0, g0, g0, -3);

    FBI_SetHeader(fbinfo, c_instr);
    CFG_RegisterInstr(cfg, c_instr);

    //
    //  add ORET, g0, rs{top+1}
    //

    APPEND_INSTR6(ADD, RS(ops_top + 1), g0, ORET);
    Instr_SetLastUseOfSrc(c_instr, 0);

    return c_instr;
}

/* Name        : CSeq_create_function_epilogue_code
   Description : create function epilogue codes
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: create monitorexit code if necessary.  */
InstrNode *
CSeq_create_function_epilogue_code(CFG *cfg, InstrNode *p_instr, int pc, 
				   InstrNode **first_instr,
				   Method *method)
{
    InstrNode *c_instr = p_instr;

    APPEND_NOP();
    if (first_instr != NULL) *first_instr = c_instr;

    /* create monitorexit code if necessary */
    if (Method_IsSynchronized(method)) {
	/* ref_reg is an object to lock on */
	int ref_reg = RT(6);

	if (Method_IsStatic(method)) {
	    Hjava_lang_Class *class = Method_GetDefiningClass(method);

	    APPEND_INSTR2(SETHI, RT(0), HI(class));
	    APPEND_INSTR5(OR, ref_reg, RT(0), LO(class));
	    Instr_SetLastUseOfSrc(c_instr, 0);
	} else {
	    APPEND_INSTR6(ADD, ref_reg, RL(0), g0);
	}

	c_instr = CSeq_create_monitorexit_code(cfg, c_instr, pc, NULL,
					       ref_reg);
    }
    
    return c_instr;
}

/* Name        : CSeq_create_null_pointer_checking_code
   Description : create runtime null pointer checking code
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
InstrNode *
CSeq_create_null_pointer_checking_code(CFG *cfg, InstrNode *p_instr,
				       int pc, InstrNode **first_instr, 
				       int operand_stack_index)
{
    //
    //         subcc  AS{index}, 0, g0
    //         bne    NORMAL
    //      EXCEPTION:
    //    <= execute_java_constructor(0, "java.lang.NullPointerException", 
    //                                0, "()V")
    //         mov    0, IP0
    //         sethi  %hi(Null_Pointer_Exception), AT0
    //         or     AT0, %lo(Null_Pointer_Exception) , AP1
    //         mov    0, IP2
    //         sethi  %hi(Default_Signature), AT1
    //         or     AT1, %lo(Default_Signature), AP3
    //         call   execute_java_constructor
    //      NORMAL:
    //===================================================================
    //    Now, the exception is generated via software trap.
    //    The trap number 0x10 is to generate SIGILL signal.
    //         subcc  AS{index}, 0, g0
    //         te     0x10
    //      NORMAL:
    // ===============================>
    //    In UltraSPARC, subcc and trap cannot be grouped
    //    in one cycle. But subcc and branch can be grouped.
    //         subcc  AS{index}, 0, g0
    //         be     EXCEPTION
    //         nop
    //         ...
    //       EXCEPTION:
    //         ta   0x10
    //==================================================================
    //                  VLIW ISA
    //==================================================================
    //         se    AS{index}, 0, cc
    //         tset  cc, 0x10
    //      NORMAL:
    //
    InstrNode *c_instr = p_instr;
#ifndef NEW_NULL_CHECK
    InstrNode *branch_instr;
#endif


#ifdef NEW_NULL_CHECK

    APPEND_INSTR5(LD, g0, RS(operand_stack_index), 0);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);
    if (first_instr != NULL) *first_instr = c_instr;

#else /* not NEW_NULL_CHECK */

    APPEND_INSTR5(SUBCC, g0, RS(operand_stack_index), 0);
    if (first_instr != NULL) *first_instr = c_instr;
    APPEND_INSTR3(BNE);
    CFGGen_process_for_branch(cfg, c_instr);
    branch_instr = c_instr;
    Instr_PredictTaken(branch_instr);

    // EXCEPTION:
    APPEND_INSTR9(TA, 0x10);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);

    // NORMAL:
    c_instr = branch_instr;
    APPEND_NOP();

#endif /* not NEW_NULL_CHECK */


    return c_instr;
}

/* Name        : CSeq_create_array_bound_checking_code
   Description : Make code sequences that checks array bounds.
                 This is based on branch & trap instruction.
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
       Type at array_ref_index should be reference.
       Type at array_index_index should be integer.
   
   Post-condition:
   
   Notes: If first_instr is not null, first_instr is set to first
          instruction of checking code */
InstrNode *
CSeq_create_array_bound_checking_code(CFG *cfg, InstrNode *p_instr,
				      int pc, InstrNode **first_instr,
				      int array_ref_index, 
				      int array_index_index)
{ 
    //
    //     ld 	[rs{array_ref_index} + ARRAY_SIZE_OFFSET], it0
    //     subcc 	it0, is{array_index_index}, g0
    //     bcc NORMAL
    // EXCEPTION:
    //     tleu 	0x11
    // NORMAL:  
    //
    InstrNode *c_instr = p_instr;
#ifndef USE_TRAP_FOR_ARRAY_BOUND_CHECK 
    InstrNode *branch_instr;
#endif

    // load the length of array
    APPEND_INSTR5(LD, IT(0), RS(array_ref_index), ARRAY_SIZE_OFFSET);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);
    if (first_instr != NULL) *first_instr = c_instr;

	
#ifdef USE_TRAP_FOR_ARRAY_BOUND_CHECK 

    /* compare the index with the length */
    APPEND_INSTR6(SUBCC, g0, IT(0), IS(array_index_index));
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR9(TLEU, 0x11);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);

#else /* not USE_TRAP_FOR_ARRAY_BOUND_CHECK */

    /* compare the index with the length */
    APPEND_INSTR6(SUBCC, g0, IT(0), IS(array_index_index));
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR3(BCC);
    branch_instr = c_instr;
    CFGGen_process_for_branch(cfg, branch_instr);
    Instr_PredictTaken(branch_instr);

    // EXCEPTION:
    APPEND_INSTR9(TA, 0x11);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);

    // NORMAL:
    c_instr = branch_instr;
    APPEND_NOP();

#endif /* not USE_TRAP_FOR_ARRAY_BOUND_CHECK */


    return c_instr;
}

/* Name        : CSeq_create_array_store_checking_code
   Description : Make code sequences that checks array store types.
                 This is based on branch & trap instruction.
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
		 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
       Type at array_ref_index should be reference.
       Type at array_vale_index should be reference.
   
   Post-condition:
   
   Notes: If first_instr is not null, first_instr is set to first
          instruction of checking code */
InstrNode *
CSeq_create_array_store_checking_code(CFG *cfg, InstrNode *p_instr,
				      int pc, InstrNode **first_instr,
				      int array_ref_index, int value_index)
{
    //    add    rt0, rs{value_index}, g0
    //    add    rt1, rs{array_ref_index}, g0
    //    subcc  rt0, 0, g0
    //    be     FINISH
    // NORMAL:
    //    ld     [rt0], rt3       //load dtable
    //    ld     [rt1+ARRAY_ENCODINGOFFSET], it2
    //    ld     [rt3+DTABLE_ROWOFFSET], rt4
    //    srl    it2, 8, it5      // (pos >> 3)
    //    ldub   [rt4, it5], it6  // (0x1 << (7-(pos & 0x7)))
    //    andcc  it6, it2, g0
    //    te    0x14
    // FINISH:
    //    NOP
    InstrNode *c_instr, *branch_instr, *end_instr;

    c_instr = p_instr;

    APPEND_INSTR6(ADD, RT(0), RS(value_index), g0);
    if (first_instr != NULL) *first_instr = c_instr;
    APPEND_INSTR6(ADD, RT(1), RS(array_ref_index), g0);

    APPEND_INSTR5(SUBCC, g0, RT(0), 0);
    APPEND_INSTR3(BE);
    branch_instr = c_instr;
    CFGGen_process_for_branch(cfg, branch_instr);

    // NORMAL
    APPEND_INSTR5(LD, RT(3), RT(0), 0);       // get dtable pointer
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR5(LD, IT(2), RT(1), ARRAY_ENCODINGOFFSET);
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR5(LD, RT(4), RT(3), DTABLE_ROWOFFSET); //get row array
    Instr_SetLastUseOfSrc(c_instr, 0);

    APPEND_INSTR5(SRL, IT(5), IT(2), 8);      // (pos >> 3)
    APPEND_INSTR4(LDUB, IT(6), RT(4), IT(5));
    Instr_SetLastUseOfSrc(c_instr, 0);
    Instr_SetLastUseOfSrc(c_instr, 1);

    APPEND_INSTR6(ANDCC, g0, IT(6), IT(2));
    Instr_SetLastUseOfSrc(c_instr, 0);
    Instr_SetLastUseOfSrc(c_instr, 1);
    APPEND_INSTR9(TE, 0x14);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);

    //FINISH
    APPEND_NOP();
    end_instr = c_instr;
    Instr_connect_instruction (branch_instr, end_instr);
    c_instr = end_instr;

    return c_instr;
}

/* Name        : CSeq_create_array_size_checking_code
   Description : check if array size is equal to or greater than 0 at
                 run time
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
InstrNode *
CSeq_create_array_size_checking_code(CFG *cfg, InstrNode *p_instr, int pc,
				     InstrNode **first_instr,
				     int operand_stack_index)
{
    //
    //       subcc  is{operand_stack_index}, 0, g0
    //       bge    NORMAL
    //    EXCEPTION:
    //  <= execute_java_constructor(0, "java.lang.NegativeArraySizeException",
    //                              0, "()V")
    //       mov    0, IP0
    //       sethi  %hi(Negative_Array_Size_Exception), AT0
    //       or     AT0, %lo(Negative_Array_Exception) , AP1
    //       mov    0, IP2
    //       sethi  %hi(Default_Signature), AT1
    //       or     AT1, %lo(Default_Signature), AP3
    //       call   execute_java_constructor
    //    NORMAL:
    //======================================================================
    //       subcc  is{operand_stack_index}, 0, g0
    //       tl     0x12
    //   ==================================>
    //       subcc  is{operand_stack_index}, 0, g0
    //       bl     EXCEPTION
    //       nop
    //       ....
    //     EXCEPTION:
    //       ta     0x12
    //======================================================================
    //               VLIW ISA
    //======================================================================
    //       sl     is{operand_stack_index}, 0, cc
    //       tset   cc, 0x12
    //
    InstrNode *c_instr = p_instr;
    InstrNode *branch_instr;
    InstrNode *end_instr;


    APPEND_INSTR5(SUBCC, g0, IS(operand_stack_index), 0);
    if (first_instr != NULL) *first_instr = c_instr;

    APPEND_INSTR3(BL);
    branch_instr = c_instr;
    CFGGen_process_for_branch(cfg, branch_instr);

    // NORMAL:
    APPEND_NOP();
    end_instr = c_instr;

    // EXCEPTION:
    c_instr = branch_instr;
    APPEND_INSTR9(TA, 0x12);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);

    c_instr = end_instr;

#if 0
    APPEND_INSTR3(BGE);
    branch_instr = c_instr;
    CFGGen_process_for_branch(cfg, branch_instr);
    Instr_PredictTaken(branch_instr);

    // EXCEPTION:
    APPEND_INSTR9(TA, 0x12);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);

    // NORMAL:
    c_instr = branch_instr;
    APPEND_NOP();
#endif 0


    return c_instr;
}

/* Name        : CSeq_create_process_class_code
   Description : For precise class loading, insert runtime class
                 loading code
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
InstrNode *
CSeq_create_process_class_code(CFG *cfg, InstrNode *p_instr, int pc,
			       InstrNode **first_instr,
			       Hjava_lang_Class* class)
{
    //
    //    sethi  %hi(class), it0
    //    or     %it0, %lo(class), rt1
    //    ldsb   [rt1+offset_of_state], it2   <--- load class state
    //    subcc  it2, CSTATE_OK, g0
    //    bne    PROCESS_CLASS
    //  NORMAL:
    //    add    rt1, g0, g0     <- nop to indicate rt1 is last used.
    //    .....
    //
    //  PROCESS_CLASS:
    //    mov    rt1, rt2
    //    add    g0, CSTATE_OK, it3   <- This instruction can be put into delay slot
    //    call   processClass (o0: rt2, o1: it3, ret: void)
    //===============================================================
    //              VLIW ISA
    //===============================================================
    //    sethi  %hi(class), it0
    //    or     %it0, %lo(class), rt1
    //    ldsb   [rt1+offset_of_state], it2   <--- load class state
    //    subcc  it2, CSTATE_OK, g0
    //    bne    PROCESS_CLASS
    //  NORMAL:
    //    add    rt1, g0, g0     <- nop to indicate rt1 is last used.
    //    .....
    //
    //  PROCESS_CLASS:
    //    mov    rt1, rt2
    //    add    g0, CSTATE_OK, it3   <- This instruction can be put into delay slot
    //    call   processClass (o0: rt2, o1: it3, ret: void)
    //
    InstrNode *c_instr = p_instr;
    InstrNode *branch_instr;
    InstrNode *join_instr;
    InstrNode *last_instr;

    int arg_vars[2];

    int offset_of_state = ((char*) &class->state) - ((char*) class);
    
    //reset bytecode pc
    pc = -1;

    APPEND_INSTR2(SETHI, RT(0), HI(class));
    if (first_instr != NULL) *first_instr = c_instr;
    APPEND_INSTR5(OR, RT(1), RT(0), LO(class));
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR5(LDSB, IT(2), RT(1), offset_of_state);


    APPEND_INSTR5(SUBCC, g0, IT(2), CSTATE_OK);
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR3(BNE);
    branch_instr = c_instr;
    CFGGen_process_for_branch(cfg, branch_instr);


    APPEND_INSTR6(ADD, g0, RT(1), g0);
    join_instr = c_instr;
    Instr_SetLastUseOfSrc(c_instr, 0);

    last_instr = c_instr;

    //
    // taken path, PROCESS_CLASS
    //
    arg_vars[0] = RT(2);
    arg_vars[1] = IT(3);

    c_instr = branch_instr;
    APPEND_INSTR6(ADD, RT(2), RT(1), g0);
    APPEND_INSTR5(ADD, IT(3), g0, CSTATE_OK);
    APPEND_INSTR1(CALL, (int) processClass);
    CFGGen_process_for_function_call(cfg, c_instr, processClass, 
				     2, arg_vars, 0, NULL);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);

    Instr_connect_instruction(c_instr, join_instr);

    return last_instr;
}

#include "InvokeType.h"
#ifdef INVOKE_STATUS

unsigned ResolvedInlined;
unsigned ResolvedDirect;
unsigned UnresolvedInlinedTrue;
unsigned UnresolvedInlinedFalse;
unsigned Unresolved;

InstrNode*
make_increase_variable_code(CFG *cfg, InstrNode *c_instr, void* var)
{
    int pc = -10;
    APPEND_INSTR2(SETHI, RT(4), HI(var));
    APPEND_INSTR5(LD, RT(5), RT(4), LO(var));
    APPEND_INSTR5(ADD, RT(6), RT(5), 1);
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR8(ST,  RT(6), RT(4), LO(var));
    Instr_SetLastUseOfSrc(c_instr, 0);
    Instr_SetLastUseOfSrc(c_instr, 1);

    return c_instr;
}

void
print_invoke_status()
{
    fprintf(stderr, "Resolved & inlined : %d\n", ResolvedInlined);
    fprintf(stderr, "Resolved & direct : %d\n", ResolvedDirect);
    fprintf(stderr, "Unresolved & if-inline-true : %d\n", UnresolvedInlinedTrue);
    fprintf(stderr, "Unresolved & if-inline-false : %d\n", UnresolvedInlinedFalse);
    fprintf(stderr, "Unresolved : %d\n", Unresolved);
}

#endif /* INVOKE_STATUS */

#ifdef TYPE_ANALYSIS
InstrNode *
CSeq_create_pseudo_invoke_code(CFG *cfg, InstrNode *p_instr, int pc,
                               Method* called_method, InvokeType type,
                               FuncInfo* info, int ops_top)
{
    InstrNode* c_instr = p_instr;

    APPEND_INSTR1(INVOKE, (int)called_method);
/*      CFG_MarkExceptionGeneratableInstr(cfg, c_instr); */

    //To postpone generation of invoke instruction, one has to know
    //current status, e.g., operand_stack_top ...
    Instr_SetStackOffsetOfRetVal(c_instr, Var_offset_of_stack_var+ ops_top+1);
    Instr_SetOperandStackTop(c_instr, ops_top);
    Instr_SetInvokeType(c_instr, type);

    Instr_SetInstrInfo(c_instr, info);

    return c_instr;
}
#endif /* TYPE_ANALYSIS */

static InstrNode *
_create_method_inline_code(CFG *cfg, InstrNode *p_instr, int pc,
                           InlineGraph* caller_graph, Method* called_method, 
                           ArgSet* argset, int ops_top)
{
    InstrNode* c_instr = p_instr;

#ifdef INVOKE_STATUS
    c_instr = make_increase_variable_code(cfg, c_instr, &ResolvedInlined);
#endif

    APPEND_NOP();
    IG_RegisterMethod(caller_graph, called_method, pc,
                      argset, c_instr, Var_offset_of_stack_var + ops_top + 1);

    return c_instr;
}

static InstrNode *
_create_direct_call_code(CFG *cfg, InstrNode *p_instr, int pc,
                         FuncInfo* info, void* target_addr)
{
    InstrNode* c_instr = p_instr;

#ifdef INVOKE_STATUS
    c_instr = make_increase_variable_code(cfg, c_instr, &ResolvedDirect);
#endif

    APPEND_INSTR1(CALL, (int)target_addr);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);
    Instr_SetInstrInfo(c_instr, info);
    add_new_resolve_instr(cfg, c_instr, target_addr);

    return c_instr;
}

static InstrNode *
_create_ld_ld_jmpl_code(CFG *cfg, InstrNode *p_instr, int pc,
                        FuncInfo* info, int meth_idx, int ops_top)
{
    InstrNode* c_instr = p_instr;

#ifdef INVOKE_STATUS
    c_instr = make_increase_variable_code(cfg, c_instr, &Unresolved);
#endif

    APPEND_INSTR4(LD, RT(0), RS(ops_top+1), g0);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);
    APPEND_INSTR5(LD, RT(1), RT(0), sizeof(void*)*meth_idx);
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR6(JMPL, ORET, RT(1), g0);
    Instr_SetLastUseOfSrc(c_instr, 0);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);
    Instr_SetInstrInfo(c_instr, info);

    return c_instr;
}

#ifdef INLINE_CACHE
static InstrNode *
_create_inline_cache_code(CFG *cfg, InstrNode *p_instr, int pc,
                          FuncInfo* info, int meth_idx, void* target_addr)
{
    InstrNode* c_instr = p_instr;

#ifdef INVOKE_STATUS
    c_instr = make_increase_variable_code(cfg, c_instr, &Unresolved);
#endif

    APPEND_INSTR1(CALL, (int)target_addr);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);

    Instr_SetInstrInfo(c_instr, info);
    add_new_resolve_instr(cfg, c_instr, target_addr);

    // only used for inline cache
    // because call is expanded in phase 4.
    Instr_GetFuncInfo(c_instr)->dtable_offset = meth_idx;
    if (IG_GetDepth(CFGGen_current_ig_node) == 0) {
        Instr_GetFuncInfo(c_instr)->needCSIUpdate = TRUE;
    }

    Instr_SetStartOfInlineCache(c_instr);

    return c_instr;
}
#endif

static InstrNode*
_create_if_inlining_code(CFG *cfg, InstrNode *p_instr, int pc,
                         InlineGraph* caller_graph, Method* called_method, 
                         FuncInfo* info, int meth_idx, ArgSet* argset, 
                         Hjava_lang_Class* type, int ops_top)
{
    InstrNode* c_instr = p_instr;
    InstrNode *b_instr, *e_instr;
    struct _dispatchTable *dtable = type->dtable;
#ifdef VIRTUAL_INLINING_DBG
    int virtual_id;
#endif

    if (type != NULL) {
        //add receiver type to argset
        if (argset == NULL)
          argset = AS_make_from_type(type);
        else 
          AS_SetArgType(argset, 0, type);
    }
    // virtual method
    //
    // ld    [rs{TOP-arg_size}], rt0    <- get dispatch table
    // sethi %hi(dtable), rt1
    // ori   rt1, %lo(dtable), rt2      <- inlined class dtable
    // subcc rt0, rt2, g0
    // bne   _not_matched
    // add   rt0, g0, g0                <- inlining start point
    // nop
    // ba    _exit
    // _not_matched:
    // ld [rt0 + sizeof(void*) * dtable_offset], rt1
    // jmpl rt1, g0, o7
    // _exit:
    // nop
    //
    APPEND_INSTR4(LD, RT(0), RS(ops_top+1), g0);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);
    APPEND_INSTR2(SETHI, RT(1), HI(dtable));
    APPEND_INSTR5(OR, RT(2), RT(1), LO(dtable));
    Instr_SetLastUseOfSrc(c_instr, 0);
#ifdef VIRTUAL_INLINING_DBG
    virtual_id = get_new_virtual_id(caller_graph, (void*)dtable);
    c_instr = make_profile_code(cfg, c_instr, virtual_id, RT(0));
#endif // VIRTUAL_INLINING_DBG

    APPEND_INSTR6(SUBCC, g0, RT(0), RT(2));
    Instr_SetLastUseOfSrc(c_instr, 1);
    APPEND_INSTR3(BNE);
    b_instr = c_instr;
    CFGGen_process_for_branch(cfg, b_instr);

    // matched case (fall through)
    c_instr = b_instr;
    APPEND_INSTR4(ADD, g0, RT(0), g0);
    Instr_SetLastUseOfSrc(c_instr, 0);
    IG_RegisterMethod(caller_graph, called_method, pc,
                      argset, c_instr, Var_offset_of_stack_var + ops_top + 1);
#ifdef INVOKE_STATUS    
    c_instr = make_increase_variable_code(cfg, c_instr, &UnresolvedInlinedTrue);
#endif
    // exit code
    APPEND_NOP();
    e_instr = c_instr;

    // unmatched case
    c_instr = b_instr;
    APPEND_INSTR5(LD, RT(1), RT(0), sizeof(void*)*meth_idx);
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR6(JMPL, ORET, RT(1), g0);
    Instr_SetLastUseOfSrc(c_instr, 0);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);
    Instr_SetInstrInfo(c_instr, info);
#ifdef INVOKE_STATUS    
    c_instr = make_increase_variable_code(cfg, c_instr, &UnresolvedInlinedFalse);
#endif
    Instr_connect_instruction(c_instr, e_instr);

    return e_instr;
}

static void*
get_target_addr_of_call_site(Method* meth, 
                             Hjava_lang_Class* type,
                             ArgSet* argset)
{
#ifdef CUSTOMIZATION
    SpecializedMethod* sm = SM_make_SpecializedMethod(meth, argset, type);

    if (SM_GetNativeCode(sm))
      return SM_GetNativeCode(sm);
    else
      return SM_MakeTrampCode(sm);
#else
    extern void static_method_dispatcher();

    if (METHOD_TRANSLATED(meth)) {
        return METHOD_NATIVECODE(meth);
    } else {
        methodTrampoline* tramp;
        tramp = (methodTrampoline*)gc_malloc(sizeof(methodTrampoline),
					     &gc_jit_code);
        FILL_IN_JIT_TRAMPOLINE(tramp, meth, static_method_dispatcher);

        FLUSH_DCACHE(&tramp->code[0], &tramp->code[3]);

        return (void*)&tramp->code[0];
    }
#endif
}

static inline
bool
is_candidate_of_speculative_if_inlining(Method* caller, Method* callee)
{
    return ((callee->class->subclass_head == NULL)
            && IG_IsMethodInlinable(CFGGen_current_ig_node, callee));
}

#ifdef DYNAMIC_CHA
static inline
bool
is_candidate_of_speculative_inlining(Method* caller, Method* callee)
{
    return (callee->isOverridden == false
            && IG_IsMethodInlinable(CFGGen_current_ig_node, callee)
            && Method_GetByteCodeLen(callee) <= 15);
}

static InstrNode*
_create_speculative_inlining_code(CFG *cfg, InstrNode *p_instr, int pc,
                                  InlineGraph* caller_graph, 
                                  Method* called_method, FuncInfo* info, 
                                  int meth_idx, ArgSet* argset, int ops_top)
{
    InstrNode* c_instr = p_instr;
    InstrNode *b_instr, *e_instr;

    // virtual method
    //
    // spec_inline
    // add   rt0, g0, g0                <- inlining start point
    // nop
    // _exit:
    //
    // _safe_case:
    // ld    [rs{TOP-arg_size}], rt0    <- get dispatch table
    // ld    [rt0 + sizeof(void*) * dtable_offset], rt1
    // jmpl  rt1, g0, o7
    // nop
    // ba _exit
    //
    APPEND_INSTR3(SPEC_INLINE);
    //instrInfo field is used to indicate speculatively inlined method.
    c_instr->dchaInfo = (void*) called_method;
    b_instr = c_instr;
    CFGGen_process_for_branch(cfg, b_instr);

    // matched case (fall through)
    c_instr = b_instr;
    APPEND_INSTR4(ADD, g0, RT(0), g0);
    Instr_SetLastUseOfSrc(c_instr, 0);
    IG_RegisterMethod(caller_graph, called_method, pc,
                      argset, c_instr, Var_offset_of_stack_var + ops_top + 1);
#ifdef INVOKE_STATUS    
    c_instr = make_increase_variable_code(cfg, c_instr, &UnresolvedInlinedTrue);
#endif
    // exit code
    APPEND_NOP();
    e_instr = c_instr;

    // unmatched case
    c_instr = b_instr;
    APPEND_INSTR4(LD, RT(0), RS(ops_top+1), g0);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);
    APPEND_INSTR5(LD, RT(1), RT(0), sizeof(void*)*meth_idx);
    Instr_SetLastUseOfSrc(c_instr, 0);
    APPEND_INSTR6(JMPL, ORET, RT(1), g0);
    Instr_SetLastUseOfSrc(c_instr, 0);
    CFG_MarkExceptionGeneratableInstr(cfg, c_instr);
    Instr_SetInstrInfo(c_instr, info);
#ifdef INVOKE_STATUS    
    c_instr = make_increase_variable_code(cfg, c_instr, &UnresolvedInlinedFalse);
#endif
    Instr_connect_instruction(c_instr, e_instr);

    return e_instr;
}

#endif

/* Name        : CSeq_make_corresponding_invoke_sequence
   Description : make corresponding invoke sequence
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
     ops_top & corresponding stack types have to be set.
     p_instr must be a nop instruction.
   Post-condition:
   
   Notes:  ops_top must not be changed in this function */
InstrNode*
CSeq_make_corresponding_invoke_sequence(CFG* cfg, InstrNode* p_instr, int pc,
                                        Method* callee_method, int ops_top,
                                        Hjava_lang_Class* receiver_type,
                                        ArgSet* argset,
                                        FuncInfo* info,
                                        int meth_idx,
                                        bool need_null_check,
                                        bool resolved)
{
    extern InlineGraph* CFGGen_current_ig_node;
    InstrNode* c_instr = p_instr;

    Method* caller_method = IG_GetMethod(CFGGen_current_ig_node);

    if (resolved == true) {
        if (IG_IsMethodInlinable(CFGGen_current_ig_node, callee_method)) {
            c_instr = _create_method_inline_code(cfg, c_instr, pc,
                                                 CFGGen_current_ig_node,
                                                 callee_method, argset,
                                                 ops_top);
        } else { // if 'method inlining' is not possible
            void* ncode;

            if (need_null_check == true)
              c_instr 
                  = CSeq_create_null_pointer_checking_code(cfg, c_instr, pc,
                                                           NULL, ops_top+1);

            ncode = get_target_addr_of_call_site(callee_method, 
                                                 receiver_type,
                                                 argset);
            c_instr = _create_direct_call_code(cfg, c_instr, pc,
                                               info, ncode);
        }
    } else { // if 'resolved == false'
#ifdef INLINE_CACHE
        //check if it is a candidate of specialization
#if defined(CUSTOMIZATION)
        if (The_Use_Specialization_Flag
            && !IG_IsMethodInlinable(CFGGen_current_ig_node, callee_method)
            && argset != NULL
            && AS_GetCount(argset) >= 1) {
            CallContext* cc =
                CC_new(meth_idx, argset);
            void* calling_proc;
            calling_proc = CC_MakeTrampCode(cc);

            //some kind of hacking. To use typecheck code
            //this specialized call must set g1,etc.
            c_instr = _create_inline_cache_code(cfg, c_instr, pc,
                                                info, meth_idx,
                                                calling_proc);
            Instr_GetFuncInfo(c_instr)->needCSIUpdate = FALSE;
        } else
#endif
        if (!The_Final_Translation_Flag) { // if 'initial translation'
#if defined(INTERPRETER)
            if(The_Use_Interpreter_Flag)
              c_instr = _create_ld_ld_jmpl_code(cfg, c_instr, pc,
                                                info, meth_idx, ops_top);
            else
#endif
              c_instr = _create_inline_cache_code(cfg, c_instr, pc,
                                                  info, meth_idx, 
                                                  dispatch_method_with_inlinecache);
        } else {
            // check CallSiteInfo
            unsigned v_call =
                find_appropriate_virtual_call_implementation_type(caller_method, pc);
#ifdef DYNAMIC_CHA
            if (is_candidate_of_speculative_inlining(caller_method, callee_method)) {
                c_instr = 
                    _create_speculative_inlining_code(cfg, c_instr, pc,
                                                      CFGGen_current_ig_node,
                                                      callee_method, info, 
                                                      meth_idx, argset,
                                                      ops_top);

            } else
#endif
            if (v_call == CSI_UNKNOWN) {
                if (is_candidate_of_speculative_if_inlining(caller_method, callee_method)) {
                    receiver_type = callee_method->class;

                    c_instr = 
                        _create_if_inlining_code(cfg, c_instr, pc,
                                                 CFGGen_current_ig_node,
                                                 callee_method, info, 
                                                 meth_idx, argset, 
                                                 receiver_type, ops_top);
                } else {
#if defined(INTERPRETER)
                    if(The_Use_Interpreter_Flag)
                      c_instr = _create_ld_ld_jmpl_code(cfg, c_instr, pc,
                                                        info, meth_idx, ops_top);
                    else
#endif
                      c_instr = _create_inline_cache_code(cfg, c_instr, pc,
                                                          info, meth_idx,
                                                          dispatch_method_with_inlinecache);
                }

            } else if (v_call == CSI_JMPL) {
                //make_ld_ld_jmpl
                c_instr = _create_ld_ld_jmpl_code(cfg, c_instr, pc,
                                                  info, meth_idx, ops_top);

            } else { // CallSiteInfo == CSI_IFINLINE
                callee_method = (Method*)v_call; 
                
                if (!IG_IsMethodInlinable(CFGGen_current_ig_node, callee_method)) {
                    c_instr = _create_ld_ld_jmpl_code(cfg, c_instr, pc,
                                                      info, meth_idx, ops_top);
                } else { // if 'method inlining' is possible
                    extern Hjava_lang_Class* find_receiver_type(Method *method, uint32 bpc);
                    receiver_type = 
                        find_receiver_type(caller_method, pc);
                    
                    c_instr = 
                        _create_if_inlining_code(cfg, c_instr, pc,
                                                 CFGGen_current_ig_node,
                                                 callee_method, info,
                                                 meth_idx, argset,
                                                 receiver_type, ops_top);
                }
            }
        }
#else /* not INLINE_CACHE */
        c_instr = _create_ld_ld_jmpl_code(cfg, c_instr, pc,
                                          info, meth_idx, ops_top);
#endif
    }
    return c_instr;
}

InstrNode*
CSeq_create_null_returning_code(CFG *cfg, InstrNode *c_instr, Method *method)
{
    extern InlineGraph *CFGGen_current_ig_node;
    int ret_type = Method_GetRetType(method);
    int org_stack_offset = IG_GetStackTop(CFGGen_current_ig_node) 
        - Var_offset_of_stack_var;
    int pc = -13;

    // These instructions are never executed.
    // Even if callee method ends without ATHROW, the caller method expects
    // return value. So these are used for satisfying the assumption of
    // translator.
    switch (ret_type) {
      case RT_NO_RETURN:
        break;
      case RT_INTEGER:
        APPEND_INSTR6(ADD, IS(org_stack_offset), g0, g0);
        break;
      case RT_REFERENCE:
        APPEND_INSTR6(ADD, RS(org_stack_offset), g0, g0);
        break;
      case RT_LONG:
        APPEND_INSTR6(ADD, LS(org_stack_offset), g0, g0);
        APPEND_INSTR6(ADD, IVS(org_stack_offset+1), g0, g0);
        break;
      case RT_FLOAT:
        APPEND_INSTR5(LDF, FS(org_stack_offset), g0, g0);
        break;
      case RT_DOUBLE:
        APPEND_INSTR5(LDDF, DS(org_stack_offset), g0, g0);
        break;
      default:
        assert(0 && "Unknown return type. Control should not be here!\n");
    }

    return c_instr;
}



/* FIXME: this function is not implemented currently */
InstrNode*
CSeq_create_call_stat_code(CFG *cfg, InstrNode *c_instr, 
                           Method *called_method, int pc)
{
//     int    data[2];
    
//     if (called_method->accflags & ACC_PUBLIC) {
//         c_instr = register_format5_instruction(cfg, c_instr, pc,
//                                                ADD, IT(0), g0,
//                                                VIRTUAL_CALL_TO_PUBLIC);
//         c_instr = register_format5_instruction(cfg, c_instr, pc,
//                                                ADD, IT(1), g0, 1 );
//     } else if (called_method->accflags & ACC_PROTECTED) {
//         c_instr = register_format5_instruction(cfg, c_instr, pc,
//                                                ADD, IT(0), g0,
//                                                VIRTUAL_CALL_TO_PROTECTED);
//         c_instr = register_format5_instruction(cfg, c_instr, pc,
//                                                ADD, IT(1), g0, 1);
//     } else if (called_method->accflags & ACC_PRIVATE) {
//         c_instr = register_format5_instruction(cfg, c_instr, pc,
//                                                ADD, IT(0), g0,
//                                                VIRTUAL_CALL_TO_PRIVATE);
//         c_instr = register_format5_instruction(cfg, c_instr, pc,
//                                                ADD, IT(1), 0,
//                                                called_method->accflags & ACC_FINAL);
//     } else {
//         c_instr = register_format5_instruction(cfg, c_instr, pc,
//                                                ADD, IT(0), g0,
//                                                VIRTUAL_CALL_TO_ETC);
//         c_instr = register_format5_instruction(cfg, c_instr, pc,
//                                                ADD, IT(1), g0,
//                                                called_method->accflags & ACC_FINAL);
        
//     }
    
//     data[0] = IT(0);
//     data[1] = IT(1);
    
//     c_instr = register_format1_instruction(cfg, c_instr, pc,
//                                            CALL, (int) update_function_call);
    
//     process_for_function_call(cfg, c_instr, update_function_call,
//                               2, data, 0, NULL);
    return c_instr;
}



#define APPEND_IMM_ARITHMETIC_INSTR(_OPCODE_, _TARGET_, _SOURCE_, _IMM_, _IDX_) \
if ((_IMM_) >= MIN_SIMM13 && (_IMM_) <= MAX_SIMM13) {                    \
    APPEND_INSTR5(_OPCODE_, _TARGET_, _SOURCE_, _IMM_);                  \
} else {                                                                 \
    APPEND_INSTR2(SETHI, IT(_IDX_), HI(_IMM_));                          \
    APPEND_INSTR5(OR, IT(_IDX_+1), IT(_IDX_), LO(_IMM_));                \
    Instr_SetLastUseOfSrc(c_instr, 0);                                   \
    APPEND_INSTR6(_OPCODE_, _TARGET_, _SOURCE_, IT(_IDX_+1));            \
    Instr_SetLastUseOfSrc(c_instr, 1);                                   \
}

static void
_create_lookupswitch_code(CFG* cfg, InstrNode* p_instr, InstrNode* d_instr,
                          int pc, int ops_top, int num_of_pairs,
                          byte* pair_array)
{
    InstrNode* c_instr = p_instr;
    int32 key, dest;
    int32 low, high;
    
    low = BCode_get_int32(pair_array);
    high = BCode_get_int32(pair_array + (num_of_pairs-1)*8);

    if (num_of_pairs <= 2) {
        int i;

        assert(num_of_pairs != 0);

        for (i = 0; i < num_of_pairs; i++) {
            key = BCode_get_int32(pair_array + i*8);
            dest = pc + BCode_get_int32(pair_array + 4 + i*8);

            APPEND_IMM_ARITHMETIC_INSTR(SUBCC, g0, IS(ops_top+1), key, 0);
            APPEND_INSTR3(BE);
            CFGGen_process_for_branch(cfg, c_instr);

            CFGGen_process_and_verify_for_npc(dest, c_instr);
        }

        // connect fall-through path to default instruction
        Instr_connect_instruction(c_instr, d_instr);

    } else if (num_of_pairs >= 8 && (high-low+1) <= (num_of_pairs*3 - 8)) {
        // Condition:
        //  1. num_pairs > 7 : height > 2 (1+2+4=7)
        //     if height is lower than or equal to 2, then binary search 
        //     is faster
        //  2. (high-low+1) <= (num_of_pairs*3 - 8) : memory usage
        //     about (high-low+1) + 8 words memory is needed for lookuptable
        //     about num_of_pairs*3 words memory is needed for binary search
        //  
        //   sub   is{TOP}, low, it0
        //   subcc it0, high - low, g0
        //
        //   bgu   DEFAULT
        //   sethi  %hi(jumptable), it1
        //   or     it1, %lo(jumptable), rt2
        //   sll    it0, 2, it3
        //   ld     [rt2+it3], rt4
        //   jmpl   rt4, g0, g0
        //  DEFAULT:
        InstrNode* branch_instr;
        InstrNode* jump_instr;
        int jump_table_size = high - low + 1;
        int num_of_dests = 0;
        int* jump_table = 
            (int*) FMA_calloc((jump_table_size + 1) * sizeof(void*));
        int* dest_list = 
            (int*) FMA_calloc((num_of_pairs+1) * sizeof(int)); //including default
        int jump_table_offset_in_data_segment =
            CFG_AddNewDataEntry(cfg, JUMP_TABLE,
                                jump_table_size * sizeof(InstrNode *),
                                (void *) jump_table,
                                ALIGN_WORD);
        int i;

        APPEND_IMM_ARITHMETIC_INSTR(SUB, IT(0), IS(ops_top+1), low, 1);
        APPEND_IMM_ARITHMETIC_INSTR(SUBCC, g0, IT(0), high-low, 1);

        APPEND_INSTR3(BGU);
        branch_instr = c_instr;
        CFGGen_process_for_branch(cfg, c_instr); // bgu DEFAULT

        APPEND_INSTR2(SETHI, IT(1), HI(jump_table_offset_in_data_segment));
        add_new_resolve_instr(cfg, c_instr, 
                              (void *) jump_table_offset_in_data_segment);
        APPEND_INSTR5(OR, RT(2), IT(1), LO(jump_table_offset_in_data_segment));
        Instr_SetLastUseOfSrc(c_instr, 0);
        add_new_resolve_instr(cfg, c_instr, 
                              (void*) jump_table_offset_in_data_segment);
        APPEND_INSTR5(SLL, IT(3), IT(0), 2);
        Instr_SetLastUseOfSrc(c_instr, 0);
        APPEND_INSTR4(LD, RT(4), RT(2), IT(3));
        Instr_SetLastUseOfSrc(c_instr, 0);
        Instr_SetLastUseOfSrc(c_instr, 1);
        APPEND_INSTR6(JMPL, g0, RT(4), g0);
        jump_instr = c_instr;
        Instr_SetLastUseOfSrc(jump_instr, 0);

        *jump_table++ = (int) jump_instr;

        // target of branch_instr is default instruction
        Instr_connect_instruction(branch_instr, d_instr);
        // default instruction is the 0th successor of jump instruction
        Instr_connect_instruction(jump_instr, d_instr);
     
        //
        // set up jump table
        //
        // The successors are pushed in the reverse order.
        //
        c_instr = jump_instr;

        //
        // check unique destinations
        //

        // skipt default path
        num_of_dests++;

        // every untouched table entry has value 0.
        // 0 means 0th successor, i.e., the default target.
        for (i = 0; i < num_of_pairs; i++) {
            int j;
            int32 key;
            int32 offset;

            key = BCode_get_int32(pair_array + i*8);
            offset = BCode_get_int32(pair_array + 4 + i*8);

            for(j = 0; j < num_of_dests; j++) {
                if (dest_list[j] == offset) {
                    break;
                }
            }

            if (j == num_of_dests) { // new destination
                dest_list[num_of_dests++] = offset;
            }

            jump_table[key - low] = (int) j;
        }

        for (i = num_of_dests - 1; i >= 1; i--) {
            int npc = pc + dest_list[i];

            CFGGen_process_and_verify_for_npc(npc, c_instr);
        }

    } else {
        InstrNode* bg_instr;
        InstrNode* bl_instr;
        int32 idx = num_of_pairs/2;
        key = BCode_get_int32(pair_array + idx*8);
        dest = pc + BCode_get_int32(pair_array + 4 + idx*8);

        APPEND_IMM_ARITHMETIC_INSTR(SUBCC, g0, IS(ops_top+1), key, 0);

        APPEND_INSTR3(BG);
        CFGGen_process_for_branch(cfg, c_instr);
        bg_instr = c_instr;

        // less than or equal
        APPEND_INSTR3(BL);
        CFGGen_process_for_branch(cfg, c_instr);
        bl_instr = c_instr;

        // equal
        APPEND_NOP();
        CFGGen_process_and_verify_for_npc(dest, c_instr);


        // when value is larger than key
        _create_lookupswitch_code(cfg, bg_instr, d_instr, pc, ops_top,
                                  (num_of_pairs-1)/2, pair_array+(idx+1)*8);

        // when value is less than key
        _create_lookupswitch_code(cfg, bl_instr, d_instr, pc, ops_top,
                                  num_of_pairs/2, pair_array);

    }
}
/* Name        : CSeq_create_lookupswitch_code
   Description : generate code for lookupswitch.
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes: 
    Recursively generate code that bisects the set of values into 
    subregions. Kemal suggested the following idea, and I have implemented
    it according the idea - walker.

    When the number of values in the subregion is just 2 or 1,
    generate sequence of compare and branch.
    Otherwise, when the highest and lowest values in a subregion of values 
    fit within a reasonable jumptable size, and the number of distinct values
    in the subregion is not too small, generate code to use a jumptable for 
    the subregion of values.
    Otherwise, bisect the region with a compare against the median value and
    branch, and recursively generate code for the subregions */

InstrNode*
CSeq_create_lookupswitch_code(CFG* cfg, InstrNode* p_instr, int pc,
                              int ops_top, int num_of_pairs,
                              byte* pair_array)
{
    InstrNode* c_instr = p_instr;
    InstrNode* d_instr;

    APPEND_NOP();
    d_instr = c_instr;

    Instr_disconnect_instruction(p_instr, d_instr);

    _create_lookupswitch_code(cfg, p_instr, d_instr, pc, ops_top,
                              num_of_pairs, pair_array);

    return d_instr;
}



