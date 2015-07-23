/* interpreter.h
   Interface to the bytecode interpreter.
   
   Written by: Yoo C. Chung <chungyc@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Laboratory, Seoul National University
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef _INTERPRETER_H_INCLUDED
#define _INTERPRETER_H_INCLUDED

/* Locations pointing to start and end of interpreter code. */
void _interpret_start (void), _interpret_end (void);

/* Invoke interpreter. */
void intrp_execute (Method *m, void *args, void *bytecode);

/* Check whether to initially interpret method. */
int intrp_interpretp (Method *m);

/* Return code which calls interpreter from native code. */
void* intrp_interpreter_caller (Method *m);

/* Return code which calls native code from interpreter. */
void* intrp_native_caller (Method *m);


/*** Support routines for exception handling. ***/

/* Check whether program counter is in interpreter. */
extern inline int
intrp_inside_interpreter (void *pc)
{
    return (pc >= (void*)_interpret_start && pc < (void*)_interpret_end);
}

/* Find associated exception handler for an interpreter stack frame. */
int intrp_find_handler (exceptionFrame *frame, Hjava_lang_Class *eclass);

/* Unwind stack for interpreted code.
   Returns caller native stack frame. */
exceptionFrame* intrp_unwind_stack (exceptionFrame *frame);

/* Invoke exception handler. */
void intrp_invoke_handler (exceptionFrame *frame,
			   Hjava_lang_Throwable *exception, int bpc);

#endif /* _INTERPRETER_H_INCLUDED */
