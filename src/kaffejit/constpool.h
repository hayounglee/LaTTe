/* constpool.h
 * Manage the constant pool.
 *
 * Copyright (c) 1997 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Richard Henderson <rth@tamu.edu>, 1997
 */

#ifndef __jit_constpool_h
#define __jit_constpool_h

#define CPint		1
#define CPlong		2
#define CPref		3
#define CPfloat		4
#define CPdouble	5


typedef struct _constpool {
	struct _constpool* next;
	uintp		   at;
	union _constpoolval {
		jint	   i;
		jlong	   l;
		void*	   r;
		float	   f;
		double	   d;
	} val;
} constpool;

#define ALLOCCONSTNR	32

extern constpool* firstConst;
extern constpool* lastConst;
extern constpool* currConst;
extern uint32 nConst;

constpool* newConstant(int type, ...);
void establishConstants(void *at);

#endif
