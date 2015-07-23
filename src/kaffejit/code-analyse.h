/* code-analyse.h
 * Analyse a method's bytecodes.
 *
 * Copyright (c) 1997 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 */

#ifndef __code_analyse_h
#define __code_analyse_h

typedef	struct Hjava_lang_Class*	frameElement;

#define	TUNASSIGNED		((frameElement)0)
#define	TUNSTABLE		((frameElement)1)
#define	TADDR			((frameElement)2)
#define	TOBJ			((frameElement)3)
#define	TVOID			(voidClass)
#define	TINT			(intClass)
#define	TLONG			(longClass)
#define	TFLOAT			(floatClass)
#define	TDOUBLE			(doubleClass)

#define	CONSTANTTYPE(VAL, LCL)					\
	switch (CLASS_CONST_TAG(meth->class, (LCL))) {		\
	case CONSTANT_Integer:					\
		VAL = TINT;					\
		break;						\
	case CONSTANT_Long:					\
		VAL = TLONG;					\
		break;						\
	case CONSTANT_Float:					\
		VAL = TFLOAT;					\
		break;						\
	case CONSTANT_Double:					\
		VAL = TDOUBLE;					\
		break;						\
	case CONSTANT_String:					\
	case CONSTANT_ResolvedString:				\
		VAL = TOBJ;					\
		break;						\
	default:						\
		VAL = TUNSTABLE;				\
		break;						\
	}

typedef struct perPCInfo {
	int16			flags;
	int16			stackPointer;
	uintp			pc;
	struct perPCInfo*	nextBB;
	frameElement*		frame;
} perPCInfo;

typedef struct codeinfo {
	int16			codelen;
	int16			framesz;
	perPCInfo		perPC[1];
} codeinfo;

#define	FLAG_STARTOFBASICBLOCK		0x0001
#define	FLAG_STARTOFEXCEPTION		0x0002
#define	FLAG_STACKPOINTERSET		0x0004
#define	FLAG_NORMALFLOW			0x0008
#define	FLAG_FLOW			0x0010
#define	FLAG_JUMP			0x0020
#define	FLAG_NEEDVERIFY			0x0040
#define	FLAG_DONEVERIFY			0x0080

#define	FLAGS(_pc)			codeInfo->perPC[_pc].flags
#define	STACKPOINTER(_pc)		codeInfo->perPC[_pc].stackPointer
#define	INSNPC(_pc)			codeInfo->perPC[_pc].pc
#define	FRAME(_pc)			codeInfo->perPC[_pc].frame
#define	INSN(pc)			meth->bcode[(pc)]
#define	INSNLEN(pc)			insnLen[INSN(pc)]
#define	BYTE(pc)			(int8)(meth->bcode[(pc)+0])
#define	WORD(pc)			(int16)( \
					 (meth->bcode[(pc)+0] << 8) + \
					 (meth->bcode[(pc)+1]))
#define	DWORD(pc)			(int32)( \
					 (meth->bcode[(pc)+0] << 24) + \
					 (meth->bcode[(pc)+1] << 16) + \
					 (meth->bcode[(pc)+2] << 8) + \
					 (meth->bcode[(pc)+3]))
#define	INCPC(V)			pc += (V)

#define	SET_STARTOFBASICBLOCK(PC)	ATTACH_NEW_BASICBLOCK(PC); \
					SET_NEWFRAME(PC); \
					FLAGS(PC) |= FLAG_STARTOFBASICBLOCK

#define	SET_STACKPOINTER(PC, SP)	STACKPOINTER(PC) = (SP); \
					FLAGS(PC) |= FLAG_STACKPOINTERSET

#define	SET_INSNPC(PC, V)		(codeInfo ? INSNPC(PC) = (V) : 0)
#define	SET_INSN(PC, V)
#define	SET_NORMALFLOW(pc)		FLAGS(pc) |= FLAG_NORMALFLOW
#define	SET_JUMPFLOW(from, to)		FLAGS(to) |= FLAG_FLOW; \
					FLAGS(from) |= FLAG_JUMP
#define	SET_STARTOFEXCEPTION(pc)	FLAGS(pc) |= FLAG_STARTOFEXCEPTION
#define	SET_NEEDVERIFY(pc)		FLAGS(pc) |= FLAG_NEEDVERIFY
#define	SET_DONEVERIFY(pc)		FLAGS(pc) &= ~FLAG_NEEDVERIFY; \
					FLAGS(pc) |= FLAG_DONEVERIFY

#define	IS_STARTOFBASICBLOCK(pc)	(FLAGS(pc) & FLAG_STARTOFBASICBLOCK)
#define	IS_STACKPOINTERSET(pc)		(FLAGS(pc) & FLAG_STACKPOINTERSET)
#define	IS_NORMALFLOW(pc)		(FLAGS(pc) & FLAG_NORMALFLOW)
#define	IS_STARTOFEXCEPTION(pc)		(FLAGS(pc) & FLAG_STARTOFEXCEPTION)
#define	IS_NEEDVERIFY(pc)		(FLAGS(pc) & FLAG_NEEDVERIFY)
#define	IS_DONEVERIFY(pc)		(FLAGS(pc) & FLAG_DONEVERIFY)

#define	ALLOCFRAME()			gc_malloc_fixed((codeInfo->framesz+1) * sizeof(frameElement))

#define	ATTACH_NEW_BASICBLOCK(DPC)				\
	if ((DPC) != 0 && !IS_STARTOFBASICBLOCK(DPC)) {		\
		btail->nextBB = &codeInfo->perPC[DPC];		\
		btail = btail->nextBB;				\
	}

#define	SET_NEWFRAME(pc)					\
	if (FRAME(pc) == 0) {					\
		FRAME(pc) = ALLOCFRAME();			\
		assert(FRAME(pc) != 0);				\
	}

#define	FRAMEMERGE(PC, SP)					\
	SET_STACKPOINTER(PC, SP);				\
	mergeFrame(PC, SP, activeFrame, meth);			\
        if (!IS_DONEVERIFY(PC)) {				\
                SET_NEEDVERIFY(PC);				\
        }

#define	FRAMEMERGE_LOCALS(PC)					\
	mergeFrame(PC, meth->localsz+meth->stacksz, activeFrame, meth); \
        if (!IS_DONEVERIFY(PC)) {				\
                SET_NEEDVERIFY(PC);				\
        }

#define	FRAMELOAD(_pc)						\
	{							\
		int m;						\
		for (m = 0; m < codeInfo->framesz; m++) {	\
			activeFrame[m] = FRAME(_pc)[m];		\
		}						\
	}

#define	STKPUSH(l)			sp -= (l)
#define	STKPOP(l)			sp += (l)

#define	STACKINIT(s, i)			FRAME(pc)[sp+s] = (i)
#define	STACKIN(s, i)			if ((i) != activeFrame[sp+s]) { \
						failed = true; \
						VDBG( printf("pc %d: stk %d (is %d, want %d)\n", pc, sp+s, (int) activeFrame[sp+s], (int) i); ) \
					}
#define	STACKOUT(s, i)			activeFrame[sp+s] = (i)
#define	STACKOUT_LOCAL(s, i, l)		activeFrame[sp+s] = activeFrame[l]
#define	STACKOUT_CONST(s, i, v)		activeFrame[sp+s] = (i)
#define	STACKCOPY(f, t)			activeFrame[sp+t] = activeFrame[sp+f]
#define	STACKSWAP(f, t)			{				\
					frameElement tmp;		\
					tmp = activeFrame[sp+t];	\
					activeFrame[sp+t] = activeFrame[sp+f]; \
					activeFrame[sp+f] = tmp;	\
					}
#define	LOCALINIT(l, i);		FRAME(pc)[l] = (i)
#define	LOCALIN(l, i)			if ((i) != activeFrame[l]) { \
						failed = true; \
						VDBG( printf("pc %d: lcl %d (is %d, want %d)\n", pc, l, (int) activeFrame[l], (int) i); ) \
					}
#define	LOCALOUT(l, i)			activeFrame[l] = (i)
#define LOCALOUT_STACK(l, i, s)		activeFrame[l] = activeFrame[sp+s]

struct _methods;
void verifyMethod(struct _methods*);
void tidyVerifyMethod(void);

extern codeinfo* codeInfo;
extern const uint8 insnLen[];

#endif
