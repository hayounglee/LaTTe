/*
 * locks-internal.h
 * Manage locking system using internal system.
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 */

#ifndef __locks_internal_h
#define __locks_internal_h

#define	MUTEX_DECL	struct Hjava_lang_Thread*

#define	MUTEX_INITIALISE(LOCK)						\
		(LOCK)->mux = NULL

#define	MUTEX_LOCK(LOCK)						\
		intsDisable();						\
		while ((LOCK)->holder != NULL) {			\
			suspendOnQThread(currentThread, &(LOCK)->mux,	\
				NOTIMEOUT);				\
		}							\
		(LOCK)->holder = currentThread;				\
		intsRestore()

#define	MUTEX_UNLOCK(LOCK)						\
		intsDisable();						\
		if ((LOCK)->mux != 0) {					\
			Hjava_lang_Thread* tid;				\
			tid = (LOCK)->mux;				\
			(LOCK)->mux = TCTX(tid)->nextQ;			\
			assert(TCTX(tid)->status != THREAD_RUNNING);	\
			iresumeThread(tid);				\
		}							\
		intsRestore()

#define	CONDVAR_DECL	struct Hjava_lang_Thread*

#define	CONDVAR_INITIALISE(LOCK)					\
		(LOCK)->cv = NULL

#define	CONDVAR_WAIT(LOCK, TIMEOUT)					\
		intsDisable();						\
		if ((LOCK)->mux != NULL) {				\
			Hjava_lang_Thread* tid;				\
			tid = (LOCK)->mux;				\
			(LOCK)->mux = TCTX(tid)->nextQ;			\
			assert(TCTX(tid)->status != THREAD_RUNNING);	\
			iresumeThread(tid);				\
		}							\
		suspendOnQThread(currentThread, &(LOCK)->cv, (TIMEOUT));\
		while ((LOCK)->holder != NULL) {			\
			suspendOnQThread(currentThread,&(LOCK)->mux,NOTIMEOUT);\
		}							\
		intsRestore()

#define	CONDVAR_SIGNAL(LOCK)						\
		intsDisable();						\
		if ((LOCK)->cv != NULL) {				\
			Hjava_lang_Thread* tid;				\
			tid = (LOCK)->cv;				\
			(LOCK)->cv = TCTX(tid)->nextQ;			\
			TCTX(tid)->nextQ = (LOCK)->mux;			\
			(LOCK)->mux = tid;				\
		}							\
		intsRestore()

#define	CONDVAR_BROADCAST(LOCK)						\
		intsDisable();						\
		if ((LOCK)->cv != NULL) {				\
			Hjava_lang_Thread** tidp;			\
			for (tidp = &(LOCK)->cv; *tidp != 0;		\
					tidp = &TCTX(*tidp)->nextQ)	\
				;					\
			(*tidp) = (LOCK)->mux;				\
			(LOCK)->mux = (LOCK)->cv;			\
			(LOCK)->cv = NULL;				\
		}							\
		intsRestore()

#define	SPINON(THING)		intsDisable()
#define	SPINOFF(THING)		intsRestore()

extern int blockInts;
extern bool needReschedule;

#define	intsDisable()	blockInts++
#define	intsRestore()	if (blockInts == 1 && needReschedule == true) {	\
				reschedule();				\
			}						\
			blockInts--     

#endif
