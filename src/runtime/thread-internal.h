/*
 * thread-internal.h
 * Thread support using internal system.
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 * Modified by MASS Laboratory, SNU, 1999.
 */

#ifndef __thread_internal_h
#define __thread_internal_h

#include "config-setjmp.h"


struct Hjava_lang_Thread;

typedef struct _ctx {
	uint8				status;
	uint8				priority;
	uint8*				restorePoint;
	uint8*				stackBase;
	uint8*				stackEnd;

#ifdef INTERPRETER
	uint8*				jrestorePoint;
	uint8*				jstackBase;
	uint8*				jstackEnd;
#endif /* INTERPRETER */

	jlong				time;
	struct Hjava_lang_Thread*	nextQ;
	struct Hjava_lang_Thread*	nextlive;
	struct Hjava_lang_Thread*	nextalarm;
	struct Hjava_lang_Thread**	blockqueue;
	uint8				flags;
	void*				exceptPtr;
	void				(*func)(void);
	jmp_buf				env;

	/* List of native exception handlers used in the thread. */
	struct _EH_native_handler*	native_handlers;

#ifdef USE_TRANSLATOR_STACK
	uint8*  curStack;
#endif


	/* for alignment (Gcc extension) */
	double				align[0];
} ctx;

extern struct Hjava_lang_Thread* liveThreads;
extern struct Hjava_lang_Thread* alarmList;
extern struct Hjava_lang_Thread* threadQhead[];
extern struct Hjava_lang_Thread* threadQtail[];
extern ctx**	threadContext;

#ifdef INTERPRETER
extern uint8* jstack_point; /* Top of Java interpreter stack. */
#endif

#define TCTX(t)	(threadContext[unhand(t)->PrivateInfo])

void	suspendOnQThread(struct Hjava_lang_Thread*, struct Hjava_lang_Thread**, jlong, bool from_wait);
void	iresumeThread(struct Hjava_lang_Thread*);
#ifdef NEW_CTX_SWITCH
void    suspendOnLockQueue( struct Hjava_lang_Thread*  tid,
                            struct Hjava_lang_Thread** queue );
void    resumeFromLockQueue( struct Hjava_lang_Thread* tid );
#endif NEW_CTX_SWITCH
void	killThread(void);
int	blockOnFile(int, int);
void	reschedule(void);
void	freeThreadCtx(int);

/*
 * We are depricating the use of the THREADSWITCH macros and using a
 * longjump/setjmp strategy instead.  For the moment we'll support both.
 */
#if !defined(THREADSWITCH)

#define GET_SP(E)	(((void**)(E))[SP_OFFSET])
#define SET_SP(E, V)	((void**)(E))[SP_OFFSET] = (V)

#define	STACK_COPY	(32*4)
#define	STACK_ALLOWANCE	128

#define	THREADSWITCH(TO, FROM)						\
		if (setjmp((FROM)->env) == 0) {				\
			(FROM)->restorePoint = GET_SP((FROM)->env);	\
			longjmp((TO)->env, unhand(currentThread)->PrivateInfo);\
		}							\

#define	THREADINIT(TO, FUNC)						\
		{							\
			int cidx = setjmp((TO)->env);			\
			if (cidx == 0) {				\
				SET_SP((TO)->env,(TO)->stackEnd-STACK_COPY);\
				(TO)->func = (FUNC);			\
			}						\
			else {						\
				(*threadContext[cidx]->func)();		\
			}						\
		}

#define	THREADINFO(TO)							\
		do {                                           		\
			uint8* ptr;                            		\
			int i;                                 		\
			setjmp((TO)->env);				\
			ptr = STACK_ALLOWANCE + (uint8*)GET_SP((TO)->env);\
			(TO)->restorePoint = 0;                		\
			(TO)->stackEnd = (void*)ptr;           		\
			(TO)->stackBase = (TO)->stackEnd - threadStackSize;\
			(TO)->flags = THREAD_FLAGS_NOSTACKALLOC;	\
		} while (0)

#define	THREADFRAMES(THRD, CNT)						\
		CNT = 0

#endif

#define CURRENTTHREAD()							\
		(currentThread)

/* Set interpreter stack point for first thread. */
#ifdef INTERPRETER
#define THREAD_CREATEFIRST_INTRP(TID) do {				\
		jstack_point = TCTX(TID)->jrestorePoint;		\
	} while (0)
#else /* not INTERPRETER */
#define THREAD_CREATEFIRST_INTRP(TID) do { /* nothing */ } while (0)
#endif /* not INTERPRETER */

#define	THREAD_CREATEFIRST(TID)						\
		allocThreadCtx(TID, 0);					\
		assert(unhand(TID)->PrivateInfo != 0);			\
 		TCTX(TID)->priority = (uint8)unhand(TID)->priority;	\
		TCTX(TID)->status = THREAD_SUSPENDED;			\
		TCTX(TID)->flags = THREAD_FLAGS_NOSTACKALLOC;		\
		TCTX(TID)->nextlive = liveThreads;			\
		liveThreads = (TID);					\
		THREADINFO(TCTX(TID));					\
		THREAD_CREATEFIRST_INTRP(TID);				\
		talive++;						\
		iresumeThread(TID)


#define	THREAD_CREATE(TID, FUNC)					\
		assert(unhand(TID)->PrivateInfo == 0);			\
		allocThreadCtx(TID, threadStackSize);			\
		assert(unhand(TID)->PrivateInfo != 0);			\
		TCTX(TID)->priority = (uint8)unhand(TID)->priority;	\
		TCTX(TID)->status = THREAD_SUSPENDED;			\
		TCTX(TID)->flags = THREAD_FLAGS_GENERAL;		\
		TCTX(TID)->nextlive = liveThreads;			\
		liveThreads = (TID);					\
		assert(FUNC != 0);					\
		THREADINIT(TCTX(TID), FUNC);				\
		talive++;						\
		gc_set_finalizer(TID, &gc_thread);			\
		if (unhand(TID)->daemon) {				\
			tdaemon++;					\
		}							\
		iresumeThread(TID)

#ifndef NEW_CTX_SWITCH
#define	THREAD_YIELD()							\
		intsDisable();						\
		if (threadQhead[TCTX(currentThread)->priority] != 	\
		    threadQtail[TCTX(currentThread)->priority]) {	\
			/* Get the next thread and move me to the end */\
			threadQhead[TCTX(currentThread)->priority] =	\
				TCTX(currentThread)->nextQ;		\
			TCTX(threadQtail[TCTX(currentThread)->		\
				priority])->nextQ = currentThread;	\
			threadQtail[TCTX(currentThread)->priority] =	\
				currentThread;				\
			TCTX(currentThread)->nextQ = 0;			\
			needReschedule = true;				\
		}							\
		intsRestore()
#else
#define	THREAD_YIELD()							\
		if (threadQhead[TCTX(currentThread)->priority] != 	\
		    threadQtail[TCTX(currentThread)->priority]) {	\
			/* Get the next thread and move me to the end */\
			threadQhead[TCTX(currentThread)->priority] =	\
				TCTX(currentThread)->nextQ;		\
			TCTX(threadQtail[TCTX(currentThread)->		\
				priority])->nextQ = currentThread;	\
			threadQtail[TCTX(currentThread)->priority] =	\
				currentThread;				\
			TCTX(currentThread)->nextQ = 0;			\
            reschedule();   \
		}
#endif NEW_CTX_SWITCH


#define	THREAD_SLEEP(TIME)						\
		if ((TIME) == 0) {					\
			return;						\
		}							\
		intsDisable();						\
		suspendOnQThread(currentThread, 0, (TIME), false);	\
		intsRestore()

#define	THREAD_ALIVE(TID, STATUS)					\
		intsDisable();						\
		if (unhand(TID)->PrivateInfo == 0 ||			\
                    TCTX(TID)->flags & THREAD_FLAGS_STOPPED ||          \
		    TCTX(TID)->status == THREAD_DEAD) {                 \
			STATUS = false;					\
		}							\
		else {							\
			STATUS = true;					\
		}							\
		intsRestore();

#define	THREAD_STOP(TID)						\
                TCTX(TID)->flags |=                                     \
                     (THREAD_FLAGS_KILLED | THREAD_FLAGS_STOPPED);	\
                resumeThread(TID)

#define	THREAD_FRAMES(TID, COUNT)					\
		THREADFRAMES(TID, COUNT)

#ifdef NEW_CTX_SWITCH
#define THREAD_INIT()    blockInts = 0
#else
#define	THREAD_INIT()							\
		/* Every thread starts with the interrupts off */	\
		intsRestore()
#endif NEW_CTX_SWITCH


#define	THREAD_EXIT()							\
		for (;;) {						\
			killThread();					\
			sleepThread(1000);				\
		}

#define	THREAD_FREE(TID)						\
		if (unhand(TID)->PrivateInfo != 0) {			\
			ctx* ct;					\
			ct = threadContext[unhand(TID)->PrivateInfo];	\
			threadContext[unhand(TID)->PrivateInfo] = NULL;	\
			numberOfThreads--;				\
			unhand(TID)->PrivateInfo = 0;			\
			gc_free_fixed(ct);				\
		}

/* Flags used for threading I/O calls */
#define	TH_READ				0
#define	TH_WRITE			1
#define	TH_ACCEPT			TH_READ
#define	TH_CONNECT			TH_WRITE

#define THREAD_SUSPENDED		0
#define THREAD_RUNNING			1
#define THREAD_DEAD			2

#define	THREAD_FLAGS_GENERAL		0
#define	THREAD_FLAGS_NOSTACKALLOC	1
#define	THREAD_FLAGS_KILLED		2
#define	THREAD_FLAGS_ALARM		4
#define	THREAD_FLAGS_USERSUSPEND	8
#define	THREAD_FLAGS_ERROR		16
#define THREAD_FLAGS_STOPPED            32

#endif
