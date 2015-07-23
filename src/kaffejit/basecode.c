/*
 * basecode.c
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>
 */

#include "config.h"
#include "config-std.h"
#include "gtypes.h"
#include "slots.h"
#include "seq.h"
#include "registers.h"
#include "icode.h"
#include "basecode.h"
#include "md.h"
#include "flags.h"

#if 0
/* Modify references */
#define	INCREF(_s)	if ((_s) && (_s)->insn) {		\
				(_s)->insn->ref++;		\
			}
#if 0
#define	LINCREF(_s)	if (_s) {				\
				if ((_s)[0].insn) {		\
					(_s)[0].insn->ref++;	\
				}				\
				if ((_s)[1].insn) {		\
					(_s)[1].insn->ref++;	\
				}				\
			}
#else
#define LINCREF(_s)	INCREF(_s)
#endif
#define	SEQ_TYPE(T)	seq->type = (T)
#endif

#define	INCREF(S)
#define	LINCREF(S)
#define	SEQ_TYPE(T)

#define	ASSIGNSLOT(_d, _s)					\
			(_d).s.slot = (_s);			\
			(_d).s.seq = ((_s) ? (_s)->insn : 0)
#define	LASSIGNSLOT(_d, _s)					\
			ASSIGNSLOT(_d, _s)

extern void nop(sequence*);

void
_slot_const_const(SlotInfo* dst, jword s1, jword s2, ifunc f, int type)
{
	sequence* seq = nextSeq();

	seq->u[1].iconst = s1;
	seq->u[2].iconst = s2;
	ASSIGNSLOT(seq->u[0], dst);
	if (dst != 0) {
		dst->insn = seq;
	}

	SEQ_TYPE(type);
	seq->func = f;
}

void
_slot_slot_const(SlotInfo* dst, SlotInfo* s1, jword s2, ifunc f, int type)
{
	sequence* seq;
#if defined(TWO_OPERAND)
	SlotInfo* olddst = 0;

	/* Two operand systems cannot handle three operand ops.
	 * We need to fixit so the dst is one of the source ops.
	 */
	if (s1 != 0 && dst != 0) {
		if (s1 != dst && (type != Tload && type != Tstore)) {
			move_any(dst, s1);
			s1 = dst;
		}
	}
#endif
	seq = nextSeq();

	INCREF(s1);

	ASSIGNSLOT(seq->u[1], s1);
	seq->u[2].iconst = s2;
	ASSIGNSLOT(seq->u[0], dst);
	if (dst != 0) {
		dst->insn = seq;
	}

	SEQ_TYPE(type);
	seq->func = f;
}

void
_slot_slot_fconst(SlotInfo* dst, SlotInfo* s1, double s2, ifunc f, int type)
{
	sequence* seq = nextSeq();

	ASSIGNSLOT(seq->u[1], s1);
	seq->u[2].fconst = s2;
	ASSIGNSLOT(seq->u[0], dst);
	INCREF(s1);
	if (dst != 0) {
		dst->insn = seq;
	}

	SEQ_TYPE(type);
	seq->func = f;
}

void
_slot_slot_slot(SlotInfo* dst, SlotInfo* s1, SlotInfo* s2, ifunc f, int type)
{
	sequence* seq;
#if defined(TWO_OPERAND)
	SlotInfo* olddst = 0;

	/* Two operand systems cannot handle three operand ops.
	 * We need to fix it so the dst is one of the source ops.
	 */
	if (s1 != 0 && s2 != 0 && dst != 0) {
		if (s2 == dst) {
			olddst = dst;
			slot_alloctmp(dst);
		}
		if (s1 != dst) {
			move_any(dst, s1);
			s1 = dst;
		}
	}
#endif
	seq = nextSeq();

	INCREF(s1);
	INCREF(s2);

	ASSIGNSLOT(seq->u[1], s1);
	ASSIGNSLOT(seq->u[2], s2);
	ASSIGNSLOT(seq->u[0], dst);
	if (dst != 0) {
		dst->insn = seq;
	}

	SEQ_TYPE(type);
	seq->func = f;

#if defined(TWO_OPERAND)
	if (olddst != 0) {
		move_any(olddst, dst);
	}
#endif
}

void
_lslot_lslot_lslot(SlotInfo* dst, SlotInfo* s1, SlotInfo* s2, ifunc f, int type)
{
	sequence* seq;
#if defined(TWO_OPERAND)
	/* Two operand systems cannot handle three operand ops.
	 * We need to fixit so the dst is one of the source ops.
	 */
	SlotInfo* olddst = 0;
	if (s1 != 0 && s2 != 0 && dst != 0) {
		if (s2 == dst) {
			olddst = dst;
			slot_alloc2tmp(dst);
		}
		if (s1 != dst) {
			move_anylong(dst, s1);
			s1 = dst;
		}
	}
#endif
	seq = nextSeq();

	LASSIGNSLOT(seq->u[1], s1);
	LASSIGNSLOT(seq->u[2], s2);
	ASSIGNSLOT(seq->u[0], dst);
	LINCREF(s1);
	LINCREF(s2);
	if (dst != 0) {
		dst[0].insn = seq;
		dst[1].insn = 0;
	}

	SEQ_TYPE(type);
	seq->func = f;

#if defined(TWO_OPERAND)
	if (olddst != 0) {
		move_anylong(olddst, dst);
	}
#endif
}

void
_lslot_lslot_slot(SlotInfo* dst, SlotInfo* s1, SlotInfo* s2, ifunc f, int type)
{
	sequence* seq;
#if defined(TWO_OPERAND)
	/* Two operand systems cannot handle three operand ops.
	 * We need to fixit so the dst is one of the source ops.
	 */
	SlotInfo* olddst = 0;
	if (s1 != 0 && s2 != 0 && dst != 0) {
		if (s2 == dst) {
			olddst = dst;
			slot_alloctmp(dst);
		}
		if (s1 != dst) {
			move_any(dst, s1);
			s1 = dst;
		}
	}
#endif
	seq = nextSeq();

	LASSIGNSLOT(seq->u[1], s1);
	ASSIGNSLOT(seq->u[2], s2);
	ASSIGNSLOT(seq->u[0], dst);
	LINCREF(s1);
	INCREF(s2);
	if (dst != 0) {
		dst[0].insn = seq;
		dst[1].insn = 0;
	}

	SEQ_TYPE(type);
	seq->func = f;

#if defined(TWO_OPERAND)
	if (olddst != 0) {
		move_any(olddst, dst);
	}
#endif
}

void
_slot_slot_lslot(SlotInfo* dst, SlotInfo* s1, SlotInfo* s2, ifunc f, int type)
{
	sequence* seq;
#if defined(TWO_OPERAND)
	/* Two operand systems cannot handle three operand ops.
	 * We need to fixit so the dst is one of the source ops.
	 */
	SlotInfo* olddst = 0;
	if (s1 != 0 && s2 != 0 && dst != 0) {
		if (s2 == dst) {
			olddst = dst;
			slot_alloctmp(dst);
		}
		if (s1 != dst) {
			move_any(dst, s1);
			s1 = dst;
		}
	}
#endif
	seq = nextSeq();

	ASSIGNSLOT(seq->u[1], s1);
	LASSIGNSLOT(seq->u[2], s2);
	ASSIGNSLOT(seq->u[0], dst);
	INCREF(s1);
	LINCREF(s2);
	if (dst != 0) {
		dst->insn = seq;
	}

	SEQ_TYPE(type);
	seq->func = f;

#if defined(TWO_OPERAND)
	if (olddst != 0) {
		move_any(olddst, dst);
	}
#endif
}

void
_slot_lslot_lslot(SlotInfo* dst, SlotInfo* s1, SlotInfo* s2, ifunc f, int type)
{
	sequence* seq;

#if defined(TWO_OPERAND)
	ABORT();
#endif

	seq = nextSeq();

	LASSIGNSLOT(seq->u[1], s1);
	LASSIGNSLOT(seq->u[2], s2);
	ASSIGNSLOT(seq->u[0], dst);
	LINCREF(s1);
	LINCREF(s2);
	if (dst != 0) {
		dst->insn = seq;
	}

	SEQ_TYPE(type);
	seq->func = f;
}

void
_lslot_lslot_const(SlotInfo* dst, SlotInfo* s1, jword s2, ifunc f, int type)
{
	sequence* seq = nextSeq();

	LASSIGNSLOT(seq->u[1], s1);
	seq->u[2].iconst = s2;
	LASSIGNSLOT(seq->u[0], dst);
	LINCREF(s1);
	if (dst != 0) {
		dst[0].insn = seq;
		dst[1].insn = 0;
	}

	SEQ_TYPE(type);
	seq->func = f;
}

void
_lslot_lslot_lconst(SlotInfo* dst, SlotInfo* s1, jlong s2, ifunc f, int type)
{
	sequence* seq = nextSeq();

	LASSIGNSLOT(seq->u[1], s1);
	seq->u[2].lconst = s2;
	LASSIGNSLOT(seq->u[0], dst);
	LINCREF(s1);
	if (dst != 0) {
		dst[0].insn = seq;
		dst[1].insn = 0;
	}

	SEQ_TYPE(type);
	seq->func = f;
}

void
_lslot_slot_lconst(SlotInfo* dst, SlotInfo* s1, jlong s2, ifunc f, int type)
{
	sequence* seq = nextSeq();

	LASSIGNSLOT(seq->u[1], s1);
	seq->u[2].lconst = s2;
	LASSIGNSLOT(seq->u[0], dst);
	INCREF(s1);
	if (dst != 0) {
		dst[0].insn = seq;
		dst[1].insn = 0;
	}

	SEQ_TYPE(type);
	seq->func = f;
}

void
_lslot_slot_fconst(SlotInfo* dst, SlotInfo* s1, double s2, ifunc f, int type)
{
	sequence* seq = nextSeq();

	LASSIGNSLOT(seq->u[1], s1);
	seq->u[2].fconst = s2;
	LASSIGNSLOT(seq->u[0], dst);
	INCREF(s1);
	if (dst != 0) {
		dst[0].insn = seq;
		dst[1].insn = 0;
	}

	SEQ_TYPE(type);
	seq->func = f;
}

void
_slot_slot_slot_const_const(SlotInfo* dst, SlotInfo* s1, SlotInfo* s2,
			    jword s3, jword s4, ifunc f,
			    int type)
{
	sequence* seq = nextSeq();

	seq->u[4].iconst = s4;
	seq->u[3].iconst = s3;
	ASSIGNSLOT(seq->u[2], s2);
	ASSIGNSLOT(seq->u[1], s1);
	ASSIGNSLOT(seq->u[0], dst);
	INCREF(s1);
	INCREF(s2);
	if (dst != 0) {
		dst[0].insn = seq;
	}

	SEQ_TYPE(type);
	seq->func = f;
}

void
_slot_slot_const_const_const(SlotInfo* dst, SlotInfo* s1, jword s2,
			     jword s3, jword s4, ifunc f,
			     int type)
{
	sequence* seq = nextSeq();

	seq->u[4].iconst = s4;
	seq->u[3].iconst = s3;
	seq->u[2].iconst = s2;
	ASSIGNSLOT(seq->u[1], s1);
	ASSIGNSLOT(seq->u[0], dst);
	INCREF(s1);
	if (dst != 0) {
		dst[0].insn = seq;
	}

	SEQ_TYPE(type);
	seq->func = f;
}
