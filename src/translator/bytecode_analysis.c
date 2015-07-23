/* bytecode_analysis.c
   
   By this function, bytecodes are traversed and some informations are
   collected. Currently, collected information includes join point
   information.
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
               SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#include <stdlib.h>
#include <strings.h>
#include "config.h"

#include "exception.h"
#include "bytecode_analysis.h"
#include "classMethod.h"
#include "bytecode.h"
#include "gc.h"
#include "constants.h"

#define EDBG(s)    

static inline
void
check_nullpointer_eh(Method *method, constIndex idx)
{
    constants *pool;
    Utf8Const *name;
    Hjava_lang_Class *class;
    
    class = Method_GetDefiningClass(method);
    pool = Class_GetConstantPool(class);
    
    if (idx == 0) {
	Method_SetHaveNullException(method);
        return;
    }

    if (pool->tags[idx] == CONSTANT_ResolvedClass) {
        name = CLASS_CLASS(idx, pool)->name;
    } else {
        assert(pool->tags[idx] == CONSTANT_Class);
        name = WORD2UTF(pool->data[idx]);
    }

    if (equalUtf8Consts(name, object_class_name)
         || equalUtf8Consts(name, throwable_class_name)
         || equalUtf8Consts(name, exception_class_name)
	|| equalUtf8Consts(name, null_pointer_exception_name)) {
	Method_SetHaveNullException(method);
    }
}

/* Name        : insert_excepion_handlers_into_tstack
   Description : inserts exception handlers into traverse stack so
                 that all bytecodes in the method can be traversed
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
inline static
int32
insert_exception_handlers(Method *method, int *tstack, int tstack_top,
			  int *visited, int8 *binfo) 
{
    jexception *etable;
    int i;
    
    etable = Method_GetExceptionTable(method);

    if (etable != NULL) {
	for(i = 0; i < JE_GetLength(etable); i++) {
	    jexceptionEntry *entry;
	    int handler_pc;
	    int j;
	
	    entry = JE_GetEntry(etable, i);
	    handler_pc = JEE_GetHandlerPC(entry);
	

	    for(j = JEE_GetStartPC(entry); j <= JEE_GetEndPC(entry); j++) {
		binfo[j] |= METHOD_WITHIN_TRY;
	    }

	    check_nullpointer_eh(method, JEE_GetCatchIDX(entry));
	
	    tstack[++tstack_top] = handler_pc;
	    visited[handler_pc] = true;
	}
    }
    
    return tstack_top;
}

/* Name        : process_for_npc
   Description : record next processing bytecode pc, and detect join point
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   	npc is next pc to process.
   
   Post-condition:
   	if npc is join point, join_points array value is set
   
   Notes:  */
inline static
int32
process_for_npc(Method *method, uint32 npc, int32 *tstack, int32 tstack_top,
		int32 *visited, int8 *binfo)
{
    if (visited[npc]) {
	binfo[npc] |= METHOD_JOIN_POINT;
    } else {
        visited[npc] = true;
        tstack[++tstack_top] = npc;
    }

    return tstack_top;
}

/* Name        : BA_bytecode_analysis
   Description : traverses bytecodes and collect some informations
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
BA_bytecode_analysis(Method *method)
{
    int32 pc, npc;
    int32 addr;
    byte opcode;
    int32 i;
    uint8 *bcodes;
    int32 codelen;
    int32 *tstack;
    int32 *visited;
    int32 tstack_top;
    bool wide;
    int8 *binfo;

    /* check if bytecode information is already collected */
    if (Method_GetBcodeInfo(method) != NULL) return;

    codelen = Method_GetByteCodeLen(method);
    bcodes = Method_GetByteCode(method);

    /* tstack stores next bytecode pc to traverse */
    tstack = (int *) alloca(sizeof(int) * codelen);
    bzero(tstack, sizeof(int) * codelen);

    /* allocate bytecode information */
    binfo = (int8 *) gc_malloc_fixed(sizeof(int8) * codelen);
    bzero(binfo, sizeof(int8) * codelen);
    
    /* traversing stack top */
    tstack_top = -1;

    /* visited array element is set if that bytecode is traversed */
    visited = (int *) alloca(sizeof(int) * codelen);
    bzero(visited, sizeof(int) * codelen);

    tstack_top = insert_exception_handlers(method, tstack, tstack_top,
					   visited, binfo);
    
    /* The first instruction of method will be traversed. */
    tstack[++tstack_top] = 0;
    visited[0] = true;

    wide = false;

    while(tstack_top >= 0) {
        assert(tstack_top < codelen);

	/* pop from the stack */
        pc = tstack[tstack_top--];	
        opcode = BCode_get_uint8(bcodes + pc);

        switch(opcode) {
          case IFEQ: case IFNE: case IFLT:
          case IFGE: case IFGT: case IFLE:
          case IF_ICMPEQ: case IF_ICMPNE:
          case IF_ICMPLT: case IF_ICMPGE:
          case IF_ICMPGT: case IF_ICMPLE:
          case IF_ACMPEQ: case IF_ACMPNE:
          case IFNULL:    case IFNONNULL:
            //
            // There are two successors.
            //
            // for the fall-through path
            npc = pc + BCode_get_opcode_len(opcode);
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);

            // for the taken path
            npc = pc + BCode_get_int16(bcodes + pc + 1);
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);

	    Method_SetHaveBranch(method);
#ifdef VARIABLE_RET_COUNT 
            Method_IncreaseBranchNO(method);
#endif /* VARIABLE_RET_COUNT */

            break;

          case GOTO:
            npc = pc + BCode_get_int16(bcodes + pc + 1);
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);
            break;

          case GOTO_W:
            npc = pc + BCode_get_int32(bcodes + pc + 1);
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);
            break;

            // jsr and jsr_w are used to implement the finally clauses.
            // Its only successor is the instruction next to them.
            // Its destination is a finally block.
          case JSR:
            npc = pc + BCode_get_opcode_len(opcode);
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);

            addr = pc + BCode_get_int16(bcodes + pc + 1);
	    /* finally block entry should not be marked as join point,
	       because finally blocks will be inlined into each jsr site. */
            if (!visited[addr]) 
		tstack_top = process_for_npc(method, addr, tstack, tstack_top,
					     visited, binfo);

            break;

          case JSR_W:
            npc = pc + BCode_get_opcode_len(opcode);
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);

            addr = pc + BCode_get_int32(bcodes + pc + 1);
            if (!visited[addr]) 
		tstack_top = process_for_npc(method, addr, tstack, tstack_top,
					     visited, binfo);

            break;

          case TABLESWITCH:
            addr = (pc + 4) & 0xfffffffc; // consider padding
            i = BCode_get_int32(bcodes + addr + 8) -
                BCode_get_int32(bcodes + addr + 4) + 1;
            for (; i > 0; i--) {
                npc = pc + BCode_get_int32(bcodes + addr + 8 + i * 4);
                tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					     visited, binfo);
            }

            npc = pc + BCode_get_int32(bcodes + addr);
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);
            break;

          case LOOKUPSWITCH:
            addr = (pc + 4) & 0xfffffffc; // consider padding
            // get the number of pairs
            i = BCode_get_int32(bcodes + addr + 4);
            for (; i > 0; i--) {
                npc = pc + BCode_get_int32(bcodes + addr + 4 + i * 8);
                tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					     visited, binfo);
            }
            npc = pc + BCode_get_int32(bcodes + addr);
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);
            break;

          case IRETURN: case LRETURN: case ARETURN:
          case FRETURN: case DRETURN: case RETURN:
	    Method_SetHaveReturn(method);
	    break;
	    
	case ATHROW:
	    Method_SetHaveAthrow(method);
            break;			// no successor

          case RET:
            break;

	  /* The xxx_W instructions in the following are rewritten
	     bytecodes of WIDE (rewritten by the interpreter). */
	  case ILOAD_W:  case LLOAD_W:  case FLOAD_W:
	  case DLOAD_W:  case ALOAD_W:
	  case ISTORE_W: case LSTORE_W: case FSTORE_W:
	  case DSTORE_W: case ASTORE_W:
	  case RET_W:    case IINC_W:
          case WIDE:
            wide = true;
            npc = pc + BCode_get_opcode_len(opcode);
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);
            break;

          case ILOAD:   case LLOAD:   case FLOAD:
          case DLOAD:   case ALOAD:
          case ISTORE:  case LSTORE:  case FSTORE:
          case DSTORE:  case ASTORE:
            npc = pc + BCode_get_opcode_len(opcode);
            if (wide) {
                wide = false;
                npc++;
            }
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);
            break;

          case IINC:
            npc = pc + BCode_get_opcode_len(opcode);
            if (wide) {
                wide = false;
                npc += 2;
            }
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);
            break;

          case INVOKEINTERFACE:
	    Method_SetHaveUnresolved(method);
	    Method_SetHaveInvoke(method);
            npc = pc + BCode_get_opcode_len(opcode);
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);
            break;

          case INVOKEVIRTUAL:
          {
              uint8 *bcode = Method_GetByteCode(method);
              uint16 index = BCode_get_uint16(bcode + pc + 1);
              Method* called_method = Method_GetReferredMethod(method, index);
              if (!(Method_GetAccessFlags(called_method) & (ACC_FINAL|ACC_PRIVATE)))
                Method_SetHaveUnresolved(method);
          }
                
	  case INVOKESTATIC:
	  case INVOKESPECIAL:
	    Method_SetHaveInvoke(method);
	      
          default:
            npc = pc + BCode_get_opcode_len(opcode);
            tstack_top = process_for_npc(method, npc, tstack, tstack_top,
					 visited, binfo);
            break;
        }
    }

    Method_SetBcodeInfo(method, binfo);
}

