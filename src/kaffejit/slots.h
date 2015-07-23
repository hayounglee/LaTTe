/* slots.h
 * Slots.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>
 */

#ifndef __slots_h
#define __slots_h

#define	Tcomplex		0
#define	Tnull			Tcomplex
#define	Tconst			1
#define	Tcopy			2
#define	Taddregconst		3
#define	Tstore			4
#define	Tload			5

#define	NOSLOT			0

#define	MAXTEMPS		16

/* Return slots */
#define	returnInt		0
#define	returnRef		0
#define	returnLong		0 /* 1 */
#define	returnFloat		2
#define	returnDouble		2 /* 3 */

/* Compile-time information about a slot. */

struct SlotInfo {
	/* Union must be first item of SlotInfo */
	union {
		jint		tint;
		jlong		tlong;
		jdouble		tdouble;
		void*		taddr;
		char*		tstr;
	} v;

	struct _sequence*	insn;

	/* If regno != NOREG, then the register that currently contains it.
	 * If regno == NOREG, then the value is on the stack,
	 * in the slot's home location (SLOT2FRAMEOFFSET). */
	int			regno;
	int			modified;
};
typedef struct SlotInfo SlotInfo;

extern SlotInfo* slotinfo;
extern SlotInfo* localinfo;
extern SlotInfo* tempinfo;
extern int tmpslot;
extern int stackno;
extern int maxslot;

void initSlots(int);

/* Slot access macros */

#define	stack(_s)		(&localinfo[stackno+(_s)])
#define	rstack(_s)		stack(_s)
#define	wstack(_s)		stack(_s)
#define	local(_s)		(&localinfo[(_s)])
#define	local_long(_s)		(&localinfo[(_s)])
#define	local_float		local
#define	local_double		local_long

#define	slot_value(_s)		((_s)->v.tint)
#define	slot_fvalue(_s)		((_s)->v.tdouble)
#define	slot_insn(_s)		((_s)->insn)

#define	slot_alloctmp(t)	(t) = &tempinfo[tmpslot],		\
				tmpslot += 1
#define	slot_alloc2tmp(t)	(t) = &tempinfo[tmpslot],		\
				tmpslot += 2
#define	slot_freetmp(t)		slot_invalidate(t)
#define	slot_free2tmp(t)	slot_invalidate(t);			\
				slot_invalidate(t+1)
#define	slot_invalidate(_s)	(_s)->regno = NOREG;			\
				(_s)->modified = 0
#define	slot_nowriteback(_s)	_slot_nowriteback(_s)

#endif
