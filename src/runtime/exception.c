/*
 * exception.c
 * Handle exceptions for the interpreter or translator.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>
 * Modified by MASS Laboratory, SNU, 1999.
 */

#define	DBG(s)

#include <setjmp.h>
#include "config.h"
#include "config-std.h"
#include "config-signal.h"
#include "config-mem.h"
#include "jtypes.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "classMethod.h"
#include "code.h"
#include "exception.h"
#include "baseClasses.h"
#include "lookup.h"
#include "thread.h"
#include "errors.h"
#include "itypes.h"
#include "external.h"
#include "soft.h"
#include "md.h"
#include "locks.h"
#include "stackTrace.h"

#include "exception_handler.h"

#undef INLINE
#define INLINE
#include "exception.ic"

#define	DEFINEFRAME()		exceptionFrame frame
#define	EXCEPTIONFRAMEPTR	&frame

Hjava_lang_Object* buildStackTrace(struct _exceptionFrame*);

extern Hjava_lang_Object* exceptionObject;


/*
 * Throw an internal exception.
 */
void
throwException(Hjava_lang_Object* eobj)
{
	if (eobj != 0) {
		unhand((Hjava_lang_Throwable*)eobj)->backtrace =
		    buildStackTrace(0);
	}
	throwExternalException(eobj);
}

/*
 * Throw an exception.
 * I've replaced macros with real codes.
 */
void
throwExternalException(Hjava_lang_Object* eobj)
{
	register exceptionFrame *frame asm ("%sp");

	if (eobj == 0) {
		eobj = NullPointerException;
	}

	asm volatile("ta 3");

	EH_exception_manager( (Hjava_lang_Throwable*) eobj, frame );
 	exit(0);	/* Shouldn't reach here. */
}


/*
 * Setup a signal handler.
 */
void
catchSignal(int sig, void* handler)
{
	sigset_t nsig;

#if defined(HAVE_SIGACTION)

	struct sigaction newact;

	newact.sa_handler = (SIG_T)handler;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = 0;
#if defined(SA_SIGINFO)
	newact.sa_flags |= SA_SIGINFO;
#endif
	sigaction(sig, &newact, NULL);

#elif defined(HAVE_SIGNAL)

	signal(sig, (SIG_T)handler);

#else
	ABORT();
#endif

	/* Unblock this signal */
	sigemptyset(&nsig);
	sigaddset(&nsig, sig);
	sigprocmask(SIG_UNBLOCK, &nsig, 0);
}
