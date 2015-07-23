/*
 * thread.h
 * Thread support.
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 */

#ifndef __thread_h
#define __thread_h

#include "md.h"

/*
 * Use native threads is available, otherwise use the internal ones.
 */
#if defined(USE_NATIVE_THREADS)
#include "thread-native.h"
#else
#include "thread-internal.h"
#endif

#include "native.h"
#include "java_lang_Thread.h"
#include "java_lang_ThreadGroup.h"

#define	THREADCLASS			"java/lang/Thread"
#define	THREADGROUPCLASS		"java/lang/ThreadGroup"
#define	THREADDEATHCLASS		"java/lang/ThreadDeath"

#define	NOTIMEOUT			0

/*
 * Interface to the thread system.
 */
void	initThreads(void);
void	yieldThread(void);
void	sleepThread(jlong);
bool	aliveThread(Hjava_lang_Thread*);
jint	framesThread(Hjava_lang_Thread*);
void	setPriorityThread(Hjava_lang_Thread*, jint);
void	resumeThread(Hjava_lang_Thread*);
void	suspendThread(Hjava_lang_Thread*);
void	startThread(Hjava_lang_Thread*);
void	stopThread(Hjava_lang_Thread*);

extern int threadStackSize;
extern Hjava_lang_Thread* currentThread;

extern unsigned short     currentThreadID;

#endif


