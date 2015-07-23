/* constpool.c
 * Manage the constant pool.
 *
 * Copyright (c) 1997 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Richard Henderson <rth@tamu.edu>, 1997
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "gtypes.h"
#include "constpool.h"
#include "gc.h"

#include <stdarg.h>


constpool* firstConst;
constpool* lastConst;
constpool* currConst;
uint32 nConst;

/*
 * Allocate a new constant.
 */
constpool*
newConstant(int type, ...)
{
	constpool *c;
	union _constpoolval val;
	va_list arg;

	memset(&val, 0, sizeof(val));
	va_start(arg, type);
	switch (type) {
	case CPint:
		val.i = va_arg(arg, jint);
		break;
	case CPlong:
		val.l = va_arg(arg, jlong);
		break;
	case CPref:
		val.r = va_arg(arg, void*);
		break;
	case CPfloat:
		/* Remember ISO rules about argument widening.  */
		val.f = va_arg(arg, double);
		break;
	case CPdouble:
		val.d = va_arg(arg, double);
		break;
	}
	va_end(arg);

	/* Find out if we've allocated this constant (or its bit
	   equivalent) before, and if so, reuse it.  */
	for (c = firstConst; c != currConst; c = c->next) {
		if (memcmp(&c->val, &val, sizeof(val)) == 0)
			return c;
	}

	if (!c) {
		int i;

		/* Allocate chunk of label elements */
		c = gc_calloc_fixed(ALLOCCONSTNR, sizeof(constpool));

		/* Attach to current chain */
		if (lastConst == 0) {
			firstConst = c;
		}
		else {
			lastConst->next = c;
		}
		lastConst = &c[ALLOCCONSTNR-1];

		/* Link elements into list */
		for (i = 0; i < ALLOCCONSTNR-1; i++) {
			c[i].next = &c[i+1];
		}
		c[ALLOCCONSTNR-1].next = NULL;
	}

	c->val = val;
	currConst = c->next;
	nConst++;

	return (c);
}

void
establishConstants(void *at)
{
	constpool *c;

	for (c = firstConst; c != currConst; c = c->next) {
		c->at = (uintp)at;
		*(union _constpoolval*)at = c->val;
		at = (void*)(((uintp)at) + sizeof(c->val));
	}

	currConst = firstConst;
	nConst = 0;
}
