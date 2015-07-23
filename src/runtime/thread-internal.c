/*
 * thread-internal.c
 * Internal threading system support
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 * Modified by MASS Laboratory, SNU, 1999.
 */

#include "exception_handler.h"

Hjava_lang_Thread* threadQhead[java_lang_Thread_MAX_PRIORITY + 1];
Hjava_lang_Thread* threadQtail[java_lang_Thread_MAX_PRIORITY + 1];

static int maxFd = -1;
static fd_set readsPending;
static fd_set writesPending;
static Hjava_lang_Thread* readQ[FD_SETSIZE];
static Hjava_lang_Thread* writeQ[FD_SETSIZE];
static struct timeval zerotimeout = { 0, 0 };
static bool alarmBlocked;

#ifdef INTERPRETER
uint8* jstack_point; /* Top of Java interpreter stack. */
#endif

Hjava_lang_Thread* liveThreads;
Hjava_lang_Thread* alarmList;

int blockInts;
bool needReschedule;

static void addToAlarmQ(Hjava_lang_Thread*, jlong);
static void removeFromAlarmQ(Hjava_lang_Thread*);
static void checkEvents(void);
void reschedule(void);
void reschedule_from_wait(void);
ctx* newThreadCtx(int);

extern int flag_call_stat;

#define NDBG( s )
#define CDBG( s )   
#define THDBG( s )

CDBG(
void print_risc_count();
)
CDBG(
extern unsigned long long   num_of_executed_risc_instrs;
);


#ifdef NEW_CTX_SWITCH
//
// The number of priority level is now assumed 8.
// The lowest priority is 0 and the highest priority is 7.
// The normal priority is 4.
//
#define  PRIORITY_LEVELS     8

static unsigned char   priority_bit_map[] = { 0x1,
                                              0x2,
                                              0x4,
                                              0x8,
                                              0x10,
                                              0x20,
                                              0x40,
                                              0x80
};

static int   num_of_threads[PRIORITY_LEVELS];

unsigned char   used_priority;

static int   highest_priority_map[] = {
    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

inline static
void
add_ready_thread( int priority )
{
    assert( priority >= java_lang_Thread_MIN_PRIORITY &&
            priority <= java_lang_Thread_MAX_PRIORITY );

    num_of_threads[priority]++;
    used_priority |= priority_bit_map[priority];
}

inline static
void
remove_ready_thread( int priority )
{
    assert( priority >= java_lang_Thread_MIN_PRIORITY &&
            priority <= java_lang_Thread_MAX_PRIORITY );
    assert( num_of_threads[priority] > 0 );

    num_of_threads[priority]--;
    if (num_of_threads[priority] == 0) {
        used_priority &= ~priority_bit_map[priority];
    }
}

inline static
int
get_highest_priority()
{
    return highest_priority_map[used_priority];
}

inline static
int
is_priority_used( int priority )
{
    return used_priority & priority_bit_map[priority];
}
#endif NEW_CTX_SWITCH


#define MAXTCTX         128
ctx** threadContext;

/* Select an alarm system */
#if defined(HAVE_SETITIMER) && defined(ITIMER_REAL)
#define	MALARM(_mt)							\
	{								\
		struct itimerval tm;					\
		tm.it_interval.tv_sec = 0;				\
		tm.it_interval.tv_usec = 0;				\
		tm.it_value.tv_sec = (_mt) / 1000;			\
		tm.it_value.tv_usec = ((_mt) % 1000) * 1000;		\
		setitimer(ITIMER_REAL, &tm, 0);				\
	}
#elif defined(HAVE_ALARM)
#define	MALARM(_mt)	alarm((int)(((_mt) + 999) / 1000))
#endif

/* Select call is indirect so we can change if when embedding Kaffe into
 * something else.
 */
extern int select();
int (*select_call)(int, fd_set*, fd_set*, fd_set*, struct timeval*) = select;

/*
 * Explicit request by user to resume a thread
 * The definition says that it is just legal to call this after a preceeding
 * suspend (which got through). If the thread was blocked for some other
 * reason (either sleep or IO or a muxSem), we simply can't do it
 * We use a new thread flag THREAD_FLAGS_USERSUSPEND for this purpose
 * (which is set by suspendThread(.))
 */
void
resumeThread(Hjava_lang_Thread* tid)
{
	intsDisable();

        if ((TCTX(tid)->flags & THREAD_FLAGS_USERSUSPEND) != 0) {
                TCTX(tid)->flags &= ~THREAD_FLAGS_USERSUSPEND;
                iresumeThread(tid);
        }
        if ((TCTX(tid)->flags & THREAD_FLAGS_KILLED) != 0) {
		iresumeThread(tid);
        }

	intsRestore();
}


#ifdef NEW_CTX_SWITCH
void
resumeFromLockQueue(Hjava_lang_Thread* tid)
{
	Hjava_lang_Thread** ntid;
    int    priority = TCTX(tid)->priority;

	intsDisable();

    TCTX(tid)->status = THREAD_RUNNING;

    add_ready_thread( TCTX(tid)->priority );

    /* Place thread on the end of its queue */
    if (threadQhead[priority] == 0) {
        threadQhead[priority] = tid;
        threadQtail[priority] = tid;
        if (priority > TCTX(currentThread)->priority) {
            needReschedule = true;
        }
    }
    else {
        TCTX(threadQtail[priority])->nextQ = tid;
        threadQtail[priority] = tid;
    }
    TCTX(tid)->nextQ = 0;

	intsRestore();
}

#endif NEW_CTX_SWITCH

/*
 * Resume a thread running.
 * This routine has to be called only from locations which ensure
 * run / block queue consistency. There is no check for illegal resume
 * conditions (like explicitly resuming an IO blocked thread). There also
 * is no update of any blocking queue. Both has to be done by the caller
 */
void
iresumeThread(Hjava_lang_Thread* tid)
{
	Hjava_lang_Thread** ntid;

DBG(	printf("resumeThread %x\n", tid);			)
	intsDisable();

	if (TCTX(tid)->status != THREAD_RUNNING) {

		/* Remove from alarmQ if necessary */
		if ((TCTX(tid)->flags & THREAD_FLAGS_ALARM) != 0) {
			removeFromAlarmQ(tid);
		}
		/* Remove from lockQ if necessary */
		if (TCTX(tid)->blockqueue != 0) {
			for (ntid = TCTX(tid)->blockqueue; *ntid != 0; ntid = &TCTX(*ntid)->nextQ) {
				if (*ntid == tid) {
					*ntid = TCTX(tid)->nextQ;
					break;
				}
			}
			TCTX(tid)->blockqueue = 0;
		}

		TCTX(tid)->status = THREAD_RUNNING;

#ifdef NEW_CTX_SWITCH
        add_ready_thread( TCTX(tid)->priority );
#endif NEW_CTX_SWITCH

		/* Place thread on the end of its queue */
		if (threadQhead[TCTX(tid)->priority] == 0) {
			threadQhead[TCTX(tid)->priority] = tid;
			threadQtail[TCTX(tid)->priority] = tid;
			if (TCTX(tid)->priority > TCTX(currentThread)->priority) {
				needReschedule = true;
			}
		}
		else {
			TCTX(threadQtail[TCTX(tid)->priority])->nextQ = tid;
			threadQtail[TCTX(tid)->priority] = tid;
		}
		TCTX(tid)->nextQ = 0;
	}
SDBG(	else {
		printf("Re-resuming 0x%x\n", tid);
	}							)
	intsRestore();
}


/*
 * Suspend a thread.
 * This is an explicit user request to suspend the thread - the counterpart
 * for resumeThreadRequest(.). It is JUST called by the java method
 * Thread.suspend()
 * What makes it distinct is the fact that the suspended thread is not contained
 * in any block queue. Without a special flag (indicating the user suspend), we
 * can't check s suspended thread for this condition afterwards (which is
 * required by resumeThreadRequest()). The new thread flag
 * THREAD_FLAGS_USERSUSPEND is used for this purpose.
 */
void
suspendThread(Hjava_lang_Thread* tid)
{
	Hjava_lang_Thread** ntid;

	intsDisable();

	if (TCTX(tid)->status != THREAD_SUSPENDED) {
		TCTX(tid)->status = THREAD_SUSPENDED;

		/*
                 * This is used to indicate the explicit suspend condition
                 * required by resumeThreadRequest()
                 */
                TCTX(tid)->flags |= THREAD_FLAGS_USERSUSPEND;

		for (ntid = &threadQhead[TCTX(tid)->priority]; *ntid != 0; ntid = &TCTX(*ntid)->nextQ) {
			if (*ntid == tid) {
#ifdef NEW_CTX_SWITCH
                assert( is_priority_used( TCTX(tid)->priority ) );
                remove_ready_thread( TCTX(tid)->priority );
#endif NEW_CTX_SWITCH
				*ntid = TCTX(tid)->nextQ;
				TCTX(tid)->nextQ = 0;
				if (tid == currentThread) {
					reschedule();
				}
				break;
			}
		}
	}
SDBG(	else {
		printf("Re-suspending 0x%x\n", tid);
	}							)

	intsRestore();
}

#ifdef NEW_CTX_SWITCH
void
suspendOnLockQueue( Hjava_lang_Thread*  tid,
                    Hjava_lang_Thread** lock_queue )
{
    Hjava_lang_Thread** ntid;
    int    priority = TCTX(tid)->priority;

    TCTX(tid)->status = THREAD_SUSPENDED;

    //
    // remove the current thread from the ready queue
    // and insert the lock queue
    //
    remove_ready_thread( priority );
    threadQhead[priority] = TCTX(tid)->nextQ;
    if (threadQhead[priority] == NULL) {
        threadQtail[priority] = NULL;
    }

    TCTX(tid)->nextQ = *lock_queue;
    *lock_queue = tid;
    TCTX(tid)->blockqueue = lock_queue;

    reschedule();
}
#endif NEW_CTX_SWITCH

/*
 * Suspend a thread on a queue.
 */
// bug fix by walker - 1999.10.15
// I have fixed a bug in CatchDeath (in Kaffe testcase), but this solution
// seems incomplete. Someday, we have to implement the whole thread
// scheduling system.
//
// If a thread is stopped by other thread, as soon as it is resumed,
// Thread.Death exception is thrown.  However, it is not correct in the 
// following situation.
// 
// -> thread T acquires a lock of obj
// -> T calls wait method of obj (then it releases the lock of obj)
//    synchronized(obj) {
//        obj.wait();
//    }
// -> another thread T' acquires a lock of obj
// -> T' stops thread T
//    synchronized(obj) {
//        T.stop();
//    }
// -> previously, Thread.Death exception is thrown without re-acquiring
//    the lock of obj. So, IllegalMonitorStateException is thrown.
//    To fix it, I have added an argument to suspendOnQThread and duplicate
//    reschedule function. When suspendOnQThread is called from waitCond,
//    different reschedule function which does not throw Thread.Death exception
//    is called. In that case, Thread.Death is called in waitCond function.
//

void
suspendOnQThread(Hjava_lang_Thread* tid, 
                 Hjava_lang_Thread** queue, 
                 jlong timeout,
                 bool from_wait)
{
    Hjava_lang_Thread** ntid;
    Hjava_lang_Thread* last;
        
DBG(	printf("suspendOnQThread %x %x (%d)\n", tid, queue, (int)timeout); )

#ifndef NEW_CTX_SWITCH
    assert(blockInts > 0);
#endif NEW_CTX_SWITCH

    if (TCTX(tid)->status != THREAD_SUSPENDED) {
        TCTX(tid)->status = THREAD_SUSPENDED;

        last = 0;
        for (ntid = &threadQhead[TCTX(tid)->priority]; 
             *ntid != 0; ntid = &TCTX(*ntid)->nextQ) {
            if (*ntid == tid) {
#ifdef NEW_CTX_SWITCH
                assert( is_priority_used( TCTX(tid)->priority ) );
                remove_ready_thread( TCTX(tid)->priority );
#endif NEW_CTX_SWITCH

                /* Remove thread from runq */
                *ntid = TCTX(tid)->nextQ;
                if (*ntid == 0) {
                    threadQtail[TCTX(tid)->priority] = last;
                }

                /* Insert onto head of lock wait Q */
                if (queue != 0) {
                    TCTX(tid)->nextQ = *queue;
                    *queue = tid;
                    TCTX(tid)->blockqueue = queue;
                }

                /* If I have a timeout, insert into alarmq */
                if (timeout > NOTIMEOUT) {
                    addToAlarmQ(tid, timeout);
                }

                /* If I was running, reschedule */
                if (tid == currentThread) {
                    if (from_wait)
                      reschedule_from_wait();
                    else
                      reschedule();
                }
                break;
            }
            last = *ntid;
        }
    }
SDBG(	else {
		printf("Re-suspending 0x%x on %x\n", tid, *queue);
	}							)
}


/*
 * Kill thread.
 */
void
killThread(void)
{
    Hjava_lang_Thread** ntid;

    intsDisable();

    /* Notify on the object just in case anyone is waiting */
    lockMutex(&currentThread->base);
    broadcastCond(&currentThread->base);
    unlockMutex(&currentThread->base);

DBG(	printf("killThread %x\n", currentThread);			)

    if (TCTX(currentThread)->status != THREAD_DEAD) {

        /* Get thread off runq (if it needs it) */
        if (TCTX(currentThread)->status == THREAD_RUNNING) {
            for (ntid = &threadQhead[TCTX(currentThread)->priority]; 
                 *ntid != 0; ntid = &TCTX(*ntid)->nextQ) {
                if (*ntid == currentThread) {
#ifdef NEW_CTX_SWITCH
                    assert( is_priority_used( TCTX(currentThread)->priority ) );
                    remove_ready_thread( TCTX(currentThread)->priority );
#endif NEW_CTX_SWITCH

                    *ntid = TCTX(currentThread)->nextQ;
                    break;
                }
            }
        }

        talive--;
        if (unhand(currentThread)->daemon) {
            tdaemon--;
        }

        /* If we only have daemons left, then everyone is dead. */
        if (talive == tdaemon) {
            /* Am I suppose to close things down nicely ?? */
            CDBG(
                print_risc_count();
                );

            EXIT(0);
        }

        /* Remove thread from live list so it can be garbaged */
        for (ntid = &liveThreads; *ntid != 0; ntid = &TCTX(*ntid)->nextlive) {
            if (currentThread == (*ntid)) {
                (*ntid) = TCTX(currentThread)->nextlive;
THDBG( fprintf(stderr, "Thread %s privateinfo %d is killed", 
               unhand(currentThread)->name->data, unhand(currentThread)->PrivateInfo); )
                 break;
            }
        }

        /* Remove thread from thread group */
        if (unhand(currentThread)->group != NULL) {
            do_execute_java_method(0, (Hjava_lang_Object*)unhand(currentThread)->group, "remove", "(Ljava/lang/Thread;)V", 0, 0, currentThread);
        }
		
        /* Run something else */
        needReschedule = true;
        blockInts = 1;

        /* Dead Jim - let the GC pick up the remains */
        TCTX(currentThread)->status = THREAD_DEAD;
    }

    intsRestore();
}

/*
 * Change thread priority.
 */
void
setPriorityThread(Hjava_lang_Thread* tid, int prio)
{
	Hjava_lang_Thread** ntid;
	Hjava_lang_Thread* last;

	if (unhand(tid)->PrivateInfo == 0) {
		unhand(tid)->priority = prio;
		return;
	}

	if (TCTX(tid)->status == THREAD_SUSPENDED) {
		TCTX(tid)->priority = (uint8)prio;
		return;
	}

	intsDisable();

	/* Remove from current thread list */
	last = 0;
	for (ntid = &threadQhead[TCTX(tid)->priority]; *ntid != 0; ntid = &TCTX(*ntid)->nextQ) {
		if (*ntid == tid) {
#ifdef NEW_CTX_SWITCH
            assert( is_priority_used( TCTX(tid)->priority ) );
            remove_ready_thread( TCTX(tid)->priority );
#endif NEW_CTX_SWITCH
			*ntid = TCTX(tid)->nextQ;
			if (*ntid == 0) {
				threadQtail[TCTX(tid)->priority] = last;
			}
			break;
		}
		last = *ntid;
	}

	/* Insert onto a new one */
	unhand(tid)->priority = prio;
	TCTX(tid)->priority = (uint8)unhand(tid)->priority;
#ifdef NEW_CTX_SWITCH
    add_ready_thread( prio );
#endif NEW_CTX_SWITCH

	if (threadQhead[prio] == 0) {
		threadQhead[prio] = tid;
		threadQtail[prio] = tid;
	}
	else {
		TCTX(threadQtail[prio])->nextQ = tid;
		threadQtail[prio] = tid;
	}
	TCTX(tid)->nextQ = 0;

	/* If I was reschedulerd, or something of greater priority was,
	 * insist on a reschedule.
	 */
	if (tid == currentThread || prio > TCTX(currentThread)->priority) {
		needReschedule = true;
	}

	intsRestore();
}

static
void
addToAlarmQ(Hjava_lang_Thread* tid, jlong timeout)
{
	Hjava_lang_Thread** tidp;

	assert(blockInts > 0);

	TCTX(tid)->flags |= THREAD_FLAGS_ALARM;

	/* Get absolute time */
	TCTX(tid)->time = timeout + currentTime();

	/* Find place in alarm list and insert it */
	for (tidp = &alarmList; (*tidp) != 0; tidp = &TCTX(*tidp)->nextalarm) {
		if (TCTX(*tidp)->time > TCTX(tid)->time) {
			break;
		}
	}
	TCTX(tid)->nextalarm = *tidp;
	*tidp = tid;

	/* If I'm head of alarm list, restart alarm */
	if (tidp == &alarmList) {
		MALARM(timeout);
	}
}

static
void
removeFromAlarmQ(Hjava_lang_Thread* tid)
{
	Hjava_lang_Thread** tidp;

	assert(blockInts >= 1);

	TCTX(tid)->flags &= ~THREAD_FLAGS_ALARM;

	/* Find thread in alarm list and remove it */
	for (tidp = &alarmList; (*tidp) != 0; tidp = &TCTX(*tidp)->nextalarm) {
		if ((*tidp) == tid) {
			(*tidp) = TCTX(tid)->nextalarm;
			TCTX(tid)->nextalarm = 0;
			break;
		}
	}
}

/*
 * Handle alarm.
 * This routine uses a different meaning of "blockInts". Formerly, it was just
 * "don't reschedule if you don't have to". Now it is "don't do ANY
 * rescheduling actions due to an expired timer". An alternative would be to
 * block SIGALARM during critical sections (by means of sigprocmask). But
 * this would be required quite often (for every outmost intsDisable(),
 * intsRestore()) and therefore would be much more expensive than just
 * setting an int flag which - sometimes - might cause an additional
 * setitimer call.
 */
static
void
alarmException(int sig)
{
	Hjava_lang_Thread* tid;
	jlong time;

	/* Re-enable signal - necessary for SysV */
	catchSignal(sig, alarmException);

	intsDisable();

	/*
	 * If ints are blocked, this might indicate an inconsistent state of
	 * one of the thread queues (either alarmList or threadQhead/tail).
	 * We better don't touch one of them in this case and come back later.
	 */
	if (blockInts > 1) {
		MALARM(50);
		intsRestore();
		return;
	}

	/* Wake all the threads which need waking */
	time = currentTime();
	while (alarmList != 0 && TCTX(alarmList)->time <= time) {
		/* Restart thread - this will tidy up the alarm and blocked
		 * queues.
		 */
		tid = alarmList;
		alarmList = TCTX(alarmList)->nextalarm;
		iresumeThread(tid);
	}

	/* Restart alarm */
	if (alarmList != 0) {
		MALARM(TCTX(alarmList)->time - time);
	}

	/*
	 * The next bit is rather tricky.  If we don't reschedule then things
	 * are fine, we exit this handler and everything continues correctly.
	 * On the otherhand, if we do reschedule, we will schedule the new
	 * thread with alarms blocked which is wrong.  However, we cannot
	 * unblock them here incase we have just set an alarm which goes
	 * off before the reschedule takes place (and we enter this routine
	 * recusively which isn't good).  So, we set a flag indicating alarms
	 * are blocked, and allow the rescheduler to unblock the alarm signal
	 * after the context switch has been made.  At this point it's safe.
	 */
	alarmBlocked = true;
	intsRestore();
	alarmBlocked = false;
}

/*
 * Reschedule the thread.
 * Called whenever a change in the running thread is required.
 */

#ifdef NEW_CTX_SWITCH

void
reschedule(void)
{
    Hjava_lang_Thread* lastThread;
    int b;
    sigset_t nsig;
    int    highest_priority = get_highest_priority();

    if (highest_priority != -1) {
        if (threadQhead[highest_priority] != currentThread) {
            lastThread = currentThread;
            currentThread = threadQhead[highest_priority];
            currentThreadID = unhand( currentThread )->PrivateInfo;
            b = blockInts;

            THREADSWITCH(TCTX(currentThread), TCTX(lastThread));
            /* Alarm signal may be blocked - if so
            * unblock it.
            */
            if (alarmBlocked == true) {
                alarmBlocked = false;
                sigemptyset(&nsig);
                sigaddset(&nsig, SIGALRM);
                sigprocmask(SIG_UNBLOCK, &nsig, 0);
            }

            /* Restore ints */
            blockInts = b;

            /* I might be dying */
            if ((TCTX(lastThread)->flags & THREAD_FLAGS_KILLED) != 0 && blockInts == 1) {
                TCTX(lastThread)->flags &= ~THREAD_FLAGS_KILLED;
                blockInts = 0;
                throwException(ThreadDeath);
                assert("Rescheduling dead thread" == 0);
            }
        }
        /* Now kill the schedule */
        needReschedule = false;
    } else {
        /* Nothing to run - wait for external event */
        checkEvents();
    }
}

#else

void
reschedule(void)
{
    int i;
    Hjava_lang_Thread* lastThread;
    int b;
    sigset_t nsig;

    /* A reschedule in a non-blocked context is half way to hell */
//	assert(blockInts > 0);
    b = blockInts;

    for (;;) {
        for (i = java_lang_Thread_MAX_PRIORITY; 
             i >= java_lang_Thread_MIN_PRIORITY; i--) {
            if (threadQhead[i] != 0) {
                if (threadQhead[i] != currentThread) {
                    lastThread = currentThread;
                    currentThread = threadQhead[i];

                    currentThreadID =
                        unhand( currentThread )->PrivateInfo;
                    THREADSWITCH(TCTX(currentThread), TCTX(lastThread));
                    /* Alarm signal may be blocked - if so
                     * unblock it.
                     */
                    if (alarmBlocked == true) {
                        alarmBlocked = false;
                        sigemptyset(&nsig);
                        sigaddset(&nsig, SIGALRM);
                        sigprocmask(SIG_UNBLOCK, &nsig, 0);
                    }

                    /* Restore ints */
                    blockInts = b;

                    /* I might be dying */
                    if ((TCTX(lastThread)->flags & THREAD_FLAGS_KILLED) != 0 
                        && blockInts == 1) {
                        TCTX(lastThread)->flags &= ~THREAD_FLAGS_KILLED;
                        blockInts = 0;
                        throwException(ThreadDeath);
                        assert("Rescheduling dead thread" == 0);
                    }
                }
                /* Now kill the schedule */
                needReschedule = false;
                return;
            }
        }
        /* Nothing to run - wait for external event */
        checkEvents();
    }
}

void
reschedule_from_wait(void)
{
    int i;
    Hjava_lang_Thread* lastThread;
    int b;
    sigset_t nsig;

    /* A reschedule in a non-blocked context is half way to hell */
//	assert(blockInts > 0);
    b = blockInts;

    for (;;) {
        for (i = java_lang_Thread_MAX_PRIORITY; 
             i >= java_lang_Thread_MIN_PRIORITY; i--) {
            if (threadQhead[i] != 0) {
                if (threadQhead[i] != currentThread) {
                    lastThread = currentThread;
                    currentThread = threadQhead[i];

                    currentThreadID =
                        unhand( currentThread )->PrivateInfo;
                    THREADSWITCH(TCTX(currentThread), TCTX(lastThread));
                    /* Alarm signal may be blocked - if so
                     * unblock it.
                     */
                    if (alarmBlocked == true) {
                        alarmBlocked = false;
                        sigemptyset(&nsig);
                        sigaddset(&nsig, SIGALRM);
                        sigprocmask(SIG_UNBLOCK, &nsig, 0);
                    }

                    /* Restore ints */
                    blockInts = b;
                }
                /* Now kill the schedule */
                needReschedule = false;
                return;
            }
        }
        /* Nothing to run - wait for external event */
        checkEvents();
    }
}

#endif NEW_CTX_SWITCH


/*
 * Wait for some file descriptor or other event to become ready.
 */
static
void
checkEvents(void)
{
	int r;
	fd_set rd;
	fd_set wr;
	Hjava_lang_Thread* tid;
	Hjava_lang_Thread* ntid;
	int i;
	int b;

DBG(	printf("checkEvents\n");					)

	FD_COPY(&readsPending, &rd);
	FD_COPY(&writesPending, &wr);

	/*
	 * Select() is called with indefinite wait, but we have to make sure
	 * we can get interrupted by timer events.  However, we should *NOT*
	 * reschedule.
	 */
	needReschedule = false;
	b = blockInts;
	blockInts = 0;
	r = (*select_call)(maxFd+1, &rd, &wr, 0, 0);
	blockInts = b;

	/* If select get's interrupted, just return now */
	if (r < 0 && errno == EINTR) {
		return;
	}

	/* We must be holding off interrupts before we start playing with
	 * the read and write queues.  This should be already done but a
	 * quick check never hurt anyone.
	 */
	assert(blockInts > 0);

	/* On a select error, mark all threads in error and release
	 * them all.  They should try to re-select and pick up their
	 * own individual errors.
	 */
	if (r < 0) {
		for (i = 0; i <= maxFd; i++) {
			for (tid = readQ[i]; tid != 0; tid = ntid) {
				ntid = TCTX(tid)->nextQ;
				TCTX(tid)->flags |= THREAD_FLAGS_ERROR;
				iresumeThread(tid);
			}
			for (tid = writeQ[i]; tid != 0; tid = ntid) {
				ntid = TCTX(tid)->nextQ;
				TCTX(tid)->flags |= THREAD_FLAGS_ERROR;
				iresumeThread(tid);
			}
			writeQ[i] = 0;
			readQ[i] = 0;
		}
		return;
	}

DBG(	printf("Select returns %d\n", r);				)

	for (i = 0; r > 0 && i <= maxFd; i++) {
		if (readQ[i] != 0 && FD_ISSET(i, &rd)) {
			for (tid = readQ[i]; tid != 0; tid = ntid) {
				ntid = TCTX(tid)->nextQ;
				iresumeThread(tid);
			}
			readQ[i] = 0;
			r--;
		}
		if (writeQ[i] != 0 && FD_ISSET(i, &wr)) {
			for (tid = writeQ[i]; tid != 0; tid = ntid) {
				ntid = TCTX(tid)->nextQ;
				iresumeThread(tid);
			}
			writeQ[i] = 0;
			r--;
		}
	}
}

/*
 * An attempt to access a file would block, so suspend the thread until
 * it will happen.
 */
int
blockOnFile(int fd, int op)
{
	fd_set fset;
	int r;

DBG(	printf("blockOnFile()\n");					)

	/* Trap obviously invalid file descriptors */
	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}

	intsDisable();

	retry:

	/* First a quick check to see if the file handle is usable.
	 * This saves going through all that queuing stuff.
	 */
	FD_ZERO(&fset);
	FD_SET(fd, &fset);
	r = (*select_call)(fd+1, (op == TH_READ ? &fset : 0), (op == TH_WRITE ? &fset : 0), 0, &zerotimeout);

	/* Select got interrupted - do it again */
	if (r < 0 && errno == EINTR) {
		goto retry;
	}
	/* If r != 0 then either its and error and we should return it, or the
	 * file is okay to use so we should use it.  Either way, return now.
	 */
	if (r != 0) {
		intsRestore();
		assert(blockInts == 0);  
		return (r);
	}

	if (fd > maxFd) {
		maxFd = fd;
	}
	if (op == TH_READ) {
		FD_SET(fd, &readsPending);
		suspendOnQThread(currentThread, &readQ[fd], NOTIMEOUT, false);
		FD_CLR(fd, &readsPending);
	}
	else {
		FD_SET(fd, &writesPending);
		suspendOnQThread(currentThread, &writeQ[fd], NOTIMEOUT, false);
		FD_CLR(fd, &writesPending);
	}

	/* If we have an error flagged, retry the whole thing. */
	if ((TCTX(currentThread)->flags & THREAD_FLAGS_ERROR) != 0) {
		TCTX(currentThread)->flags &= ~THREAD_FLAGS_ERROR;
		goto retry;
	}

	intsRestore();
	assert(blockInts == 0);

	return (1);
}

static
void
allocThreadCtx(Hjava_lang_Thread* tid, int stackSize)
{
	static int maxNumberOfThreads = 0;
	static int ntid = 0;
	void* mem;

	/* If we run out of context slots, allocate some more */
	if (numberOfThreads >= maxNumberOfThreads-1) {
		mem = gc_calloc_fixed(maxNumberOfThreads+MAXTCTX, sizeof(ctx*));
//		mem = calloc(maxNumberOfThreads+MAXTCTX, sizeof(ctx*));
		memcpy(mem, threadContext, maxNumberOfThreads * sizeof(ctx*));
		gc_free_fixed(threadContext);
//		free(threadContext);
		threadContext = mem;
		maxNumberOfThreads += MAXTCTX;
	}

	for (;;) {
		ntid++;
		if (ntid == maxNumberOfThreads) {
			ntid = 1;
		}
		if (threadContext[ntid] == 0) {
			mem = newThreadCtx(stackSize);
			GC_WRITE(threadContext, mem);
			threadContext[ntid] = mem;
			threadContext[ntid]->status = THREAD_SUSPENDED;
			numberOfThreads++;
			unhand(tid)->PrivateInfo = ntid;
			return;
		}
	}
}

/*
 * Allocate a new thread context and stack.
 */
ctx*
newThreadCtx(int stackSize)
{
	/* A dummy native exception handler that can't catch anything.
	   Acts as a sentinel for the exception handling system.  This
	   can be shared since native exception handlers being newly
	   registered do not mutate previous handlers.

	   Because of the STATE field, a jmp_buf whose type I really
	   don't want to know, it's a little hard to statically
	   initialize this explicitly.  Fortunately, the only
	   important thing here is that the START and END fields must
	   be equal, which happens automatically. */
	static struct _EH_native_handler dummy_native_handler;

	ctx *ct;

#ifdef INTERPRETER
	ct = gc_calloc_fixed(1, sizeof(ctx) + stackSize + threadStackSize);
#else
	ct = gc_calloc_fixed(1, sizeof(ctx) + stackSize);
#endif

	/* Set list of native exception handlers. */
	ct->native_handlers = &dummy_native_handler;

	/* Allocate native stack. */
	ct->stackBase = (uint8*)(ct + 1);
	ct->stackEnd = ct->stackBase + stackSize;
	ct->restorePoint = ct->stackEnd;

#ifdef INTERPRETER
	/* Allocate Java interpreter stack. */
	ct->jstackBase = (uint8*)(ct + 1) + stackSize;
	ct->jstackEnd = ct->jstackBase + threadStackSize;
	ct->jrestorePoint = ct->jstackEnd;
#endif /* INTERPRETER */

	return (ct);
}
