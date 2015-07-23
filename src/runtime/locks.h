/*
 * locks.h
 * Manage locking system.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>, 1996,97.
 */

#ifndef __locks_h
#define __locks_h

#include "md.h"



#define	lockMutex(THING)	_lockMutex((THING))
#define	unlockMutex(THING)	_unlockMutex((THING))
#define	waitCond(THING, TIME)	_waitCond((THING), (TIME))
#define	signalCond(THING)	_signalCond((THING))
#define	broadcastCond(THING)	_broadcastCond((THING))


struct Hjava_lang_Thread;
struct Hjava_lang_Object;

typedef struct Hjava_lang_Object    quickLock;


//
// from "locks-internal.h"
//
extern int blockInts;
extern bool needReschedule;

#define	intsDisable()	blockInts++
#define	intsRestore()	if (blockInts == 1 && needReschedule == true) {	\
				reschedule();				\
			}						\
			blockInts--     

extern void    _lockMutex( struct Hjava_lang_Object* obj );
extern void    _unlockMutex( struct Hjava_lang_Object* obj );
extern void     lockMutexInt(int *);
extern void     unlockMutexInt(int *);

extern void    wait_in_queue( struct Hjava_lang_Object* obj );
extern void    wakeup_waiter( struct Hjava_lang_Object* obj );

extern void    waitCondVar( struct Hjava_lang_Object* obj );

extern int     _waitCond( struct Hjava_lang_Object* obj, jlong timeout );
extern void    _signalCond( struct Hjava_lang_Object* obj );
extern void    _broadcastCond( struct Hjava_lang_Object* obj );

#endif
