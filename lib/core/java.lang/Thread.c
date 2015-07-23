/*
 * java.lang.Thread.c
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 *
 * Modified by MASS Laboratory, SNU, 1999
 */

#include "config.h"
#include "config-std.h"
#include "gtypes.h"
#include <native.h>
#include "runtime/thread.h"
#include "runtime/support.h"
#include "runtime/locks.h"

struct Hjava_lang_Thread*
java_lang_Thread_currentThread(void)
{
	return (currentThread);
}

/*
 * Yield processor to another thread of the same priority.
 */
void
java_lang_Thread_yield(void)
{
	yieldThread();
}

/*
 * Put current thread to sleep for a time.
 */
void
java_lang_Thread_sleep(jlong time)
{
	sleepThread(time);
}

/*
 * Start this thread running.
 */
void
java_lang_Thread_start(struct Hjava_lang_Thread* this)
{
	//
	// inserted by doner
	// - This function must be synchronized.
	//

	_lockMutex( (Hjava_lang_Object*)this );

	startThread(this);

	_unlockMutex( (Hjava_lang_Object*)this );
}

/*
 * Is this thread alive?
 */
jbool
java_lang_Thread_isAlive(struct Hjava_lang_Thread* this)
{
	return (aliveThread(this));
}

/*
 * Number of stack.  One for the moment.
 */
jint
java_lang_Thread_countStackFrames(struct Hjava_lang_Thread* this)
{
	return (framesThread(this));
}

/*
 * Change thread priority.
 */
void
java_lang_Thread_setPriority0(struct Hjava_lang_Thread* this, jint prio)
{
	setPriorityThread(this, prio);
}

/*
 * Stop a thread in its tracks.
 */
void
java_lang_Thread_stop0(struct Hjava_lang_Thread* this, struct Hjava_lang_Object* obj)
{
	stopThread(this);
}

void
java_lang_Thread_suspend0(struct Hjava_lang_Thread* this)
{
	suspendThread(this);
}

void
java_lang_Thread_resume0(struct Hjava_lang_Thread* this)
{
	resumeThread(this);
}

jbool
java_lang_Thread_isInterrupted(struct Hjava_lang_Thread* this, jbool val)
{
	unimp("java.lang.Thread:isInterrupted unimplemented");
}

void
java_lang_Thread_interrupt0(struct Hjava_lang_Thread* this)
{
	unimp("java.lang.Thread:interrupt0 unimplemented");
}
