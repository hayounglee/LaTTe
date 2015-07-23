/*
 * thread.c
 * Thread support.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#define	DBG(s)
#define	SDBG(s)

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "config-io.h"
#include "config-signal.h"
#include "jtypes.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "classMethod.h"
#include "baseClasses.h"
#include "lookup.h"
#include "thread.h"
#include "locks.h"
#include "exception.h"
#include "support.h"
#include "external.h"
#include "errors.h"
#include "lerrno.h"
#include "gc.h"
#include "md.h"

Hjava_lang_Class* ThreadClass;
Hjava_lang_Class* ThreadGroupClass;
Hjava_lang_Thread* currentThread;
Hjava_lang_Thread* garbageman;
Hjava_lang_Thread* finalman_thread;
Hjava_lang_ThreadGroup* standardGroup;
int threadStackSize;
int daemonThreadStackSize;

unsigned short     currentThreadID;

#ifdef TRANSLATOR
extern void*       translator_stack;
#endif

static void firstStartThread(void);
static void createInitialThread(char*);
static Hjava_lang_Thread* createDaemon(void*, char*);

/****/
static int numberOfThreads;
static int talive;
static int tdaemon;
static void alarmException(int);
static void allocThreadCtx(Hjava_lang_Thread*, int);
/***/

/*
 * Initialise threads.
 */
void
initThreads(void)
{
	/* Set default thread stack size if not set */
	if (threadStackSize == 0) {
		threadStackSize = THREADSTACKSIZE;
    }

	/* Get a handle on the thread and thread group classes */
	ThreadClass = lookupClass(THREADCLASS);
	assert(ThreadClass != 0);
	ThreadGroupClass = lookupClass(THREADGROUPCLASS);
	assert(ThreadGroupClass != 0);

	/* Create base group */
	standardGroup = (Hjava_lang_ThreadGroup*)newObject(ThreadGroupClass);
	assert(standardGroup != 0);
	unhand(standardGroup)->parent = 0;
	unhand(standardGroup)->name = makeJavaString("main", 4);
	unhand(standardGroup)->maxPriority = java_lang_Thread_MAX_PRIORITY;
	unhand(standardGroup)->destroyed = 0;
	unhand(standardGroup)->daemon = 0;
	unhand(standardGroup)->vmAllowSuspension = 0;
	unhand(standardGroup)->nthreads = 0;
	unhand(standardGroup)->threads = 0;
	unhand(standardGroup)->ngroups = 0;
	unhand(standardGroup)->groups = 0;

	/* Allocate a thread to be the main thread */
	createInitialThread("main");

#if defined(GC_ENABLE)
	/* Start the GC daemons we need */
	finalman_thread = createDaemon(&gc_finalize_main, "finaliser");
	garbageman = createDaemon(&gc_main, "gc");
	gc_mode = GC_ENABLED;
#endif

#if !defined(USE_NATIVE_THREADS)
	/* Plug in the alarm handler */
#if defined(SIGALRM)
	catchSignal(SIGALRM, alarmException);
#else
#error "No alarm signal support"
#endif
#endif
}

/*
 * Start a new thread running.
 */
void
startThread(Hjava_lang_Thread* tid)
{
	THREAD_CREATE(tid, &firstStartThread);
}

/*
 * Stop a thread from running and terminate it.
 */
void
stopThread(Hjava_lang_Thread* tid)
{
    // If this thread is already dead, or already stopped by this method,
    // there is no need to re-stop this thread.
    // - 1999.10.15 walker
    if (TCTX(tid)->status == THREAD_DEAD
        || TCTX(tid)->flags & THREAD_FLAGS_STOPPED)
      return;
    if (currentThread == tid) {
        SignalError(0, "java.lang.ThreadDeath", "");
    }
    else {
        THREAD_STOP(tid);
    }
}

/*
 * Create the initial thread with a given name.
 *  We should only ever call this once.
 */
static
void
createInitialThread(char* nm)
{
	/* Allocate a thread to be the main thread */
	currentThread = (Hjava_lang_Thread*)newObject(ThreadClass);

	assert(currentThread != 0);

	unhand(currentThread)->name = (HArrayOfChar*)makeJavaCharArray(nm, strlen(nm));
	unhand(currentThread)->priority = java_lang_Thread_NORM_PRIORITY;
	unhand(currentThread)->threadQ = 0;
	unhand(currentThread)->single_step = 0;
	unhand(currentThread)->daemon = 0;
	unhand(currentThread)->stillborn = 0;
	unhand(currentThread)->target = 0;
	unhand(currentThread)->initial_stack_memory = threadStackSize;
	unhand(currentThread)->group = standardGroup;

	THREAD_CREATEFIRST(currentThread);

	currentThreadID = unhand( currentThread )->PrivateInfo;

	gc_set_finalizer(currentThread, &gc_thread);

#ifdef TRANSLATOR
	translator_stack =
	    (void*) ((unsigned int) (TCTX(currentThread)->stackBase) - 96);
#endif

	/* Attach thread to threadGroup */
	do_execute_java_method(0, (Hjava_lang_Object*)unhand(currentThread)->group, "add", "(Ljava/lang/Thread;)V", 0, 0, currentThread);
}

/*
 * Start a daemon thread.
 */
static
Hjava_lang_Thread*
createDaemon(void* func, char* nm)
{
	Hjava_lang_Thread* tid;

DBG(	printf("createDaemon %s\n", nm);				)

	/* Keep daemon threads as root objects */
	tid = (Hjava_lang_Thread*)newObject(ThreadClass);
	assert(tid != 0);

	unhand(tid)->name = (HArrayOfChar*)makeJavaCharArray(nm, strlen(nm));
	unhand(tid)->priority = java_lang_Thread_MAX_PRIORITY;
	unhand(tid)->threadQ = 0;
	unhand(tid)->single_step = 0;
	unhand(tid)->daemon = 1;
	unhand(tid)->stillborn = 0;
	unhand(tid)->target = 0;
	unhand(tid)->initial_stack_memory = threadStackSize;
	unhand(tid)->group = 0;

	THREAD_CREATE(tid, func);

	return (tid);
}

/*
 * All threads start here.
 */
static
void
firstStartThread(void)
{
	THREAD_INIT();

DBG(	printf("firstStartThread %x\n", CURRENTTHREAD());		)

	/* Find the run()V method and call it */
	do_execute_java_method(0, (Hjava_lang_Object*)CURRENTTHREAD(), "run", "()V", 0, 0);
	do_execute_java_method(0, (Hjava_lang_Object*)CURRENTTHREAD(), "exit", "()V", 0, 0);

	THREAD_EXIT();
}

/*
 * Finalize a thread.
 *  This is chiefly to free the thread context.
 */
void
gc_finalize_thread(void* mem)
{
    Hjava_lang_Thread* tid;
    Method* final;

    tid = (Hjava_lang_Thread*)mem;

    THREAD_FREE(tid);

    /* Call thread finalizer if it has one (can it have one?) */
    final = findMethod(OBJECT_CLASS(&tid->base), final_name, void_signature);
    if (final != NULL) {
        CALL_KAFFE_METHOD(METHOD_NATIVECODE(final), &tid->base);
    }

}

/*
 * Yield process to another thread of equal priority.
 */
void
yieldThread()
{
	THREAD_YIELD();
}

/*
 * Put a thread to sleep.
 */
void
sleepThread(jlong time)
{
	THREAD_SLEEP(time);
}

/*
 * Is this thread alive?
 */
// After a thread is stopped, it is not alive. But, previously until it 
// is actually dead, it remains alive. 
// To fix it, another flag THREAD_FLAGS_STOPPED is appended. It is set
// when stopThread is called, and never cleared.
// - walker 1999.10.15
bool
aliveThread(Hjava_lang_Thread* tid)
{
    bool status;

DBG(	printf("aliveThread: tid 0x%x\n", tid);				)

    THREAD_ALIVE(tid, status);
    
    return (status);
}

/*
 * How many stack frames have I invoked?
 */
jint
framesThread(Hjava_lang_Thread* tid)
{
	jint count;

	THREAD_FRAMES(tid, count);

	return (count);
}

/*
 * If we're not using native threads, include some extra thread support.
 */
#if !defined(USE_NATIVE_THREADS)
#include "thread-internal.c"
#endif
