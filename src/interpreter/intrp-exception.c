/* intrp-exception.c
   Support for interpreted exception handlers.
   
   Written by: Yoo C. Chung <chungyc@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Laboratory, Seoul National University.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

/* This supports only SPARC Solaris. */

#include "config.h"
#include "config-std.h"
#include "gtypes.h"
#include "jit.h"
#include "classMethod.h"
#include "access.h"
#include "locks.h"
#include "exception.h"
#include "exception_handler.h"
#include "java_lang_Throwable.h"
#include "interpreter.h"


/* Size of each opcode implementation.
   Should be synchronized with DISP in interpreter.S */
#define DISP		256

/* Interpreter method pointer. */
#define INTRP_METH(f)	((f)->arg[0])

/* Interpreter frame pointer. */
#define INTRP_FRAME(f)	((f)->lcl[0])

/* Interpreter bytecode program counter. */
#define INTRP_BPC(f)	((f)->arg[2])

/* Interpreter local variables location. */
#define INTRP_LOCALS(f)	((f)->arg[4])

/* Structure of the interpreter stack frame.  Should be synchronized
   with interpreter.S, where the offsets of each field are hardwired. */
struct intrp_frame
{
  void *dummy;		/* Slack. */
  void *locals;		/* Caller local variables. */
  void *top;		/* Caller operand stack top. */
  void *bpc;		/* Caller bytecode program counter. */
  void *method;		/* Caller method pointer. */
  void *pool;		/* Caller pool for resolved constant items. */
  void *fp;		/* Caller interpreter stack frame. */
  void *target;		/* Caller return address. */
};


/* Name        : intrp_inside_interpreter
   Description : Check whether program counter is in interpreter.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
inline int
intrp_inside_interpreter (void *pc)
{
    return (pc >= (void*)_interpret_start && pc < (void*)_interpret_end);
}

/* Name        : intrp_find_handler
   Description : Find exception handler for stack frame in interpreter.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Returns -1 if there are no exception handlers for the method. */
int
intrp_find_handler (exceptionFrame *frame, Hjava_lang_Class *eclass)
{
    int bpc;
    Method *method;
    jexceptionEntry *handler;

    frame = (exceptionFrame*)frame->retbp;
    method = (Method*)INTRP_METH(frame);

    bpc = (char*)INTRP_BPC(frame) - (char*)method->bcode;
    handler = Method_GetExceptionHandler(method, bpc, eclass);

    if (handler != NULL)
	return handler->handler_pc;
    else
	return -1;
}

/* Name        : intrp_unwind_stack
   Description : Unwind stack for interpreted code.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Post-condition:
     The native stack frame is changed to that of the caller if the
     caller is interpreted.
   Notes:
     Returns native stack frame of caller. */
exceptionFrame*
intrp_unwind_stack (exceptionFrame *frame)
{
    Method *method;
    exceptionFrame *old_frame, *caller_frame;
    struct intrp_frame *callee_frame;

    old_frame = frame;
    frame = (exceptionFrame*)frame->retbp;
    callee_frame = (struct intrp_frame*)INTRP_FRAME(frame);

    /* Unlock mutex if synchronized method. */
    method = (Method*)INTRP_METH(frame);
    if (Method_IsSynchronized(method)) {
	if (Method_IsStatic(method))
	    /* Unlock class. */
	    unlockMutex((Hjava_lang_Object*)method->class);
	else
	    /* Unlock object. */
	    unlockMutex(*((Hjava_lang_Object**)INTRP_LOCALS(frame)));
    }

    if (callee_frame->method == NULL)
	/* Caller is native code.  Unwind native stack. */
	caller_frame = frame;
    else {
	/* Caller is interpreted code.
	   Restore registers stored in native stack frame. */
	caller_frame = old_frame;
	frame->arg[0] = (uintp)callee_frame->method;
	frame->arg[2] = (uintp)callee_frame->bpc;
	frame->arg[3] = (uintp)callee_frame->top;
	frame->arg[4] = (uintp)callee_frame->locals;
	frame->arg[5] = (uintp)callee_frame->target;
	frame->lcl[0] = (uintp)callee_frame->fp;
	frame->lcl[1] = (uintp)callee_frame->pool;

	/* The target address is not unwound since it is only used for
	   normal returns. */
    }

    return caller_frame;
}

/* Name        : intrp_invoke_handler
   Description : Invoke interpreted exception handler.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
void
intrp_invoke_handler (exceptionFrame *frame,
		      Hjava_lang_Throwable *exception, int bpc)
{
    char opcode;
    void *target, *fp;
    Method *method;

    fp = (void*)frame->retbp;
    frame = (exceptionFrame*)frame->retbp;
    method = (Method*)frame->arg[0];

    /* Switch the bytecode PC of the interpreter to that of the
       exception handler. */
    frame->arg[2] = (uintp)((char*)method->bcode + bpc);

    /* Empty the operand stack and push exception object. */
    frame->arg[3] = frame->lcl[0] - sizeof(int);
    *((void**)frame->arg[3]) = exception;

    /* Load the first opcode in the exception handler. */
    opcode = *((char*)method->bcode + bpc);

    /* Compute address to jump to. */
    target = (char*)_interpret_start + (opcode * DISP);

    /* Reset the stack and jump to the exception handler. */
    asm volatile ("ta 3\n\t"	/* not sure what this is for */
		  "mov %0, %%fp\n\t"
		  "restore %1, 0, %%g1\n\t"
		  "jmp %%g1\n\t"
		  "ld [ %%sp + 56 ], %%fp\n\t"
		  : : "r" (fp), "r" (target)
		  : "%fp", "%g1");
}
