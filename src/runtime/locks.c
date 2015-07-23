/*
 * locks.c
 * Manage locking system
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 * Modified by MASS Laboratory, SNU, 1999.
 */

#define	DBG(s)
#define	FDBG(s)

#include "config.h"
#include "config-std.h"
#include "object.h"
#include "baseClasses.h"
#include "thread.h"
#include "locks.h"
#include "errors.h"
#include "exception.h"
#include "md.h"


#define NDBG( s )   
#define CDBG( s )   
#define MDBG( s )   

#ifdef LOCK_TEST
#include "lock_test.h"
#define LDBG( s ) s
#else
#define LDBG( s )
#endif

#ifdef LOCK_OPT
/* Object model changed.
 * Objects don't have waiter_queue and cond_var_queue.
 * Because the number of objects which need waiter_queue or cond_var_queue
 * is small, it's better idea for memory efficiency
 */

#define	MAXLOCK		256
#define	HASHLOCK(a)	((((uintp)(a)) >> 3) & (MAXLOCK-1))


typedef struct _jLock {
	struct _jLock*			next;
	void*     				address;
	struct Hjava_lang_Thread*	waiter_queue;
	struct Hjava_lang_Thread*	cond_var_queue;
} jLock;

static struct lockList {
	jLock*		head;
} lockTable[MAXLOCK];

#if 0
static jLock   lock_pool[MAXLOCK*10];

static
jLock*
alloc_lock()
{
    static int pool_index = 0;

    return &lock_pool[pool_index++];
}
#endif // 0

inline static
jLock*
getLock(void* address)
{
	struct lockList* lockHead;
	jLock* lock;
    int    index = HASHLOCK(address);
	lockHead = &lockTable[index];

	for (lock = lockHead->head; lock != NULL; lock = lock->next) {
		if (lock->address == address) {
			return (lock);
		}
	}

	lock = gc_malloc(sizeof(jLock), &gc_lock);
//    lock = alloc_lock();
	lock->next = lockHead->head;
	lockHead->head = lock;

	/* Fill in the details */
	lock->address = address;
	return (lock);
}

#endif // LOCK_OPT

#define    OWNER_ID_MASK    0xffff
#define    SEMAPHORE_MASK   0xfffe0000
#define    HAS_WAITERS_MASK 0x10000

///
/// Function Name : _lockMutex
/// Author : Yang, Byung-Sun
/// Input
///      obj - object which contains lock information
/// Output
/// Pre-Condition
/// Post-Condition
/// Description
///      Each object contains lock fields that consists of lock and wait queue.
///      A lock is 32 bits long.
///            | semaphore(31:17) | has_waiters(16) | owner_id |
///
void
_lockMutex( struct Hjava_lang_Object* obj )
{
LDBG(  if ((obj->lock_info & SEMAPHORE_MASK) == 0) { // lock is free
            LockInfo.lock1++;
        } else if ((obj->lock_info & OWNER_ID_MASK) == currentThreadID) {
            LockInfo.lock2++;
        } else {
            LockInfo.lock3++;
        } );

    while ( 1 ) {		// spin lock
        NDBG( printf( "lockMutex: lock_info = %x(obj %p)\n", obj->lock_info, obj ); );

        assert(currentThread == NULL ||
               (currentThreadID == unhand( currentThread )->PrivateInfo &&
                TCTX( currentThread )->status == THREAD_RUNNING));

        if ((obj->lock_info & SEMAPHORE_MASK) == 0) { // lock is free
            *(((unsigned short*)&obj->lock_info) + 1) = currentThreadID;
            *((unsigned short*)&(obj->lock_info)) += 0x2; // semaphore++
            break;
        } else if ((obj->lock_info & OWNER_ID_MASK) == currentThreadID) {
            *((unsigned short*)&(obj->lock_info)) += 0x2; // semaphore++
            break;
        } else {
            wait_in_queue( obj );
        }
    }

}

void lockMutexInt( int * ilock ) {
    _lockMutex( (void *)(ilock - 1) );
};


void
_unlockMutex( struct Hjava_lang_Object* obj )
{
    NDBG( printf( "unlockMutex: lock_info = %x(obj %p)\n", obj->lock_info, obj ); );

    assert(currentThread == 0 ||
           (currentThreadID == unhand( currentThread )->PrivateInfo &&
            TCTX( currentThread )->status == THREAD_RUNNING));


#ifdef PRECISE_MONITOREXIT

    if ( *((unsigned short*) &(obj->lock_info)) >= 0x2 ) {
        *((unsigned short*)&(obj->lock_info)) -= 0x2;
    } else {
        throwException( IllegalMonitorStateException );
    }

#else
    //
    // semaphore field is decremented.
    //
    *((unsigned short*)&(obj->lock_info)) -= 0x2;
#endif // PRECISE_MONITOREXIT

    if ( (obj->lock_info >> 16) != 1 ) {	// semaphore != 0 || !has_waiters
//        assert( (obj->lock_info & SEMAPHORE_MASK) != 0 ||obj->waiter_queue == NULL );
        // do nothing
LDBG(if((obj->lock_info & SEMAPHORE_MASK) == 0) LockInfo.unlock1++;
     else  LockInfo.unlock2++;);
        
    } else {
LDBG( LockInfo.unlock3++;);
        wakeup_waiter( obj );
    }

}

void unlockMutexInt( int * ilock ) {
    _unlockMutex( (void *)(ilock - 1) );
};

void
wait_in_queue( struct Hjava_lang_Object* obj )
{
#ifdef LOCK_OPT
    jLock* lock;
#endif

#ifndef NEW_CTX_SWITCH
    intsDisable();
#endif NEW_CTX_SWITCH

    MDBG( printf( "wait_in_queue is called for %p.\tlock_info = %x\n", obj, obj->lock_info ); );


    obj->lock_info |= HAS_WAITERS_MASK;
#ifdef LOCK_OPT

    lock = getLock( obj );
#ifdef NEW_CTX_SWITCH
    suspendOnLockQueue( currentThread, &(lock->waiter_queue) );
#else
    suspendOnQThread( currentThread, &(lock->waiter_queue), NOTIMEOUT, false);
#endif NEW_CTX_SWITCH

#else

#ifdef NEW_CTX_SWITCH
    suspendOnLockQueue( currentThread, &(obj->waiter_queue) );
#else
    suspendOnQThread( currentThread, &(obj->waiter_queue), NOTIMEOUT, false);
#endif NEW_CTX_SWITCH

#endif

#ifndef NEW_CTX_SWITCH
    intsRestore();
#endif NEW_CTX_SWITCH
}



void
wakeup_waiter( struct Hjava_lang_Object* obj )
{
    Hjava_lang_Thread*    thread;
#ifdef LOCK_OPT
    jLock* lock;
#endif

#ifndef NEW_CTX_SWITCH
    intsDisable();
#endif NEW_CTX_SWITCH

    MDBG( printf( "wakeup_waiter is called for %p.\tlock_info = %x\n", obj, obj->lock_info ); );

#ifdef LOCK_OPT
    lock = getLock( obj );
    thread = lock->waiter_queue;
    lock->waiter_queue = TCTX( thread )->nextQ;
    if (lock->waiter_queue == NULL ) {
        obj->lock_info = 0;
    }
#else
    assert( obj->waiter_queue != NULL );
    thread = obj->waiter_queue;
    obj->waiter_queue = TCTX( thread )->nextQ;
    if (obj->waiter_queue == NULL) {
        obj->lock_info = 0;
    }
#endif

    assert( TCTX(thread)->status != THREAD_RUNNING );

#ifndef NEW_CTX_SWITCH
    iresumeThread( thread );

    intsRestore();
#else
    resumeFromLockQueue( thread );
#endif NEW_CTX_SWITCH
}


int
_waitCond( struct Hjava_lang_Object* obj, jlong timeout )
{
    unsigned int     lock_info;

#ifndef NDEBUG
    Hjava_lang_Thread*	curThread = currentThread;
#endif

#ifdef LOCK_OPT
    jLock* lock;
#endif

    CDBG( printf( "waitCond is called for %p.\tlock_info = %x\n", obj, obj->lock_info ); );


    if ((obj->lock_info & OWNER_ID_MASK) != currentThreadID) {
        throwException( IllegalMonitorStateException );
    }

    lock_info = obj->lock_info;

    obj->lock_info &= ~(SEMAPHORE_MASK | OWNER_ID_MASK);	// lock_info.semaphore = 0

    intsDisable();
#ifdef LOCK_OPT
    lock = getLock( obj );

    if ( lock->waiter_queue != NULL ){
         Hjava_lang_Thread*    thread;

        assert( (obj->lock_info & HAS_WAITERS_MASK) != 0 );

        thread = lock->waiter_queue;
        lock->waiter_queue = TCTX( thread )->nextQ;
        assert( TCTX( thread )->status != THREAD_RUNNING );

        if (lock->waiter_queue == NULL) {
            obj->lock_info = 0;
        } else {
            obj->lock_info = 0x00010000;
        }

        iresumeThread( thread );
    }

    suspendOnQThread(currentThread, &lock->cond_var_queue, timeout, true);
    while ((obj->lock_info & SEMAPHORE_MASK) > 0) {
        // If 'object' is locked by another thread, currentThread is blocked 
        // again, and then is inserted to waiter_queue of the 'object'.
        // For consistency with 'lock->waiter_queue', the waiter bit of 
        // 'obj->lock_info' must be set.
        // If this bit is not set, after the thread which has the lock of 
        // object releases the lock, this thread is not awakend.
        obj->lock_info |= HAS_WAITERS_MASK;
        suspendOnQThread( currentThread, &lock->waiter_queue, NOTIMEOUT, true);
    }

    intsRestore();

    assert( curThread == currentThread );

    obj->lock_info = lock_info;

    // after obj->lock_info is restored, the Thread.Death exception
    // can be thrown.
    if ((TCTX(currentThread)->flags & THREAD_FLAGS_KILLED) != 0 
        && blockInts == 0) {
        TCTX(currentThread)->flags &= ~THREAD_FLAGS_KILLED;
        blockInts = 0;
        throwException(ThreadDeath);
        assert("Rescheduling dead thread" == 0);
    }

    if (lock->waiter_queue != NULL) {
        assert( (obj->lock_info & SEMAPHORE_MASK) > 0 );

        obj->lock_info |= HAS_WAITERS_MASK;
    } else {
        obj->lock_info &= ~HAS_WAITERS_MASK;
    }
#else

         
    if (obj->waiter_queue != NULL) {
        Hjava_lang_Thread*    thread;

        assert( (obj->lock_info & HAS_WAITERS_MASK) != 0 );

        thread = obj->waiter_queue;
        obj->waiter_queue = TCTX( thread )->nextQ;
        assert( TCTX( thread )->status != THREAD_RUNNING );

        if (obj->waiter_queue == NULL) {
            obj->lock_info = 0;
        } else {
            obj->lock_info = 0x00010000;
        }

        iresumeThread( thread );
    }

    suspendOnQThread( currentThread, &obj->cond_var_queue, timeout, true);
    while ((obj->lock_info & SEMAPHORE_MASK) > 0) {
        suspendOnQThread( currentThread, &obj->waiter_queue, NOTIMEOUT, true);
    }


    intsRestore();


    assert( curThread == currentThread );

    obj->lock_info = lock_info;

    /* I might be dying */
    if ((TCTX(currentThread)->flags & THREAD_FLAGS_KILLED) != 0 
        && blockInts == 1) {
        TCTX(currentThread)->flags &= ~THREAD_FLAGS_KILLED;
        blockInts = 0;
        throwException(ThreadDeath);
        assert("Rescheduling dead thread" == 0);
    }

    if (obj->waiter_queue != NULL) {
        assert( (obj->lock_info & SEMAPHORE_MASK) > 0 );

        obj->lock_info |= HAS_WAITERS_MASK;
    } else {
        obj->lock_info &= ~HAS_WAITERS_MASK;
    }
#endif

    return 0;
}


void
_signalCond( struct Hjava_lang_Object* obj )
{
#ifdef LOCK_OPT
    jLock* lock;
#endif

    CDBG( printf( "signalCond is called for %p.\tlock_info = %x\n", obj, obj->lock_info ); );

    if ((obj->lock_info & OWNER_ID_MASK) != currentThreadID) {
        throwException( IllegalMonitorStateException );
    }

    intsDisable();
#ifdef LOCK_OPT

    lock = getLock( obj );

    if (lock->cond_var_queue != NULL) {
        Hjava_lang_Thread*   thread;

        thread = lock->cond_var_queue;
        lock->cond_var_queue = TCTX( thread )->nextQ;
        TCTX( thread )->nextQ = lock->waiter_queue;
        lock->waiter_queue = thread;

        assert( lock->waiter_queue != NULL );
        obj->lock_info |= HAS_WAITERS_MASK;
    }
#else
    if (obj->cond_var_queue != NULL) {
        Hjava_lang_Thread*   thread;

        thread = obj->cond_var_queue;
        obj->cond_var_queue = TCTX( thread )->nextQ;
        TCTX( thread )->nextQ = obj->waiter_queue;
        obj->waiter_queue = thread;

        assert( obj->waiter_queue != NULL );
        obj->lock_info |= HAS_WAITERS_MASK;
    }
#endif

    intsRestore();

}


void
_broadcastCond( Hjava_lang_Object* obj )
{
#ifdef LOCK_OPT
    jLock* lock;
#endif
   CDBG( printf( "broadcastCond is called for %p.\tlock_info = %x\n", obj, obj->lock_info ); );


    if ((obj->lock_info & OWNER_ID_MASK) != currentThreadID) {
        throwException( IllegalMonitorStateException );
    }

    intsDisable();
#ifdef LOCK_OPT
    lock = getLock( obj );

    if (lock->cond_var_queue != NULL) {
        Hjava_lang_Thread**    pthread;

        for (pthread = &lock->cond_var_queue; *pthread != NULL;
             pthread = &(TCTX( *pthread )->nextQ)) {
            // do nothing
        }

        (*pthread) = lock->waiter_queue;

        lock->waiter_queue = lock->cond_var_queue;
        lock->cond_var_queue = NULL;

        assert( lock->waiter_queue != NULL );
        obj->lock_info |= HAS_WAITERS_MASK;
    }
#else

    if (obj->cond_var_queue != NULL) {
        Hjava_lang_Thread**    pthread;

        for (pthread = &obj->cond_var_queue; *pthread != NULL;
             pthread = &(TCTX( *pthread )->nextQ)) {
            // do nothing
        }

        (*pthread) = obj->waiter_queue;

        obj->waiter_queue = obj->cond_var_queue;
        obj->cond_var_queue = NULL;

        assert( obj->waiter_queue != NULL );
        obj->lock_info |= HAS_WAITERS_MASK;
    }
#endif

    intsRestore();

}

