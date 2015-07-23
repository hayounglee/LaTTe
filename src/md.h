/*
 * sparc/solaris2/md.h
 * Solaris2 sparc configuration information.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#ifndef __sparc_solaris2_md_h
#define __sparc_solaris2_md_h

#include "runtime/threads.h"


/*
 * If we're using the SparcWorks Prof C 4.0 compiler we do things differently.
 */
#if defined(USE_SPARC_PROF_C)

void	ThreadSwitch(ctx* to, ctx* from);
int	ThreadFrames(thread* tid);
void	ThreadInit(ctx* to, void (*)());
void 	ThreadInfo(ctx* ee);

#undef	THREADSWITCH
#undef	THREADINIT
#undef	THREADINFO
#undef	THREADFRAMES

#define	THREADSWITCH(to, from)	ThreadSwitch(to, from)
#define	THREADINIT(to, func)	ThreadInit(to, func)
#define	THREADINFO(ee)		ThreadInfo(ee)
#define	THREADFRAMES(tid, cnt)	cnt = ThreadFrames(tid)

#endif

#include "jit.h"

/**/
/* Extra exception handling information. */
/**/
#include <ucontext.h>

/* Function prototype for signal handlers */
#define	EXCEPTIONPROTO							\
	int sig, siginfo_t* sip, ucontext_t* ctx

/* Get the first exception frame from a signal handler */
#define	EXCEPTIONFRAME(f, c)						\
	(f).retbp = (c)->uc_mcontext.gregs[REG_SP];			\
	(f).retpc = (c)->uc_mcontext.gregs[REG_PC]

#endif
