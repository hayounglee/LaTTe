/* exception_handler.h
   
   Header file for exception handling
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
               SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __EXCEPTION_HANDLER_H__
#define __EXCEPTION_HANDLER_H__

#include <setjmp.h>
#include <sys/signal.h>
#include <sys/ucontext.h>

struct _exceptionFrame;
struct Hjava_lang_Throwable;

/* Macros for catching exceptions in native code.
   Use them like this:

     EH_NATIVE_DURING
       exception handling domain
     EH_NATIVE_HANDLER
       exception handler
     EH_NATIVE_ENDHANDLER

   The exception caught by the handler will be put in the *local*
   variable 'captive_exception'.  Make sure that the function does not
   exit from within the code within the exception handling domain,
   since the implementation requires this.

   These catch *all* exceptions, so beware.

   The trap ("ta 3") is necessary to block harmful optimizations. */
#define EH_NATIVE_DURING ({					\
	void *frame;						\
	struct _EH_native_handler _EH_handler;			\
	asm volatile ("ta 3\nmov %%fp, %0\n": "=r" (frame));	\
	EH_set_native_handler(&_EH_handler, frame);		\
	if (!setjmp(_EH_handler.state)) {

#define EH_NATIVE_HANDLER					\
		EH_release_native_handler(&_EH_handler);	\
	} else {						\
		struct Hjava_lang_Throwable *captive_exception;	\
		captive_exception = _EH_handler.exception;

#define EH_NATIVE_ENDHANDLER }});

/* This is the structure used by native code to catch exceptions. */
struct _EH_native_handler {
    jmp_buf state;		/* Stack state stored by setjmp(). */
    void *fp;			/* The frame's frame pointer. */
    struct Hjava_lang_Throwable *exception; /* The caught exception. */
    struct _EH_native_handler *parent;	/* The previous native handler. */
};

/* These functions are used for catching exceptions in native code.
   Do not use these functions directly.  Use the macros defined above
   instead. */
void EH_set_native_handler (struct _EH_native_handler*, void*);
void EH_release_native_handler (struct _EH_native_handler*);

/* we need trap handler function declarations while initializing
   exceptions */
void EH_trap_handler(int sig, siginfo_t *siginfo, ucontext_t *ctx);
void EH_null_exception(int sig, siginfo_t *siginfo, ucontext_t* ctx);
void EH_arithmetic_exception(int sig, siginfo_t *siginfo, ucontext_t *ctx);

/* exception manager function
   find, translate, transfer control to appropriate exception handler */
void EH_exception_manager(struct Hjava_lang_Throwable *exception_object,
			  struct _exceptionFrame *stack_frame);

#endif __EXCEPTION_HANDLER_H__
